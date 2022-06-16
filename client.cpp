#define _GNU_SOURCE 1
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define EPOLLMAXFD 512

int main(int argc, char const *argv[])
{
    

    struct sockaddr_in socksev, sockcli;

    memset(&socksev, 0, sizeof(socksev));
    memset(&sockcli, 0, sizeof(sockcli));

    

    socksev.sin_family = AF_INET;
    socksev.sin_addr.s_addr = inet_addr("127.0.0.1");
    socksev.sin_port = htons(9999);


    int connfd;
    
    int epollfd = epoll_create(1);


    for(int i=0;i<512;i++)
    {

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        connfd = connect(sock, (sockaddr*)&socksev, sizeof(socksev));
        if(connfd<0)
        {
            perror("connect");
            continue;
        }
        struct epoll_event ev;
        ev.data.fd = sock;
        ev.events = EPOLLIN | EPOLLRDHUP;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev);
    }
    for(;;)
    {
        struct epoll_event events[EPOLLMAXFD];
        
        int fdns = epoll_wait(epollfd, events, EPOLLMAXFD, -1);
        for(int i=0;i<fdns;i++)
        {
            if(events[i].events & EPOLLRDHUP)
            {
                printf("socket %d disconnected\n", events[i].data.fd);
                epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
            }else if(events[i].events & EPOLLIN)
            {
                printf("socket %d EPOLLIN\n", events[i].data.fd);
            }
            else{
                printf("unknoown event, socket %d\n", events[i].data.fd);
            }
        }
    }
    return 0;
}
