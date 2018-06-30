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

#include "delaytask.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef Win32
#include <thread>
#else
#include <thread>
#include <pthread.h>
#include <semaphore.h>
#include <sys/resource.h>
#endif

#include <stdlib.h>
#include <mutex>
#include <condition_variable>

#define  EnableCoreDumps()\
{\
        struct rlimit   limit;\
        limit.rlim_cur = RLIM_INFINITY;\
        limit.rlim_max = RLIM_INFINITY;\
        setrlimit(RLIMIT_CORE, &limit);\
}

//extern sem_t DelayedTask_sem;

void task_test(void *clientData)
{
//	char *p=clientData;
//	printf("[clientData]%s\n", p);
    int *tmp = (int *)clientData;
    printf("tmp[%d]\n", tmp);
	return;
}

char str[]="hello ,this is a delay task\n";
char char_table[]="abcdefghigklmn\n";

std::condition_variable cv;
std::mutex lock;
#ifdef Win32
#else
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
#endif
int main(void) {
	//pthread_t delay_task_id;
	std::thread delay_task_id;
    //pthread_attr_t attr;
    int i =0;
    
    //EnableCoreDumps();
    //mkdir("./cores",0775);//mkdir -p ../cores
    //system("sysctl -w kernel.core_pattern=./cores/core.%e-%p-%t");//在../cores目录中生成 core.test....
    /*
    if(0 != sem_init(&DelayedTask_sem, 0, 0))
    {
        perror("Semaphore init failed\n");
    }
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&delay_task_id, &attr, delay_task_func, NULL);
	*/
    /*-----------------------------------------------------------------------------
     *  添加为RR调度后cpu从15%增加到80%,查看线程占用CPU,提高了延迟队列的调度频度
     *  root权限才有效
     *-----------------------------------------------------------------------------*/
    /*
	if (set_realtime_schedule(delay_task_id)<0)
    {
        perror("set rlt fail\n");
    }
    sem_wait(&DelayedTask_sem);
	*/
	delay_task_id = std::thread(delay_task_func, nullptr);
	delay_task_id.detach();
	//std::unique_lock<std::mutex> locker(lock);
	//cv.wait(locker);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

//eg:
    for (i=0; i<100 ;i++)
    {
	    scheduleDelayedTask(0, task_test, (void *)i);
    }
    while(!schedule_task_is_empty())
    {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        //usleep(300*1000);
        printf("is not empty\n");
    }
    printf("delay task is empty\n");
	scheduleDelayedTask(2000000, task_test, str);
	scheduleDelayedTask(3000000, task_test, str);
    while (1)
    {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        //sleep(1);
    }

	return EXIT_SUCCESS;
}
