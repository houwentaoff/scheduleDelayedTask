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
	struct timeval tv;
	int tokenCounter;
}DelayInterval;

typedef struct EventTime
{
	void (*init)(struct EventTime* fEventTime, unsigned secondsSinceEpoch,
		    unsigned usecondsSinceEpoch);
	struct timeval tv;
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
	void (*handle_timeout)(struct AlarmHandler* fAlarmHandler);
	int fToken;
}AlarmHandler_t;

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

	struct timeval tv;
}Timeval;

static struct Timeval timeVal;

DelayQueue fDelayQueue={0};
char watchVariable_flag=0;
static const int MILLION = 1000000;
sem_t DelayedTask_sem;
static timer_t timer;
static struct itimerspec ts;

static void handle_timeout(struct AlarmHandler* fAlarmHandler)
{
    delaytask_msg("==>%s\n", __func__);
    (*fAlarmHandler->fProc)(fAlarmHandler->fClientData);
    free(fAlarmHandler);
    //fAlarmHandler->fDelayQueueEntry.handle_timeout();
    delaytask_msg("<==%s\n", __func__);
}
static void DelayInterval_init(DelayInterval * fDelayInterval, long seconds, long useconds)
{
    fDelayInterval->tv.tv_sec = seconds;
    fDelayInterval->tv.tv_usec = useconds;
}
static void do_EventTime_init(struct EventTime* fEventTime, unsigned secondsSinceEpoch ,
        unsigned usecondsSinceEpoch )
{
    fEventTime->tv.tv_sec = secondsSinceEpoch;
    fEventTime->tv.tv_usec = usecondsSinceEpoch;
}
static void EventTime_init(struct EventTime* fEventTime)
{
    fEventTime->init = do_EventTime_init;
}
static bool time_ge(struct timeval *timeArg1, struct timeval *timeArg2)//arg1>=arg2
{
    return (((long) timeArg1->tv_sec > (long) (timeArg2->tv_sec))
            || (((long) timeArg1->tv_sec == (long) timeArg2->tv_sec)
            && ((long) timeArg1->tv_usec >= (long) timeArg2->tv_usec)));
}
static bool time_le(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 <= arg2
{
    return time_ge(timeArg2, timeArg1);
}
static bool time_lt(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 <  arg2
{
    return (!time_ge(timeArg1, timeArg2));
}
static bool time_eq(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 == arg2
{
    return (time_ge(timeArg1, timeArg2) && time_ge(timeArg2, timeArg1));
}
static bool time_ne(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 != arg2
{
    return (!time_eq(timeArg1, timeArg2));
}
static void time_sub(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 = arg1 - arg2
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
static void time_add(struct timeval *timeArg1, struct timeval *timeArg2)//arg1 = arg1 + arg2
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
    ftimeval->ge = time_ge;
    ftimeval->add = time_add;
    ftimeval->sub = time_sub;
    ftimeval->eq = time_eq;
    ftimeval->ne = time_ne;
    ftimeval->le = time_le;
    ftimeval->lt = time_lt;
}

static void addEntry(struct DelayQueue* fDelayQueue, DelayQueueEntry* newEntry)
{
    fDelayQueue->synchronize(fDelayQueue);

    DelayQueueEntry* cur = fDelayQueue->head(fDelayQueue);
    
    while ((timeVal.ge)(&newEntry->fDeltaTimeRemaining.tv, &cur->fDeltaTimeRemaining.tv))
    {
        
        //newEntry->fDeltaTimeRemaining.tv -= cur->fDeltaTimeRemaining.tv;
        timeVal.sub(&newEntry->fDeltaTimeRemaining.tv, &cur->fDeltaTimeRemaining.tv);
        cur = cur->fNext;
        
    }

    //cur->fDeltaTimeRemaining.tv -= newEntry->fDeltaTimeRemaining.tv;
    timeVal.sub(&cur->fDeltaTimeRemaining.tv, &newEntry->fDeltaTimeRemaining.tv);
    //add it to queue Entry
    newEntry->fNext = cur;
    newEntry->fPrev = cur->fPrev;
    cur->fPrev = newEntry->fPrev->fNext = newEntry;
}

static void removeEntry(DelayQueueEntry* entry)
{
    if (entry == NULL || entry->fNext == NULL) return;//末节???
    //entry->fNext->fDeltaTimeRemaining.tv += entry->fDeltaTimeRemaining.tv;
    timeVal.add(&entry->fNext->fDeltaTimeRemaining.tv, &entry->fDeltaTimeRemaining.tv);
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
    if (timeVal.lt(&timeNow.tv, &fDelayQueue->fLastSyncTime.tv))
    {
        fDelayQueue->fLastSyncTime.tv = timeNow.tv;
        return;
    }

    DelayInterval timeSinceLastSync;
    //timeSinceLastSync.tv = timeNow.tv - fDelayQueue->fLastSyncTime.tv;

    timeSinceLastSync.tv = timeNow.tv;
    timeVal.sub(&timeSinceLastSync.tv, &fDelayQueue->fLastSyncTime.tv);
    //        timeSinceLastSync -= fDelayQueue->fLastSyncTime.tv;


    fDelayQueue->fLastSyncTime.tv = timeNow.tv;//同步时间

    //调整延时队列 根据时间差确定哪些事件应该被???;或者减???准备进行调度
    DelayQueueEntry* curEntry = fDelayQueue->head(fDelayQueue);
    while (timeVal.ge(&timeSinceLastSync.tv, &curEntry->fDeltaTimeRemaining.tv))
    {
        timeVal.sub (&timeSinceLastSync.tv, &curEntry->fDeltaTimeRemaining.tv);
        memset(&curEntry->fDeltaTimeRemaining.tv, DELAY_ZERO, sizeof(struct timeval));// = (long)DELAY_ZERO;
        curEntry =curEntry->fNext;
    }
    timeVal.sub(&curEntry->fDeltaTimeRemaining.tv, &timeSinceLastSync.tv);
}

static void handleAlarm(DelayQueue *fDelayQueue) {
  struct timeval tv;
#if 0
      delaytask_msg("==>%s()head[0x%x]next[0x%x]pre[0x%x]\n", __func__, head(fDelayQueue), head(fDelayQueue)->fNext, head(fDelayQueue)->fPrev);
//      delaytask_msg("==>%s()head[0x%x]\n", __func__, head(fDelayQueue));
#endif
  memset(&tv, DELAY_ZERO, sizeof(struct timeval));
  if (timeVal.ne(&fDelayQueue->head(fDelayQueue)->fDeltaTimeRemaining.tv , &tv)) fDelayQueue->synchronize(fDelayQueue);

  DelayQueueEntry *p = NULL;
  p = fDelayQueue->head(fDelayQueue);
  delaytask_msg("\nqueue begin:\n");
  do
  {
      delaytask_msg("debug: tv_sec[%llu]usec[%llu]\n",
              p->fDeltaTimeRemaining.tv.tv_sec,
              p->fDeltaTimeRemaining.tv.tv_usec);
      struct AlarmHandler *tmp = container_of(p, struct AlarmHandler, fDelayQueueEntry);
      delaytask_msg("fClientData [%d]\n", (unsigned int)(unsigned long)tmp->fClientData);
      p = p->fNext;
  }while(p != head(fDelayQueue));
  delaytask_msg("queue end\n\n");
  /* bug:fix only one/per */
  while (timeVal.eq(&fDelayQueue->head(fDelayQueue)->fDeltaTimeRemaining.tv, &tv))
  {
    // This event is due to be handled:

    DelayQueueEntry* toRemove = head(fDelayQueue);
#if 1
    delaytask_msg("head[0x%llx]next[0x%llx]pre[0x%llx]\n", head(fDelayQueue), head(fDelayQueue)->fNext, head(fDelayQueue)->fPrev);
    delaytask_msg("toRemove tv_sec[%llu]usec[%llu]\n",
            toRemove->fDeltaTimeRemaining.tv.tv_sec,
            toRemove->fDeltaTimeRemaining.tv.tv_usec);
#endif

    fDelayQueue->removeEntry(toRemove); // do this first, in case handler accesses queue
    //获取派生类的alarm_handler 的指???stu.name, struct student, name
    struct AlarmHandler *tmp = container_of(toRemove, struct AlarmHandler, fDelayQueueEntry);//)
    tmp->handle_timeout(tmp);
//    toRemove->handle_timeout(); //delete itself
  }
}

void do_DelayQueue_init(DelayQueue *fDelayQueue)
{
    //永远到不了的时间
    fDelayQueue->fDelayQueueEntry.fDeltaTimeRemaining.tv.tv_sec = INT_MAX; 
    fDelayQueue->fDelayQueueEntry.fDeltaTimeRemaining.tv.tv_usec = MILLION-1;
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

void do_AlarmHandler_init(struct AlarmHandler* fAlarmHandler, TaskFunc* fProc, void * fClientData, DelayInterval *timeToDelay)
{
    fAlarmHandler->fProc = fProc;
    fAlarmHandler->fClientData = fClientData;
    fAlarmHandler->fDelayQueueEntry.fDeltaTimeRemaining.tv = timeToDelay->tv;
    fAlarmHandler->fDelayQueueEntry.fNext = NULL;
    fAlarmHandler->fDelayQueueEntry.fPrev = NULL;
}
void AlarmHandler_init(struct AlarmHandler *fAlarmHandler)
{
    fAlarmHandler->init = do_AlarmHandler_init;
    fAlarmHandler->handle_timeout  = handle_timeout;
}

int token(struct AlarmHandler * fAlarmHandler)
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
    struct  AlarmHandler *alarmHandler = (struct  AlarmHandler *)malloc(sizeof(struct  AlarmHandler));
    AlarmHandler_init(alarmHandler);
    alarmHandler->init(alarmHandler, proc, clientData, &timeToDelay);
    fDelayQueue.addEntry(&fDelayQueue, &alarmHandler->fDelayQueueEntry);

    return (TaskToken) (unsigned long)(alarmHandler->fToken);
}
volatile int total_timer_event = 0;
/**
 * @brief  非linux中:更新counter (32位counter 虚拟成64)
 *
 *         更新counter 时间点
 *            * timer 中断
 *            * 读取counter时更新
 *            * 假设没定时任务时，如何获取counter -> 按照最大定时3s一直跑, 这样可以任意时刻获取时间
 *
 * @param sig
 */
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
            ts.it_value.tv_sec = head->fDeltaTimeRemaining.tv.tv_sec; 
            ts.it_value.tv_nsec = head->fDeltaTimeRemaining.tv.tv_usec * 1000;
            //alarm(head->fDeltaTimeRemaining.tv.tv_sec);
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
    struct AlarmHandler *alarmHandler = (struct AlarmHandler *)malloc(sizeof(struct AlarmHandler));
    AlarmHandler_init(alarmHandler);
    alarmHandler->init(alarmHandler, proc, clientData, &timeToDelay);

    fDelayQueue.addEntry(&fDelayQueue, &alarmHandler->fDelayQueueEntry);
    DelayQueueEntry* head = fDelayQueue.head(&fDelayQueue);
    if ( (head->fDeltaTimeRemaining.tv.tv_sec != 0) || (head->fDeltaTimeRemaining.tv.tv_usec != 0) )
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
        //alarm(head->fDeltaTimeRemaining.tv.tv_sec);
        ts.it_interval.tv_sec = 0;//为0 表示不是间隔(周期)timer
        ts.it_interval.tv_nsec = 0; 
        ts.it_value.tv_sec = head->fDeltaTimeRemaining.tv.tv_sec; 
        ts.it_value.tv_nsec = head->fDeltaTimeRemaining.tv.tv_usec * 1000;
        //alarm(head->fDeltaTimeRemaining.tv.tv_sec);
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


