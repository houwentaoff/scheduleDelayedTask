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

#include <unistd.h>
#include <stdbool.h>
#include "delaytask.h"

//#define DELAY_TASK_DEBUG

#ifdef DELAY_TASK_DEBUG
#define delaytask_msg(format, ...)  printf("[delay task]:" format, ##__VA_ARGS__)
#else
#define delaytask_msg(format, ...)
#endif

#define FIRST   0x1 

#define DELAY_ZERO  0

//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

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

typedef TaskFun TaskFunc;
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
	DelayInterval fDeltaTimeRemaining;//默认拥有一个节点:节点为 永远到不鸟的时间
	int flag;
}DelayQueueEntry;

typedef struct DelayQueue
{
	DelayQueueEntry fDelayQueueEntry;
	EventTime fLastSyncTime;//上次同步时间， 每次sync时会更新
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

static struct Timeval timeVal;

DelayQueue fDelayQueue={0};
char watchVariable_flag=0;
static const int MILLION = 1000000;
sem_t DelayedTask_sem;
static timer_t timer;
static struct itimerspec ts;

static void handleTimeout(struct AlarmHandler* fAlarmHandler)
{
    delaytask_msg("==>%s\n", __func__);
    (*fAlarmHandler->fProc)(fAlarmHandler->fClientData);
    free(fAlarmHandler);
    //fAlarmHandler->fDelayQueueEntry.handleTimeout();
    delaytask_msg("<==%s\n", __func__);
}
static void DelayInterval_init(DelayInterval * fDelayInterval, long seconds, long useconds)
{
    fDelayInterval->fTv.tv_sec = seconds;
    fDelayInterval->fTv.tv_usec = useconds;
}
static void do_EventTime_init(struct EventTime* fEventTime, unsigned secondsSinceEpoch ,
        unsigned usecondsSinceEpoch )
{
    fEventTime->fTv.tv_sec = secondsSinceEpoch;
    fEventTime->fTv.tv_usec = usecondsSinceEpoch;
}
static void EventTime_init(struct EventTime* fEventTime)
{
    fEventTime->init = do_EventTime_init;
}
static bool timeGe(struct timeval *timeArg1, struct timeval *timeArg2)//arg1>=arg2
{
    return (((long) timeArg1->tv_sec > (long) (timeArg2->tv_sec))
            || (((long) timeArg1->tv_sec == (long) timeArg2->tv_sec)
            && ((long) timeArg1->tv_usec >= (long) timeArg2->tv_usec)));
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
    //溢出为负数需要处???
    if ((int)timeArg1->tv_usec < 0) {
        timeArg1->tv_usec += MILLION;
        --timeArg1->tv_sec;
      }
    if ((int)timeArg1->tv_sec < 0)
        timeArg1->tv_sec = timeArg1->tv_usec = 0;
    
}
static void timeAdd(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 = arg1 + arg2
{
    timeArg1->tv_sec += timeArg2->tv_sec;
    timeArg1->tv_usec += timeArg2->tv_usec;
    //溢出为负数需要处???
    if (timeArg1->tv_usec >= MILLION) {
        timeArg1->tv_usec -= MILLION;
        ++timeArg1->tv_sec;
    }

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
    if (entry == NULL || entry->fNext == NULL) return;//末节???
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
    //        timeSinceLastSync -= fDelayQueue->fLastSyncTime.fTv;


    fDelayQueue->fLastSyncTime.fTv = timeNow.fTv;//同步时间

    //调整延时队列 根据时间差确定哪些事件应该被???;或者减???准备进行调度
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
      delaytask_msg("==>%s()head[0x%x]next[0x%x]pre[0x%x]\n", __func__, head(fDelayQueue), head(fDelayQueue)->fNext, head(fDelayQueue)->fPrev);
//      delaytask_msg("==>%s()head[0x%x]\n", __func__, head(fDelayQueue));
#endif
  memset(&fTv, DELAY_ZERO, sizeof(struct timeval));
  if (timeVal.ne(&fDelayQueue->head(fDelayQueue)->fDeltaTimeRemaining.fTv , &fTv)) fDelayQueue->synchronize(fDelayQueue);

  DelayQueueEntry *p = NULL;
  p = fDelayQueue->head(fDelayQueue);
  delaytask_msg("\nqueue begin:\n");
  do
  {
      delaytask_msg("debug: tv_sec[%llu]usec[%llu]\n",
              p->fDeltaTimeRemaining.fTv.tv_sec,
              p->fDeltaTimeRemaining.fTv.tv_usec);
      AlarmHandler *tmp = container_of(p, struct AlarmHandler, fDelayQueueEntry);
      delaytask_msg("fClientData [%d]\n", (unsigned int)(unsigned long)tmp->fClientData);
      p = p->fNext;
  }while(p != head(fDelayQueue));
  delaytask_msg("queue end\n\n");
  /* bug:fix only one/per */
  while (timeVal.eq(&fDelayQueue->head(fDelayQueue)->fDeltaTimeRemaining.fTv, &fTv))
  {
    // This event is due to be handled:

    DelayQueueEntry* toRemove = head(fDelayQueue);
#if 1
    delaytask_msg("head[0x%llx]next[0x%llx]pre[0x%llx]\n", head(fDelayQueue), head(fDelayQueue)->fNext, head(fDelayQueue)->fPrev);
    delaytask_msg("toRemove tv_sec[%llu]usec[%llu]\n",
            toRemove->fDeltaTimeRemaining.fTv.tv_sec,
            toRemove->fDeltaTimeRemaining.fTv.tv_usec);
#endif

    fDelayQueue->removeEntry(toRemove); // do this first, in case handler accesses queue
    //获取派生类的alarm_handler 的指???stu.name, struct student, name
    AlarmHandler *tmp = container_of(toRemove, struct AlarmHandler, fDelayQueueEntry);//)
    tmp->handleTimeout(tmp);
//    toRemove->handleTimeout(); //delete itself
  }
}

void do_DelayQueue_init(DelayQueue *fDelayQueue)
{
    //永远到不了的时间
    fDelayQueue->fDelayQueueEntry.fDeltaTimeRemaining.fTv.tv_sec = INT_MAX; 
    fDelayQueue->fDelayQueueEntry.fDeltaTimeRemaining.fTv.tv_usec = MILLION-1;
    fDelayQueue->fDelayQueueEntry.flag = FIRST;
    fDelayQueue->fDelayQueueEntry.fNext = fDelayQueue->fDelayQueueEntry.fPrev = &fDelayQueue->fDelayQueueEntry;
}
void DelayQueue_init(DelayQueue *fDelayQueue)
{
    fDelayQueue->head = head;
    fDelayQueue->handleAlarm = handleAlarm;
    fDelayQueue->synchronize = synchronize;
    fDelayQueue->removeEntry = removeEntry;
    fDelayQueue->addEntry = addEntry;
    fDelayQueue->init = do_DelayQueue_init;
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
    /* may be you should changed by yourself eg: add usleep(0);*/
    SingleStep(0);
    usleep(30*1000);//may be changed
  }
}

void do_AlarmHandler_init(AlarmHandler* fAlarmHandler, TaskFunc* fProc, void * fClientData, DelayInterval *timeToDelay)
{
    fAlarmHandler->fProc = fProc;
    fAlarmHandler->fClientData = fClientData;
    fAlarmHandler->fDelayQueueEntry.fDeltaTimeRemaining.fTv = timeToDelay->fTv;
    fAlarmHandler->fDelayQueueEntry.fNext = NULL;
    fAlarmHandler->fDelayQueueEntry.fPrev = NULL;
}
void AlarmHandler_init(AlarmHandler *fAlarmHandler)
{
    fAlarmHandler->init = do_AlarmHandler_init;
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

    return (TaskToken) (unsigned long)(alarmHandler->fToken);
}
volatile int total_timer_event = 0;
void timer_event(int sig) 
{
    int ret = 0;
    struct timeval now;
    delaytask_msg("==>%s sig[%d]\n", __func__, sig);
    if (sig == SIGUSR1)
    {
        gettimeofday(&now, NULL);
        delaytask_msg("time coming %llu s %llu us\n", now.tv_sec, now.tv_usec);
        fDelayQueue.handleAlarm(&fDelayQueue);
        DelayQueueEntry* head = fDelayQueue.head(&fDelayQueue);
        if ( head->flag !=  FIRST){
            ts.it_interval.tv_sec = 0;//为0 表示不是间隔(周期)timer
            ts.it_interval.tv_nsec = 0; 
            ts.it_value.tv_sec = head->fDeltaTimeRemaining.fTv.tv_sec; 
            ts.it_value.tv_nsec = head->fDeltaTimeRemaining.fTv.tv_usec * 1000;
            //alarm(head->fDeltaTimeRemaining.fTv.tv_sec);
            ret = timer_settime(timer, 0, &ts, NULL);
            if (ret)
            {
                perror("timer_settime");
            }   
        }
    }
    total_timer_event++;
    delaytask_msg("<==%s sig[%d]\n", __func__, sig);
}
        
TaskToken scheduleTimerTask(__int64 microseconds,
        TaskFun* proc,
        void* clientData)
{
    int ret =0;
    
    if (microseconds < 0)microseconds = 0;
    DelayInterval timeToDelay;
    DelayInterval_init(&timeToDelay, (long)(microseconds/1000000), (long)(microseconds%1000000));
    AlarmHandler *alarmHandler = (AlarmHandler *)malloc(sizeof(AlarmHandler));
    AlarmHandler_init(alarmHandler);
    alarmHandler->init(alarmHandler, proc, clientData, &timeToDelay);

    fDelayQueue.addEntry(&fDelayQueue, &alarmHandler->fDelayQueueEntry);
    DelayQueueEntry* head = fDelayQueue.head(&fDelayQueue);
    if ( (head->fDeltaTimeRemaining.fTv.tv_sec != 0) || (head->fDeltaTimeRemaining.fTv.tv_usec != 0) )
    {
#if 1    
        /* 取消之前挂上的定时器;根据head 新设定时器 */
        struct itimerval value;  
        value.it_value.tv_sec = 0;  
        value.it_value.tv_usec = 0;  
        value.it_interval = value.it_value; 
        
        ret = setitimer(ITIMER_REAL, &value, NULL);  
        if (ret)
        {
            perror("timer_settime");
        }  
#endif        
        //alarm(head->fDeltaTimeRemaining.fTv.tv_sec);
        ts.it_interval.tv_sec = 0;//为0 表示不是间隔(周期)timer
        ts.it_interval.tv_nsec = 0; 
        ts.it_value.tv_sec = head->fDeltaTimeRemaining.fTv.tv_sec; 
        ts.it_value.tv_nsec = head->fDeltaTimeRemaining.fTv.tv_usec * 1000;
        //alarm(head->fDeltaTimeRemaining.fTv.tv_sec);
        ret = timer_settime(timer, 0, &ts, NULL);
        if (ret)
        {
            perror("timer_settime");
        }  
    }
    return (TaskToken)(unsigned long) (alarmHandler->fToken);
}

void *delay_task_func(void *data)
{
    Timeval_init(&timeVal);
    DelayQueue_init(&fDelayQueue);
    fDelayQueue.init(&fDelayQueue);
    sem_post(&DelayedTask_sem);
//    scheduleDelayedTask(1000000, task_test, str);
    doEventLoop(&watchVariable_flag);
    return NULL;
}

void * schedule_timer(void *data)
{
    struct sigevent evp;   
    int ret;
    
    Timeval_init(&timeVal);
    DelayQueue_init(&fDelayQueue);
    fDelayQueue.init(&fDelayQueue);

    evp.sigev_value.sival_ptr = &timer; //连接IO请求与信号处理的必要步骤
    evp.sigev_notify = SIGEV_SIGNAL; //SIGEV_SIGNAL表示以信号的方式通知，满足条件时就会发送信号
    evp.sigev_signo = SIGUSR1; //SIGUSR1是自己设定的信号  到期要发送的信号
    signal(SIGUSR1, timer_event); //第一个参数需要处理的信号值，第二个参数是处理函数
    ret = timer_create(CLOCK_REALTIME, &evp, &timer); 
    if (ret)
    {
        perror("CreateTimeEvent fail\r\n");
    }
    //signal(SIGALRM, timer_event); 
    sem_post(&DelayedTask_sem);
    //    scheduleDelayedTask(1000000, task_test, str);

    
    //doEventLoop(&watchVariable_flag);
    do{}while(1);
    return NULL;
}

int schedule_task_is_empty()
{
    return ((fDelayQueue.head(&fDelayQueue) == (fDelayQueue.head(&fDelayQueue))->fPrev) ? 1 : 0); 
}


