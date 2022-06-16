#include "httphandler.h"
#include "WebServer.h"



void httphandler::format_size(char *buf, struct stat *stat)
{
    if (S_ISDIR(stat->st_mode))
    {
        sprintf(buf, "%s", "[DIR]");
    }
    else
    {
        off_t size = stat->st_size;
        if (size < 1024)
        {
            sprintf(buf, "%luB", size);
        }
        else if (size < 1024 * 1024)
        {
            sprintf(buf, "%.1fK", (double)size / 1024);
        }
        else if (size < 1024 * 1024 * 1024)
        {
            sprintf(buf, "%.1fM", (double)size / 1024 / 1024);
        }
        else
        {
            sprintf(buf, "%.1fG", (double)size / 1024 / 1024 / 1024);
        }
    }
}

int httphandler::check_uri(request_t *request, response_t *response)
{
    sprintf(request->path, ".%s", request->uri);

    struct stat filestat;
    if(stat(request->path, &filestat)==-1)
    {
        response->status_code = 404;
        return 1;
    }

    response->status_code = 200;
    // 文件下载
    if(!S_ISDIR(filestat.st_mode))
    {
        response->content_length = filestat.st_size;
        request->uri_dir = 0;
    }else{
        response->content = (char *)malloc(BUFSIZE);
        memset(response->content, 0, BUFSIZE);

        request->uri_dir = 1;
        strcat(response->content, "<html><head><style>"
                                "body{font-family: monospace; font-size: 13px;}"
                                "td {padding: 1.5px 6px;}"
                                "</style></head><body><table>\n");
        char m_time[32], size[16], content_buf[MAXLINE];
        
        DIR *d = opendir(request->path);
        if(d==NULL)
        {
            perror("can not open dir");
            // close(path_fd);
            response->status_code = 500;
            return 1;
        }
        struct dirent entry;

        struct dirent *dp;
        int ffd;
        
        int ret_code;
        for(ret_code=readdir_r(d, &entry, &dp); dp!=NULL && ret_code==0;ret_code=readdir_r(d, &entry, &dp))
        {
            if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) continue;
            char filepath[255];

            sprintf(filepath, "%s%s", request->path, entry.d_name);

            if((ffd=open(filepath, O_RDONLY, 0))==-1)
            {
                perror(dp->d_name);
                continue;
            }
            fstat(ffd, &filestat);
            strftime(m_time, sizeof(m_time), "%Y-%m-%d %H:%M", localtime(&filestat.st_mtime));
            format_size(size, &filestat);
            if(S_ISREG(filestat.st_mode) || S_ISDIR(filestat.st_mode))
            {
                const char *d = S_ISDIR(filestat.st_mode) ? "/" : "";
                sprintf(content_buf, "<tr><td><a href=\"%s%s\">%s%s</a></td><td>%s</td><td>%s</td></tr>\n", dp->d_name, d, dp->d_name, d, m_time, size);
                strcat(response->content, content_buf);

            }
            close(ffd);
        }
        closedir(d);
        strcat(response->content, "</table></body></html>");
        response->content_length = strlen(response->content);
    }
    
    return 1;
}


void httphandler::noblocksleep(int us)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = us;
    select(0, NULL, NULL, NULL, &tv);
}

PROCESS httphandler::rio_read(rio_t *rio)
{
    int read_size;
    char *p = rio->rio_buf_ptr;
    int count = 0;
    while(1)
    {   
        count ++;
        read_size=read(rio->rio_fd, p, sizeof(rio->rio_buf)-(p-rio->rio_buf));
        if(read_size<=0)
        {
            if(read_size==0)
            {
                return PROCESS_ERR;
            }
            else if(errno==EAGAIN)
            {
                
                noblocksleep(count*100);
                if(count==100) {
                    printf("try to read %d times\n", count);
                    return PROCESS_ERR;
                }
                continue;
            }
            return PROCESS_ERR;
        }else{
            while(*p!='\n' && *p!=0) ++p;
            
            if(*p==0) continue;
            else break;
        }
    }
    return PROCESS_PARSE_HEADERS;
}


void httphandler::rio_init(int fd, rio_t *rio)
{
    rio->rio_buf_ptr = rio->rio_buf;
    rio->rio_cnt = 0;
    rio->rio_fd = fd;
    memset(rio->rio_buf, 0, BUFSIZE);
}

