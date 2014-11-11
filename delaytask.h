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

#define false (0)
#define true (!false)

#define DELAY_ZERO  0

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/**
 * container_of - cast a member of a structure out to the containing structure
 *
 * @ptr:	the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({		      \
	const typeof(((type *)0)->member)*__mptr = (ptr);    \
	(type *)((char *)__mptr - offsetof(type, member)); })

typedef int bool;

typedef void* TaskToken;
typedef void TaskFunc(void* clientData);
typedef long long __int64;


typedef struct DelayInterval
{
//	void (*init)(long seconds, long useconds);
	struct timeval fTv;
	int tokenCounter;
}DelayInterval;


typedef struct EventTime
{
	void (*init)(struct EventTime* fEventTime, unsigned secondsSinceEpoch,
		    unsigned usecondsSinceEpoch);
	struct timeval fTv;
}EventTime;

typedef struct DelayQueueEntry
{
	struct DelayQueueEntry* fNext;
	struct DelayQueueEntry* fPrev;
	DelayInterval fDeltaTimeRemaining;

}DelayQueueEntry;

typedef struct DelayQueue
{
	DelayQueueEntry fDelayQueueEntry;
	EventTime fLastSyncTime;
	void (*init)(struct DelayQueue *fDelayQueue);
	void (*handleAlarm)(struct DelayQueue *fDelayQueue);
	void (*addEntry)(struct DelayQueue* fDelayQueue, DelayQueueEntry* newEntry); // returns a token for the entry
	void (*updateEntry)(DelayQueueEntry* entry, DelayInterval newDelay);
	void (*removeEntry)(DelayQueueEntry* entry); // but doesn't delete it

	DelayQueueEntry*(* head)(struct DelayQueue* fDelayQueue);
	void (*synchronize)(struct DelayQueue* fDelayQueue);
}DelayQueue;

typedef struct AlarmHandler
{
	DelayQueueEntry fDelayQueueEntry;
	TaskFunc* fProc;
	void * fClientData;
	void (*init)(struct AlarmHandler* fAlarmHandler, TaskFunc* fProc, void * fClientData, DelayInterval *fDelayInterval);
	void (*handleTimeout)(struct AlarmHandler* fAlarmHandler);
	int fToken;
}AlarmHandler;

typedef struct Timeval
{
	//>= += -= - ,> (!<) <(! >=) ==(arg1 >= arg2&& arg1 >= arg2) !=
    bool (*ge)(struct timeval* arg1, struct timeval* arg2);//>=
    void (*add)(struct timeval* arg1, struct timeval* arg2);//+=
    void (*sub)(struct timeval* arg1, struct timeval* arg2);//-=
    bool (*eq)(struct timeval* arg1, struct timeval* arg2);
    bool (*ne)(struct timeval* arg1, struct timeval* arg2);
    bool (*le)(struct timeval* arg1, struct timeval* arg2);//<=
    bool (*lt)(struct timeval* arg1, struct timeval* arg2);//<

	struct timeval fTv;
}Timeval;

void *delay_task_func(void *data);
TaskToken scheduleDelayedTask(__int64 microseconds,
		TaskFunc* proc,
		void* clientData);

#endif
