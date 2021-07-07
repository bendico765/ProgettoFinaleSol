#if !defined(_THREAD_UTILS_H)
#define _THREAD_UTILS_H
#include <pthread.h>

int lock(pthread_mutex_t *mtx);
int unlock(pthread_mutex_t *mtx);
int cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx);
int cond_signal(pthread_cond_t *cond);

#endif