int httphandler::parse_requestline(rio_t *rio, request_t *request)
{
    
    if(strncasecmp(rio->rio_buf_ptr, "GET", 3)==0)
    {
        request->method = GET;
    }else if(strncasecmp(rio->rio_buf_ptr, "POST", 4)==0)
    {
        request->method = POST;
    }else{
        return 1;
    }

    while(*rio->rio_buf_ptr++!=' ');

    char *p = rio->rio_buf_ptr;
    while(*p++!=' ');
    *(p-1) = 0;
    int sizeofpath = strlen(rio->rio_buf_ptr);
    request->uri = (char *)malloc(sizeofpath+1);
    
    strcpy(request->uri, rio->rio_buf_ptr);
    rio->rio_buf_ptr = p;
    
    rio->rio_buf_ptr = strpbrk(rio->rio_buf_ptr, "\r\n");
    rio->rio_buf_ptr += 2;
    return 3;
}

PROCESS httphandler::parse_requestheader(rio_t *rio, request_t *request)
{
    char *p;
    while(1)
    {
        if(*rio->rio_buf_ptr=='\r') return PROCESS_PARSE_BODY;
        else if(strncasecmp(rio->rio_buf_ptr, "GET", 3)==0 || strncasecmp(rio->rio_buf_ptr, "POST", 4)==0)
        {
            if(strncasecmp(rio->rio_buf_ptr, "GET", 3)==0) request->method = GET;
            else if(strncasecmp(rio->rio_buf_ptr, "POST", 4)==0) request->method = POST;
            else return PROCESS_ERR;
        }
        else if(strncasecmp(rio->rio_buf_ptr, "Connection:", 11)==0)
        {
            rio->rio_buf_ptr += 11;
            while(*rio->rio_buf_ptr==' ') rio->rio_buf_ptr++;
            if(strncasecmp(rio->rio_buf_ptr, "keep-alive", 10)==0) request->keep_alive = 1;
            else request->keep_alive = 0;
        }else if(strncasecmp(rio->rio_buf_ptr, "Content-Length", 15)==0)
        {
            rio->rio_buf_ptr += 15;
            while(*rio->rio_buf_ptr==' ') rio->rio_buf_ptr++;
            p = strpbrk(rio->rio_buf_ptr, "\r\n");
            if(p==NULL) return PROCESS_ERR;
            *p = 0;
            request->content_length = atoi(rio->rio_buf_ptr);
            *p = '\r';
        }
        p = strpbrk(rio->rio_buf_ptr, "\r\n");
        if(p==NULL) return PROCESS_READ;
        rio->rio_buf_ptr = p + 2;
    }
}

int httphandler::builder(request_t *request, response_t *response, rio_t *rio)
{
    char header_line[MAXLINE];
    memset(header_line, 0, MAXLINE);

    if(response->status_code==200)
    {
        sprintf(header_line, "HTTP/1.1 %d %s\r\n", response->status_code, "OK");
    }else if(response->status_code==404)
    {
        sprintf(header_line, "HTTP/1.1 %d %s\r\n", response->status_code, "FILE NOT FOUND!");
    }else if(response->status_code==500)
    {
        sprintf(header_line, "HTTP/1.1 %d %s\r\n", response->status_code, "bad request");
    }
    strcat(rio->rio_buf, header_line);

    sprintf(header_line, "Content-Length: %d\r\n", response->content_length);
    strcat(rio->rio_buf, header_line);

    if(request->keep_alive)
    {
        sprintf(header_line, "Connection: Keep-Alive\r\n");
    }else{
        sprintf(header_line, "Connection: close\r\n");
    }
    strcat(rio->rio_buf, header_line);

    if(request->uri_dir)
    {
        sprintf(header_line, "Content-Type: text/html\r\n");
    }else{
        sprintf(header_line, "Content-Type: application/octet-stream\r\n");
    }
    strcat(rio->rio_buf, header_line);

    strcat(rio->rio_buf, "\r\n");
}


