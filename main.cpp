#include <iostream>
#include <pthread.h>

#include "WebServer.h"
#include "threadpool.h"

#include "timer.h"
#include "utils.h"


void *timer_task1(void *args)
{
    printf("%d\n", (int)(long)args);
}

int main(int argc, char const *argv[])
{

    WebServer server(9999, "127.0.0.1");
    server.eventloop();


    return 0;
}
