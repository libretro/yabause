/*  src/thr-rthreads.c: Thread functions for Libretro
    Copyright 2019 barbudreadmon

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "core.h"
#include "threads.h"
#include "rthreads/rthreads.h"
#include <stdlib.h>

struct thd_s {
   int running;
   sthread_t *thd;
   slock_t *mutex;
   scond_t *cond;
};

typedef struct YabEventQueue_rthreads
{
	int *buffer;
	int capacity;
	int size;
	int in;
	int out;
	slock_t *mutex;
	scond_t *cond_full;
	scond_t *cond_empty;
} YabEventQueue_rthreads;

typedef struct YabMutex_rthreads
{
  slock_t *mutex;
} YabMutex_rthreads;

static struct thd_s thread_handle[YAB_NUM_THREADS];

//////////////////////////////////////////////////////////////////////////////

int YabThreadStart(unsigned int id, void (*func)(void *), void *arg)
{
	if ((thread_handle[id].thd = sthread_create((void *)func, arg)) == NULL)
	{
		perror("CreateThread");
		return -1;
	}
	if ((thread_handle[id].mutex = slock_new()) == NULL)
	{
		perror("CreateThread");
		return -1;
	}
	if ((thread_handle[id].cond = scond_new()) == NULL)
	{
		perror("CreateThread");
		return -1;
	}

	thread_handle[id].running = 1;

	return 0; 
}

void YabThreadWait(unsigned int id)
{
	if (!thread_handle[id].thd)
		return;  // Thread wasn't running in the first place

	sthread_join(thread_handle[id].thd);
	thread_handle[id].thd = NULL;
	thread_handle[id].running = 0;
}

void YabThreadYield(void) {}

void YabThreadSleep(void) {}

void YabThreadRemoteSleep(unsigned int id) {}

void YabThreadWake(unsigned int id)
{
	if (!thread_handle[id].thd)
		return;  // Thread wasn't running in the first place

	scond_signal(thread_handle[id].cond);
}

void YabAddEventQueue( YabEventQueue * queue_t, int evcode )
{
	YabEventQueue_rthreads * queue = (YabEventQueue_rthreads*)queue_t;
	slock_lock(queue->mutex);
	while (queue->size == queue->capacity)
		scond_wait(queue->cond_full, queue->mutex);
	queue->buffer[queue->in] = evcode;
	++ queue->size;
	++ queue->in;
	queue->in %= queue->capacity;
	slock_unlock(queue->mutex);
	scond_broadcast(queue->cond_empty);
}

void YabThreadUSleep( unsigned int stime ) {}

void YabThreadSetCurrentThreadAffinityMask(int mask) {}

int YabWaitEventQueue( YabEventQueue * queue_t )
{
	int value;
	YabEventQueue_rthreads * queue = (YabEventQueue_rthreads*)queue_t;
	slock_lock(queue->mutex);
	while (queue->size == 0)
		scond_wait(queue->cond_empty, queue->mutex);
	value = queue->buffer[queue->out];
	-- queue->size;
	++ queue->out;
	queue->out %= queue->capacity;
	slock_unlock(queue->mutex);
	scond_broadcast(queue->cond_full);
	return value;
}

YabEventQueue * YabThreadCreateQueue( int qsize )
{
	YabEventQueue_rthreads * p = (YabEventQueue_rthreads*)malloc(sizeof(YabEventQueue_rthreads));
	p->buffer = (int*)malloc(sizeof(int)* qsize);
	p->capacity = qsize;
	p->size = 0;
	p->in = 0;
	p->out = 0;
	p->mutex = slock_new();
	p->cond_full = scond_new();
	p->cond_empty = scond_new();
	return (YabEventQueue *)p;
}

void YabThreadLock( YabMutex * mtx )
{
	YabMutex_rthreads * pmtx;
	pmtx = (YabMutex_rthreads *)mtx;
	slock_lock(pmtx->mutex);
}

void YabThreadUnLock( YabMutex * mtx )
{
	YabMutex_rthreads * pmtx;
	pmtx = (YabMutex_rthreads *)mtx;
	slock_unlock(pmtx->mutex);
}

void YabThreadFreeMutex( YabMutex * mtx )
{
	if( mtx != NULL ){
		YabMutex_rthreads * pmtx;
		pmtx = (YabMutex_rthreads *)mtx;        
		slock_free(pmtx->mutex);
		free(pmtx);
	}
}

YabMutex * YabThreadCreateMutex()
{
	YabMutex_rthreads * mtx = (YabMutex_rthreads *)malloc(sizeof(YabMutex_rthreads));
	mtx->mutex = slock_new();
	return (YabMutex *)mtx;
}

//////////////////////////////////////////////////////////////////////////////