int httphandler::write_all(request_t *request, response_t *response, rio_t *rio)
{
    rio->rio_cnt = strlen(rio->rio_buf);
    rio->rio_buf_ptr = rio->rio_buf;
    int writed_size;
    while(rio->rio_cnt>0)
    {
        if((writed_size=write(rio->rio_fd, rio->rio_buf_ptr, rio->rio_cnt))<0)
        {
            if(errno==EAGAIN)
            {
                continue;
            }
            return 0;
        }
        rio->rio_cnt -= writed_size;
        rio->rio_buf_ptr += writed_size;
    }

    int cnt=0;
    if(request->uri_dir)
    {
        while(response->content_length>0)
        {
            if((writed_size=write(rio->rio_fd, response->content+cnt, response->content_length))<0)
            {
                if(errno==EAGAIN)
                {
                    continue;
                }
                return 0;
            }
            cnt += writed_size;

            response->content_length -= writed_size;
        }
    }else{
        int path_fd = open(request->path, O_RDONLY, 0);

        char *src_addr = (char *)mmap(NULL, response->content_length, PROT_READ, MAP_PRIVATE, path_fd, 0);

        while(response->content_length>0)
        {
            if((writed_size=write(rio->rio_fd, src_addr+cnt, response->content_length))<0)
            {
                if(errno==EAGAIN)
                {
                    continue;
                }
                break;
            }
            cnt += writed_size;
            response->content_length -= writed_size;
        }

        munmap(src_addr, response->content_length);
        close(path_fd);
    }

    return 1;
}


int httphandler::process_request(int sockcli)
{
    rio_t rio;

    request_t request;
    request.keep_alive = 0;
    request.uri = NULL;
    response_t response;
    response.content = NULL;

    rio_init(sockcli, &rio);

    PROCESS process_state = PROCESS_READ;

    int requestlineok = 0;
    while(process_state!=PROCESS_END)
    {   
        switch (process_state)
        {
        // 读
        case PROCESS_READ:
            process_state = rio_read(&rio);
            break;
        // 解析
        case PROCESS_PARSE_HEADERS:
        {
            if(!requestlineok) {
                parse_requestline(&rio, &request);
                requestlineok = 1;
            }
            else process_state = parse_requestheader(&rio, &request);
            break;
        }

        // 解析body
        case PROCESS_PARSE_BODY:
            process_state = parse_requestcontent(&rio, &request);
            break;
        case PROCESS_ERR:
        {
            response.status_code = 500;
            response.content_length = 0;
            process_state = PROCESS_END;
            goto res;
        }
            break;
        default:
            break;
        }
    }

    check_uri(&request, &response);

res:
    rio_init(sockcli, &rio);

    builder(&request, &response, &rio);

    write_all(&request, &response, &rio);

    resource_free(&request, &response);

    return request.keep_alive;
}


void *httphandler::sockaccepttask(void *args)
{
    int sockcli;
    httphandler_args_t *http_h_args = (httphandler_args_t*)args;
    int epollfd = http_h_args->epollfd;
    int socksev = http_h_args->sockcli;
    timer *tim = (timer*)http_h_args->tim;
    timer_task_t *timer_task = (timer_task_t*)http_h_args->timer_task;
    struct sockaddr_in addrcli;
    socklen_t addrlen = sizeof(addrcli);
    struct epoll_event ev;
    while( (sockcli=accept(socksev, (struct sockaddr*)&addrcli, &addrlen))>0)
    {
        ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
        // ev.data.fd = sockcli;
        timer_task_t *timer_task = (timer_task_t*)malloc(sizeof(timer_task_t));
        timer_task->expire_time = timems()+30000;
        timer_task->fd = sockcli;
        ev.data.ptr = timer_task;
        tim->add_timer(timer_task);
        epoll_ctl(epollfd, EPOLL_CTL_ADD, sockcli, &ev);
        setnonblocking(sockcli);
        printf("socket %d connected\n", sockcli);
    }
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    // ev.data.fd = socksev;
    // timer_task->fd = socksev;
    ev.data.ptr = timer_task;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, socksev, &ev);
    printf("reset socksev %d\n", socksev);
    free(http_h_args);
}
void *httphandler::httphandlertask(void *args)
{
    httphandler_args_t *httphandler_args = (httphandler_args_t*)args;
    int epollfd = httphandler_args->epollfd;
    int connfd = httphandler_args->sockcli;
    timer *tim = (timer*)httphandler_args->tim;
    timer_task_t *timer_task = (timer_task_t *)httphandler_args->timer_task;
    httphandler handler(epollfd);
    int keep_alive = handler.process_request(connfd);


    printf("socket %d response over\n", connfd);
    if(keep_alive==0)
    {
        tim->del_timer(timer_task);
        close(connfd);
        epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, NULL);
        free(timer_task);
        printf("keep alive is false, socket %d disconnected\n", connfd);
    }else
    {
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
        // ev.data.fd = connfd;
        ev.data.ptr = timer_task;
        epoll_ctl(epollfd, EPOLL_CTL_MOD, connfd, &ev);
        timer_task->expire_time = timems()+30000;
        tim->adjust_timer(timer_task);
        printf("reset socket %d oneonshut\n", connfd);
    }

    free(httphandler_args);
    
}

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return new_option;
}