#include "timer.h"



timer::timer(/* args */)
{
    task_head = (timer_task_t*)malloc(sizeof(timer_task_t));
    task_head->pre = NULL;
    task_head->next = NULL;
    pthread_mutex_init(&mutex, NULL);
}

timer::~timer()
{
    free(task_head);
    pthread_mutex_destroy(&mutex);
}


void timer::add_timer(timer_task_t *task)
{
    timer_task_t *task_p = task_head;
    pthread_mutex_lock(&mutex);
    printf("lock\n");
    while(task_p->next && task_p->next->expire_time <= task->expire_time ) task_p = task_p->next;
    task->next = task_p->next;
    task_p->next = task;
    task->pre = task_p;
    if(task->next)
    {
        task->next->pre = task;
    }
    printf("unlock\n");
    pthread_mutex_unlock(&mutex);
}

void timer::del_timer(timer_task_t *task)
{
    pthread_mutex_lock(&mutex);
    printf("lock\n");
    task->pre->next = task->next;
    if(task->next) task->next->pre = task->pre;
    printf("unlock\n");
    pthread_mutex_unlock(&mutex);
}

void timer::handler()
{
    timer_task_t *task_p = task_head->next;
    while (task_p && task_p->expire_time<timems())
    {
        task_p->func(task_p->args);
        task_p->pre->next = task_p->next;
        if(task_p->next) task_p->next->pre = task_p->pre;
        task_p = task_p->next;
    }
}

void timer::adjust_timer(timer_task_t *task)
{
    if(!task->next) return;
    pthread_mutex_lock(&mutex);
    printf("lock\n");
    task->pre->next = task->next;
    task->next->pre = task->pre;

    // add_timer(task);
    timer_task_t *task_p = task;
    while(task_p->next && task_p->next->expire_time <= task->expire_time ) task_p = task_p->next;
    
    task->next = task_p->next;
    task_p->next = task;
    task->pre = task_p;
    if(task->next)
    {
        task->next->pre = task;
    }
    printf("unlock\n");
    pthread_mutex_unlock(&mutex);
}

void timer::loop()
{
    for(;;)
    {
        handler();
        struct timeval timev;
        timev.tv_sec = 0;
        timev.tv_usec = 1000;
        select(0, NULL, NULL, NULL, &timev);
    }
}


timer_task_t *timer::pop_expired_task()
{
    timer_task_t *task_p = task_head->next;
    if(task_p==NULL) return NULL;
    if(task_p->expire_time>timems()) return NULL;
    else del_timer(task_p);
    return task_p;
}

timems_t timer::find_nearst_wait_time()
{
    if(!task_head->next) return -1;

    timems_t t = task_head->next->expire_time - timems();
    return t>0 ? t:0;
}
