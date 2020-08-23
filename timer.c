#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
 
static timer_t timer;
static struct itimerspec ts;

void  time_event(int sig)
{
	int ret;
    struct timeval now;
    gettimeofday(&now, NULL);
	
    printf("time coming %llu s %llu us\n", now.tv_sec, now.tv_usec);
#if 0    //设置下一次定时器
	ts.it_interval.tv_sec = 0;//为0 表示不是间隔(周期)timer
	ts.it_interval.tv_nsec = 0; //设置初始时间为500ms秒，精确到1纳秒
	ts.it_value.tv_sec = 1;	
	ts.it_value.tv_nsec = 0;
	timer_settime(timer, 0, &ts, NULL);
	if (ret)
	{
		perror("timer_settime");
	}
#endif	
}
int main(void)
{
    struct sigevent evp;   
    int ret;
	int count = 5;
	
    evp.sigev_value.sival_ptr = &timer; //连接IO请求与信号处理的必要步骤
    evp.sigev_notify = SIGEV_SIGNAL; //SIGEV_SIGNAL表示以信号的方式通知，满足条件时就会发送信号
    evp.sigev_signo = SIGUSR1; //SIGUSR1是自己设定的信号  到期要发送的信号
    signal(SIGUSR1, time_event); //第一个参数需要处理的信号值，第二个参数是处理函数
    ret = timer_create(CLOCK_REALTIME, &evp, &timer); 
    if (ret)
    {
        perror("CreateTimeEvent fail\r\n");
    }
    else
    {
		while (count--){
			ts.it_interval.tv_sec = 0;
			ts.it_interval.tv_nsec = 0; //设置初始时间为500ms秒，精确到1纳秒
			ts.it_value.tv_sec = 1;
			ts.it_value.tv_nsec = 0; //it_value指当前定时器剩下的到期时间，定时器记时时从500ms减到0
			ret = timer_settime(timer, 0, &ts, NULL); //设置时间事件，将itimerspec的设置放到定时器中，这是开启一个定时器
			if (ret)
			{
				perror("timer_settime");
			}
			sleep(1);
		}
    }
    do{
        sleep(1);
        printf("left : %llu s %llu ns \n", ts.it_value.tv_sec, ts.it_value.tv_nsec);
    }while(1);
    return 0;
}

