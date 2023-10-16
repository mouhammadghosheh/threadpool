#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "threadpool.h"

threadpool* create_threadpool(int num_threads_in_pool) {
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL) {
        return NULL;
    }

    threadpool *pool = (threadpool *)malloc(sizeof(threadpool));
    if (pool == NULL) {
        return NULL;
    }

    pool->num_threads = num_threads_in_pool;
    pool->qsize = 0;
    pool->qhead = pool->qtail = NULL;
    pool->shutdown = pool->dont_accept = 0;

    if (pthread_mutex_init(&pool->qlock, NULL) != 0) {
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->q_not_empty, NULL) != 0 ||
        pthread_cond_init(&pool->q_empty, NULL) != 0) {
        pthread_mutex_destroy(&pool->qlock);
        free(pool);
        return NULL;
    }

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads_in_pool);
    if (pool->threads == NULL) {
        pthread_mutex_destroy(&pool->qlock);
        pthread_cond_destroy(&pool->q_not_empty);
        pthread_cond_destroy(&pool->q_empty);
        free(pool);
        return NULL;
    }

    for (int i = 0; i < num_threads_in_pool; i++) {
        if (pthread_create(&pool->threads[i], NULL, do_work, (void *)pool) != 0) {
            pthread_mutex_destroy(&pool->qlock);
            pthread_cond_destroy(&pool->q_not_empty);
            pthread_cond_destroy(&pool->q_empty);
            free(pool->threads);
            free(pool);
            return NULL;
        }
    }

    return pool;
}


void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if (from_me == NULL || dispatch_to_here == NULL) {
        return;
    }

    if (from_me->dont_accept) {
        return;
    }

    work_t* work = (work_t*) malloc(sizeof(work_t));
    if(work == NULL)
        return;
    work->arg = arg;
    work->routine = dispatch_to_here;
    work->next = NULL;
    pthread_mutex_lock(&from_me->qlock);
    if (from_me->qsize == 0) {
        from_me->qhead = from_me->qtail = work;
    } else {
        from_me ->qtail->next = work;
        from_me->qtail = work;
    }
    from_me->qsize++;
    pthread_mutex_unlock(&from_me->qlock);
    pthread_cond_signal(&from_me->q_not_empty);
}

void* do_work(void* p){

        threadpool *pool = (threadpool *)p;

        while (1) {
            pthread_mutex_lock(&pool->qlock);
            while (pool->qsize == 0 && !pool->shutdown) {
                pthread_cond_wait(&pool->q_not_empty, &pool->qlock);
            }

            if (pool->shutdown) {
                pthread_mutex_unlock(&pool->qlock);
                pthread_exit(NULL);
            }

            work_t *work = pool->qhead;
            pool->qsize--;
            if (pool->qsize == 0) {
                pool->qhead = pool->qtail = NULL;
                pthread_cond_signal(&pool->q_empty);
            } else {
                pool->qhead = work->next;
            }
            pthread_mutex_unlock(&pool->qlock);

            (work->routine)(work->arg);
            free(work);
        }
    }

void destroy_threadpool(threadpool* destroyme) {
    if (destroyme == NULL) {
        return;
    }

    pthread_mutex_lock(&destroyme->qlock);
    destroyme->dont_accept = 1;
    while (destroyme->qsize != 0) {
        pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
    }
    destroyme->shutdown = 1;
    pthread_mutex_unlock(&destroyme->qlock);
    pthread_cond_broadcast(&destroyme->q_not_empty);

    for (int i = 0; i < destroyme->num_threads; i++) {
        pthread_join(destroyme->threads[i], NULL);
    }

    pthread_mutex_destroy(&destroyme->qlock);
    pthread_cond_destroy(&destroyme->q_not_empty);
    pthread_cond_destroy(&destroyme->q_empty);
    free(destroyme->threads);
    free(destroyme);
}
