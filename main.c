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

void task_test(void *clientData)
{
	char *p=clientData;
	printf("[clientData]%s\n", p);
	return;
}

char str[]="hello ,this is a delay task\n";

int main(void) {
	pthread_t delay_task_id;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_create(&delay_task_id, &attr, delay_task_func, NULL);
	sleep(1);

	scheduleDelayedTask(1000000, task_test, str);
	scheduleDelayedTask(2000000, task_test, str);
	scheduleDelayedTask(2000000, task_test, str);
    while (1)
    {
        sleep(1);
    }

//	puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
	return EXIT_SUCCESS;
}
