/*
 * =====================================================================================
 *       Copyright (c), 2013-2020, Goke C&S.
 *       Filename:  delaytask.c
 *
 *    Description:  
 *         Others:
 *
 *        Version:  1.0
 *        Date:  Tuesday, November 11, 2014 02:23:56 HKT
 *       Revision:  none
 *       Compiler:  arm-gcc
 *
 *         Author:  Sean houwentaoff@gmail.com
 *   Organization:  Goke
 *
 * =====================================================================================
 */

#include "delaytask.h"

static struct Timeval timeVal;

DelayQueue fDelayQueue;
char watchVariable_flag=0;
static const int MILLION = 1000000;

static void handleTimeout(struct AlarmHandler* fAlarmHandler)
{
	(*fAlarmHandler->fProc)(fAlarmHandler->fClientData);
	free(fAlarmHandler);
	//fAlarmHandler->fDelayQueueEntry.handleTimeout();
}
static void DelayInterval_init(DelayInterval * fDelayInterval, long seconds, long useconds)
{
	fDelayInterval->fTv.tv_sec = seconds;
	fDelayInterval->fTv.tv_usec = useconds;
}

static void EventTime__init__(struct EventTime* fEventTime, unsigned secondsSinceEpoch ,
	    unsigned usecondsSinceEpoch )
{
	fEventTime->fTv.tv_sec = secondsSinceEpoch;
	fEventTime->fTv.tv_usec = usecondsSinceEpoch;
}
static void EventTime_init(struct EventTime* fEventTime)
{
	fEventTime->init = EventTime__init__;
}


static bool timeGe(struct timeval *timeArg1, struct timeval *timeArg2)//arg1>=arg2
{
	return (((long) timeArg1->tv_sec > (long) (timeArg2->tv_sec))
			|| ((long) timeArg1->tv_sec == (long) timeArg2->tv_sec)
			&& ((long) timeArg1->tv_usec == (long) timeArg2->tv_usec));
}

