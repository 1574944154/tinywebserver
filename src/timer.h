#pragma once

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "utils.h"


typedef struct timer_task_s timer_task_t;

struct timer_task_s
{
    int fd;
    void *args;
    void *(*func)(void *);
    timems_t expire_time;
    timer_task_t *pre;
    timer_task_t *next;
};

class timer
{
private:
    timer_task_t *task_head;
    pthread_mutex_t mutex;
public:
    timer(/* args */);
    ~timer();
    void add_timer(timer_task_t *task);
    void del_timer(timer_task_t *task);
    void adjust_timer(timer_task_t *task);
    timer_task_t *pop_expired_task();
    timems_t find_nearst_wait_time();
    void loop();
    void handler();
};

