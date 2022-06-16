#pragma once
#include <pthread.h>
#include <queue>

using std::queue;

typedef enum 
{
    THREADPOOL_TASK_NO_EXIT = 0,
    THREADPOOL_TASK_NO_RUN
} THREADPOOL_TASK_NO;

typedef struct 
{
    THREADPOOL_TASK_NO task_no;
    void *args;
    void *(*run)(void *);
} threadpool_task_t;

class threadpool
{
private:
    /* data */
    int thread_num;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    pthread_t *pthreads;
    queue<threadpool_task_t> taskqueue;
public:
    threadpool(int thread_num);
    ~threadpool();
    void submit_task(threadpool_task_t task);
    void init();
    static void *worker(void *);

private:
    void thread_decrease(int num);
    void shutdown();
};