static bool timeLe(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 <= arg2
{
    return timeGe(timeArg2, timeArg1);
}
static bool timeLt(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 <  arg2
{
    return (!timeGe(timeArg1, timeArg2));
}
static bool timeEq(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 == arg2
{
    return (timeGe(timeArg1, timeArg2) && timeGe(timeArg2, timeArg1));
}

static bool timeNe(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 != arg2
{
    return (!timeEq(timeArg1, timeArg2));
}
static void timeSub(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 = arg1 - arg2
{
	timeArg1->tv_sec -= timeArg2->tv_sec;
	timeArg1->tv_usec -= timeArg2->tv_usec;
	//溢出为负数需要处理
}

static void timeAdd(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 = arg1 + arg2
{
	timeArg1->tv_sec += timeArg2->tv_sec;
	timeArg1->tv_usec += timeArg2->tv_usec;
	//溢出为负数需要处理
}
static void Timeval_init(Timeval *ftimeval)
{
    ftimeval->ge = timeGe;
    ftimeval->add = timeAdd;
    ftimeval->sub = timeSub;
    ftimeval->eq = timeEq;
    ftimeval->ne = timeNe;
    ftimeval->le = timeLe;
    ftimeval->lt = timeLt;
}

static void addEntry(struct DelayQueue* fDelayQueue, DelayQueueEntry* newEntry)
{
	fDelayQueue->synchronize(fDelayQueue);

	DelayQueueEntry* cur = fDelayQueue->head(fDelayQueue);
	while ((timeVal.ge)(&newEntry->fDeltaTimeRemaining.fTv, &cur->fDeltaTimeRemaining.fTv))
	{
		//newEntry->fDeltaTimeRemaining.fTv -= cur->fDeltaTimeRemaining.fTv;
		timeVal.sub(&newEntry->fDeltaTimeRemaining.fTv, &cur->fDeltaTimeRemaining.fTv);
		cur = cur->fNext;
	}

	//cur->fDeltaTimeRemaining.fTv -= newEntry->fDeltaTimeRemaining.fTv;
	timeVal.sub(&cur->fDeltaTimeRemaining.fTv, &newEntry->fDeltaTimeRemaining.fTv);
	//add it to queue Entry
	newEntry->fNext = cur;
	newEntry->fPrev = cur->fPrev;
	cur->fPrev = newEntry->fPrev->fNext = newEntry;
}

static void removeEntry(DelayQueueEntry* entry)
{
	if (entry == NULL || entry->fNext == NULL) return;//末节点

	//entry->fNext->fDeltaTimeRemaining.fTv += entry->fDeltaTimeRemaining.fTv;
	timeVal.add(&entry->fNext->fDeltaTimeRemaining.fTv, &entry->fDeltaTimeRemaining.fTv);
	entry->fPrev->fNext = entry->fNext;
	entry->fNext->fPrev = entry->fPrev;
	entry->fNext = entry->fPrev = NULL;
}
static DelayQueueEntry* head(struct DelayQueue* fDelayQueue)
{
	return fDelayQueue->fDelayQueueEntry.fNext;
}
static void synchronize(struct DelayQueue* fDelayQueue)
{
	//获取了现在的时间 timeNow
	EventTime timeNow;
	struct timeval tvNow;
	gettimeofday(&tvNow, NULL);
	EventTime_init(&timeNow);
	timeNow.init(&timeNow, tvNow.tv_sec, tvNow.tv_usec);

	//当前时间比上次更新的时间早:这是不正常的应该恢复(由于外界更改时间会导致此发生)
	if (timeVal.lt(&timeNow.fTv, &fDelayQueue->fLastSyncTime.fTv))
	{
		fDelayQueue->fLastSyncTime.fTv = timeNow.fTv;
		return;
	}

	DelayInterval timeSinceLastSync;
	//timeSinceLastSync.fTv = timeNow.fTv - fDelayQueue->fLastSyncTime.fTv;

	timeSinceLastSync.fTv = timeNow.fTv;
	timeVal.sub(&timeSinceLastSync.fTv, &fDelayQueue->fLastSyncTime.fTv);
	//		timeSinceLastSync -= fDelayQueue->fLastSyncTime.fTv;


	fDelayQueue->fLastSyncTime.fTv = timeNow.fTv;//同步时间

	//调整延时队列 根据时间差确定哪些事件应该被置0;或者减少 准备进行调度
	DelayQueueEntry* curEntry = fDelayQueue->head(fDelayQueue);
	while (timeVal.ge(&timeSinceLastSync.fTv, &curEntry->fDeltaTimeRemaining.fTv))
	{
		timeVal.sub (&timeSinceLastSync.fTv, &curEntry->fDeltaTimeRemaining.fTv);
		memset(&curEntry->fDeltaTimeRemaining.fTv, DELAY_ZERO, sizeof(struct timeval));// = (long)DELAY_ZERO;
		curEntry =curEntry->fNext;
	}
	timeVal.sub(&curEntry->fDeltaTimeRemaining.fTv, &timeSinceLastSync.fTv);
}

static void handleAlarm(DelayQueue *fDelayQueue) {
  struct timeval fTv;
#if 0
      printf("==>%s()head[0x%x]next[0x%x]pre[0x%x]\n", __func__, head(fDelayQueue), head(fDelayQueue)->fNext, head(fDelayQueue)->fPrev);
//      printf("==>%s()head[0x%x]\n", __func__, head(fDelayQueue));
#endif
  memset(&fTv, DELAY_ZERO, sizeof(struct timeval));
  if (timeVal.ne(&fDelayQueue->head(fDelayQueue)->fDeltaTimeRemaining.fTv , &fTv)) fDelayQueue->synchronize(fDelayQueue);

  if (timeVal.eq(&fDelayQueue->head(fDelayQueue)->fDeltaTimeRemaining.fTv, &fTv))
  {
    // This event is due to be handled:

    DelayQueueEntry* toRemove = head(fDelayQueue);
#if 0
      printf("head[0x%x]next[0x%x]pre[0x%x]\n", head(fDelayQueue), head(fDelayQueue)->fNext, head(fDelayQueue)->fPrev);
#endif

    fDelayQueue->removeEntry(toRemove); // do this first, in case handler accesses queue
    //获取派生类的alarm_handler 的指针.stu.name, struct student, name
    AlarmHandler *tmp = container_of(toRemove, struct AlarmHandler, fDelayQueueEntry);//)
    tmp->handleTimeout(tmp);
//    toRemove->handleTimeout(); //delete itself
  }
}

void DelayQueue__init__(DelayQueue *fDelayQueue)
{
    //永远到不了的时间
    fDelayQueue->fDelayQueueEntry.fDeltaTimeRemaining.fTv.tv_sec = INT_MAX; 
    fDelayQueue->fDelayQueueEntry.fDeltaTimeRemaining.fTv.tv_usec = MILLION-1; 
    fDelayQueue->fDelayQueueEntry.fNext = fDelayQueue->fDelayQueueEntry.fPrev = &fDelayQueue->fDelayQueueEntry;
}
void DelayQueue_init(DelayQueue *fDelayQueue)
{
	fDelayQueue->head = head;
	fDelayQueue->handleAlarm = handleAlarm;
	fDelayQueue->synchronize = synchronize;
	fDelayQueue->removeEntry = removeEntry;
	fDelayQueue->addEntry = addEntry;
	fDelayQueue->init = DelayQueue__init__;
}

void SingleStep(unsigned maxDelayTime)
{
	fDelayQueue.handleAlarm(&fDelayQueue);
	return;
}



void doEventLoop(char* watchVariable) {
  // Repeatedly loop, handling readble sockets and timed events:
  while (1) {
    if (watchVariable != NULL && *watchVariable != 0) break;
    SingleStep(0);
  }
}


void AlarmHandler__init__(AlarmHandler* fAlarmHandler, TaskFunc* fProc, void * fClientData, DelayInterval *timeToDelay)
{
	fAlarmHandler->fProc = fProc;
	fAlarmHandler->fClientData = fClientData;
	fAlarmHandler->fDelayQueueEntry.fDeltaTimeRemaining.fTv = timeToDelay->fTv;
}
void AlarmHandler_init(AlarmHandler *fAlarmHandler)
{
	fAlarmHandler->init = AlarmHandler__init__;
	fAlarmHandler->handleTimeout  = handleTimeout;
}

int token(AlarmHandler * fAlarmHandler)
{
	return fAlarmHandler->fToken;
}

TaskToken scheduleDelayedTask(__int64 microseconds,
		TaskFunc* proc,
		void* clientData)
{
	if (microseconds < 0)microseconds = 0;
	DelayInterval timeToDelay;
	DelayInterval_init(&timeToDelay, (long)(microseconds/1000000), (long)(microseconds%1000000));
	AlarmHandler *alarmHandler = (AlarmHandler *)malloc(sizeof(AlarmHandler));
    AlarmHandler_init(alarmHandler);
	alarmHandler->init(alarmHandler, proc, clientData, &timeToDelay);

	fDelayQueue.addEntry(&fDelayQueue, &alarmHandler->fDelayQueueEntry);

	return (void*) (alarmHandler->fToken);
}

void *delay_task_func(void *data)
{
    Timeval_init(&timeVal);
	DelayQueue_init(&fDelayQueue);
    fDelayQueue.init(&fDelayQueue);
//	scheduleDelayedTask(1000000, task_test, str);
	doEventLoop(&watchVariable_flag);
	return NULL;
}


