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
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/resource.h>

#define  EnableCoreDumps()\
{\
        struct rlimit   limit;\
        limit.rlim_cur = RLIM_INFINITY;\
        limit.rlim_max = RLIM_INFINITY;\
        setrlimit(RLIMIT_CORE, &limit);\
}

extern sem_t DelayedTask_sem;

void task_test(void *clientData)
{
//	char *p=clientData;
//	printf("[clientData]%s\n", p);
    int *tmp = clientData;
    printf("tmp[%d]\n", (int)tmp);
	return;
}

char str[]="hello ,this is a delay task\n";
char char_table[]="abcdefghigklmn\n";

int main(void) {
	pthread_t delay_task_id;
    pthread_attr_t attr;
    int i =0;
    
    EnableCoreDumps();
    mkdir("./cores",0775);//mkdir -p ../cores
    system("sysctl -w kernel.core_pattern=./cores/core.%e-%p-%t");//在../cores目录中生成 core.test....
    
    if(0 != sem_init(&DelayedTask_sem, 0, 0))
    {
        perror("Semaphore init failed\n");
    }
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&delay_task_id, &attr, delay_task_func, NULL);
    sem_wait(&DelayedTask_sem);
//eg:
    for (i=0; i<100 ;i++)
    {
	    scheduleDelayedTask(0, task_test, (void *)i);
    }
//	scheduleDelayedTask(2000000, task_test, str);
//	scheduleDelayedTask(2000000, task_test, str);
    while (1)
    {
        sleep(1);
    }

//	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	return EXIT_SUCCESS;
}
