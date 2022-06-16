#include "threadpool.h"



threadpool::threadpool(int thread_num)
{
    this->thread_num = thread_num;
    init();
}

threadpool::~threadpool()
{
    shutdown();
}

void threadpool::init()
{
    pthread_cond_init(&cond, NULL);
    pthread_mutex_init(&mutex, NULL);
    pthreads = new pthread_t[thread_num];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    for(int i=0;i<thread_num;i++)
    {
        pthread_create(pthreads+i, &attr, worker, this);
    }
}

void *threadpool::worker(void *args)
{
    threadpool *tpool = (threadpool*)args;
    while(true)
    {
        pthread_mutex_lock(&tpool->mutex);
        while(tpool->taskqueue.empty()) pthread_cond_wait(&tpool->cond, &tpool->mutex);

        threadpool_task_t task = tpool->taskqueue.front();
        tpool->taskqueue.pop();

        pthread_mutex_unlock(&tpool->mutex);

        if(task.task_no==THREADPOOL_TASK_NO_EXIT) break;

        task.run(task.args);
    }
}


void threadpool::submit_task(threadpool_task_t task)
{
    pthread_mutex_lock(&mutex);
    taskqueue.push(task);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

void threadpool::thread_decrease(int num)
{
    threadpool_task_t task;
    task.task_no = THREADPOOL_TASK_NO_EXIT;
    for(int i=0;i<num;i++)
    {
        submit_task(task);
        --thread_num;
    }
}

void threadpool::shutdown()
{
    thread_decrease(thread_num);
    for(int i=0;i<thread_num;i++)
    {
        pthread_join(pthreads[i], NULL);
    }
    delete[] pthreads;
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
}