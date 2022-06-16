#pragma
#define _GUN_SOURCE 1
#include <arpa/inet.h> /* inet_ntoa */
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/epoll.h>

#include "threadpool.h"
#include "httphandler.h"
#include "timer.h"

#define LISTENQ 1024 /* second argument to listen() */
#define EPOLLMAX 4096

typedef struct sockaddr SA;


typedef struct
{
    int epollfd;
    int fd;
    httphandler *handler;
} res_args;

typedef struct
{
    int epollfd;
    int listenfd;
} accept_args;

class WebServer
{
private:
    /* data */
    threadpool *tpool;
    timer tim;
public:
    WebServer(int port, char *host);
    
    ~WebServer();

    bool eventloop();
private:
    int open_listenfd(int port);

    bool init();
    int port;
    char *host;
    int listenfd;
};
