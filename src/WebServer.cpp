#include "WebServer.h"


WebServer::WebServer(int port, char *host)
{
    this->port = port;
    this->host = host;
    tpool = new threadpool(5);
}

WebServer::~WebServer()
{
    delete tpool;
}

bool WebServer::init()
{
    struct sockaddr_in socksev;
    int optval = 1;
    if((listenfd=socket(AF_INET, SOCK_STREAM, 0))<0)
    {
        perror("socket");
        return false;
    }
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))<0)
    {
        perror("setsockopt");
        return false;
    }
    memset(&socksev, 0, sizeof(socksev));
    socksev.sin_family = AF_INET;
    socksev.sin_addr.s_addr = inet_addr(host);
    socksev.sin_port = htons(port);
    if(bind(listenfd, (sockaddr *)&socksev, sizeof(socksev))<0)
    {
        perror("bind");
        return false;
    }

    if(listen(listenfd, LISTENQ)<0)
    {
        perror("listen");
        return false;
    }
    signal(SIGPIPE, SIG_IGN);
    return true;
}


bool WebServer::eventloop()
{
    if(!init())
    {
        fprintf(stderr, "web server init fail\n");
        return false;
    }

    sockaddr_in sockcli;
    socklen_t clientlen;

    int epoll_fd = epoll_create(1);

    setnonblocking(listenfd);
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    // ev.data.fd = listenfd;

    timer_task_t *timer_task = (timer_task_t*)malloc(sizeof(timer_task_t));
    timer_task->fd = listenfd;
    ev.data.ptr = timer_task;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listenfd, &ev);

    struct epoll_event events[EPOLLMAX];
    int count = 0;
    while(1)
    {
        timems_t wait_time = tim.find_nearst_wait_time();
        printf("wait time is %ld ms\n", wait_time);
        int fdns = epoll_wait(epoll_fd, events, EPOLLMAX, wait_time);
        printf("epoll_wait ret %d\n", fdns);
        if(fdns==0)
        {
            timer_task_t *t_task;
            while( (t_task=tim.pop_expired_task())!=NULL)
            {
                int fd = t_task->fd;
                close(fd);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                printf("socket %d timeout, disconnected\n", fd);
                free(t_task);
            }
        }

        int i, num = fdns;
        for(i=0;i<num;++i)
        {
            int connfd = ((timer_task_t*)events[i].data.ptr)->fd;
            if(connfd==listenfd)
            {   
                httphandler_args_t *args = (httphandler_args_t*)malloc(sizeof(httphandler_args_t));
                args->epollfd = epoll_fd;
                args->sockcli = listenfd;
                args->tim = &tim;
                args->timer_task = timer_task;
                threadpool_task_t task;
                task.args = args;
                task.run = httphandler::sockaccepttask;
                task.task_no = THREADPOOL_TASK_NO_RUN;
                tpool->submit_task(task);
            }else if(events[i].events & EPOLLRDHUP)
            {
                // struct epoll_event ev;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connfd, NULL);
                timer_task_t *timer_task = (timer_task_t*)events[i].data.ptr;
                tim.del_timer(timer_task);
                free(timer_task);
                close(connfd);
                printf("socket %d disconnected\n", connfd);
            }else if(events[i].events & EPOLLIN)
            {
                printf("socket %d event response, the %d request\n", connfd, count++);

                httphandler_args_t *args = (httphandler_args_t*)malloc(sizeof(httphandler_args_t));
                args->epollfd = epoll_fd;
                args->sockcli = connfd;
                args->tim = &tim;
                args->timer_task = (timer_task_t*)events[i].data.ptr;
                
                threadpool_task_t task;
                task.args = (void *)args;
                task.run = httphandler::httphandlertask;
                task.task_no = THREADPOOL_TASK_NO_RUN;
                tpool->submit_task(task);
            }else {
                fprintf(stderr, "unknown event\n");
            }
        }
    }
}

int WebServer::open_listenfd(int port)
{
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval, sizeof(int)) < 0)
        return -1;

    // 6 is TCP's protocol number
    // enable this, much faster : 4000 req/s -> 17000 req/s
    if (setsockopt(listenfd, 6, TCP_CORK,
                   (const void *)&optval, sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
}
