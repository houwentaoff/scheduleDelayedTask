/*
 * =====================================================================================
 *       Copyright (c), 2013-2020, Goke C&S.
 *       Filename:  main.c
 *
 *    Description:  
 *         Others:
 *   
 *        Version:  1.0
 *        Date:  Tuesday, November 11, 2014 02:13:24 HKT
 *       Revision:  none
 *       Compiler:  arm-gcc
 *
 *         Author:  Sean , houwentaoff@gmail.com
 *   Organization:  Goke
 *        History:  Tuesday, November 11, 2014 Created by SeanHou
 *
 * =====================================================================================
 */
#include <unistd.h>
#include "delaytask.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#define  EnableCoreDumps()\
{\
        struct rlimit   limit;\
        limit.rlim_cur = RLIM_INFINITY;\
        limit.rlim_max = RLIM_INFINITY;\
        setrlimit(RLIMIT_CORE, &limit);\
}

extern sem_t DelayedTask_sem;
extern int total_timer_event;

void task_test(void *clientData)
{
//	char *p=clientData;
//	printf("[clientData]%s\n", p);
    int *tmp = clientData;
    printf("this is tmp[%llu]\n", (unsigned long)tmp);
	return;
}

char str[]="hello ,this is a delay task\n";
char char_table[]="abcdefghigklmn\n";

/**
 * @brief 需要root权限
 *
 * @param thread_id
 *
 * @return 
 */
static int set_realtime_schedule(pthread_t thread_id)
{
	struct sched_param param;
	int policy = SCHED_RR;
	int priority = 90;
	if (!thread_id)
		return -1;
	memset(&param, 0, sizeof(param));
	param.sched_priority = priority;
	if (pthread_setschedparam(thread_id, policy, &param) < 0)
		perror("pthread_setschedparam");
	pthread_getschedparam(thread_id, &policy, &param);
	if (param.sched_priority != priority)
		return -1;
	return 0;
}

int main(void) {
	pthread_t delay_task_id;
    pthread_attr_t attr;
    int i =0;

    EnableCoreDumps();
    mkdir("./cores",0775);//mkdir -p ../cores
    //system("sysctl -w kernel.core_pattern=./cores/core.%e-%p-%t");//在../cores目录中生成 core.test....
    
    if(0 != sem_init(&DelayedTask_sem, 0, 0))
    {
        perror("Semaphore init failed\n");
    }
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
#if defined(TIMER)
    pthread_create(&delay_task_id, &attr, schedule_timer, NULL);
#endif
#if defined (SCHEDULE_DELAY)
    pthread_create(&delay_task_id, &attr, delay_task_func, NULL);
#endif
#if defined(LINUX)
    /*-----------------------------------------------------------------------------
     *  添加为RR调度后cpu从15%增加到80%,查看线程占用CPU,提高了延迟队列的调度频度
     *  root权限才有效
     *-----------------------------------------------------------------------------*/
    if (set_realtime_schedule(delay_task_id)<0)
    {
        perror("set rlt fail\n");
    }
#endif    
    sem_wait(&DelayedTask_sem);
#if defined(TIMER)    
    for (i=0; i<3 ;i++)
    {
        scheduleTimerTask(500000*(i+1), task_test, (void *)(unsigned long)i);
    }
    usleep(500000);
    scheduleTimerTask(500000, task_test, (void *)100);
    scheduleTimerTask(500000, task_test, (void *)101);
    scheduleTimerTask(600000, task_test, (void *)102);
#endif

#if defined(SCHEDULE_DELAY)
//eg:
    for (i=0; i<100 ;i++)
    {
	    scheduleDelayedTask(0, task_test, (void *)i);
    }
#endif    
    while(!schedule_task_is_empty())
    {
        usleep(300*1000);
        printf("task queue is not null\n");
    }
    printf("task queue is empty\n");
    printf("total_timer_event[%d]\n", total_timer_event);
//	scheduleDelayedTask(2000000, task_test, str);
//	scheduleDelayedTask(2000000, task_test, str);
    while (1)
    {
        sleep(1);
    }

//	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	return EXIT_SUCCESS;
}
