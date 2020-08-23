/*
 * =====================================================================================
 *       Copyright (c), 2013-2020, Goke C&S.
 *       Filename:  delaytask.h
 *
 *    Description:  
 *         Others:
 *
 *        Version:  1.0
 *        Date:  Tuesday, November 11, 2014 02:16:04 HKT
 *       Revision:  none
 *       Compiler:  arm-gcc
 *
 *         Author:  Sean houwentaoff@gmail.com
 *   Organization:  Goke
 *
 * =====================================================================================
 */
#ifndef __DELAYTASK__H__
#define __DELAYTASK__H__

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>
#include <string.h>
#include <semaphore.h>

typedef void* TaskToken;
typedef void TaskFun(void* clientData);
typedef long long __int64;

void *delay_task_func(void *data);
TaskToken scheduleDelayedTask(__int64 microseconds,
		TaskFun* proc,
		void* clientData);
int schedule_task_is_empty();
void * schedule_timer(void *data);
TaskToken scheduleTimerTask(__int64 microseconds,
        TaskFun* proc,
        void* clientData);


#endif
