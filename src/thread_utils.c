#include "thread_utils.h"
#include <errno.h>

int lock(pthread_mutex_t *mtx){
	int err;
	if( ( err = pthread_mutex_lock(mtx)) != 0){
		errno = err;
		return -1;
	}
	return 0;
}

int unlock(pthread_mutex_t *mtx){
	int err;
	if( ( err = pthread_mutex_unlock(mtx)) != 0){
		errno = err;
		return -1;
	}
	return 0;
}

int cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx){
	int err;
	if( (err = pthread_cond_wait(cond, mtx)) != 0 ){
		errno = err;
		return -1;
	}
	return 0;
}

int cond_signal(pthread_cond_t *cond){
	int err;
	if( (err = pthread_cond_signal(cond)) != 0 ){
		errno = err;
		return -1;
	}
	return 0;
}
