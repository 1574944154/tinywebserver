#pragma once
#include <stdio.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/mman.h>

#include "timer.h"

#define BUFSIZE 10240
#define MAXLINE 1024


typedef struct httphandler_args
{
    int epollfd;
    int sockcli;
    timer *tim;
    timer_task_t *timer_task;
} httphandler_args_t;

typedef struct 
{
    int rio_fd;
    int rio_cnt;            // the length of rio_buf
    char *rio_buf_ptr;      // point unread pos
    char rio_buf[BUFSIZE];
} rio_t;
typedef enum 
{
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT,
    PATH
} REQUEST_METHOD;

typedef struct
{
    REQUEST_METHOD method;
    
    int keep_alive;
    int content_length;
    char *uri;
    int uri_dir;    // 请求的uri是目录还是文件
    char path[30];
} request_t;

typedef struct
{
    char *content;
    int content_length;
    int status_code;
    char *msg;
} response_t;

typedef enum
{
    PROCESS_READ = 0,
    PROCESS_PARSE_HEADERS,
    PROCESS_PARSE_BODY,
    PROCESS_END,
    PROCESS_ERR
} PROCESS;


int setnonblocking(int fd);

class httphandler
{
private:
    int epollfd;

    void format_size(char *buf, struct stat *stat);
   
    int check_uri(request_t *request, response_t *response);

    void noblocksleep(int us);
    
    PROCESS rio_read(rio_t *rio);

    void rio_init(int fd, rio_t *rio);

    int parse_requestline(rio_t *rio, request_t *request);

    PROCESS parse_requestheader(rio_t *rio, request_t *request);

    PROCESS parse_requestcontent(rio_t *rio, request_t *request)
    {
        if(request->method==GET)
            return PROCESS_END;
    }

    int builder(request_t *request, response_t *response, rio_t *rio);

    int write_all(request_t *request, response_t *response, rio_t *rio);

    int resource_free(request_t *request, response_t *response)
    {
        if(response->content!=NULL) free(response->content);
        if(request->uri!=NULL) free(request->uri);
    }

public:
    httphandler(int epollfd)
    {
        this->epollfd = epollfd;
    }
    
    ~httphandler()
    {

    }

    int process_request(int sockcli);

    static void *sockaccepttask(void *args);
    static void *httphandlertask(void *args);
};
