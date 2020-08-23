#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

typedef int (*ep_callback_func)(int event,int fd);

typedef struct tagEpEntry
{
  int fd;
  ep_callback_func fCallback;
}ep_entry_s;

void ep_init();
void ep_add(ep_entry_s* p);
void ep_del(ep_entry_s* p);
void ep_exit();
void ep_wait();
static int g_epfd = 0;

static ep_entry_s g_ep_entry = {-1,NULL};

static int _callback(int event,int fd)
{
  uint64_t count = 0;
  if(0 != (EPOLLIN & event))
  {
    int n =read(fd,(void*)&count,sizeof(count));
    //printf("%d\n",n);
    if (n > 0)
    {
      printf("tick.\n");
    }
  }
  return 0;
}

void timer_cre()
{
  struct itimerspec timer;
  int timerfd = timerfd_create(CLOCK_MONOTONIC,0);
  printf("timefd=%d\n",timerfd);
  memset(&timer,0,sizeof(timer));
  timer.it_interval.tv_sec = 2;
  timer.it_value.tv_sec = 2;
  timerfd_settime(timerfd,0,&timer,NULL);
  g_ep_entry.fd = timerfd;
  g_ep_entry.fCallback = (ep_callback_func)_callback;
  ep_add(&g_ep_entry);
  return;
}

void _timer_del()
{
  ep_del(&g_ep_entry);
  close(g_ep_entry.fd);
  g_ep_entry.fCallback = NULL;
  return;
}
void ep_init()
{
   int epfd = epoll_create(1);
   g_epfd = epfd;
   return ;
}

void ep_add(ep_entry_s* p)
{
  struct epoll_event stEpEvent;
  stEpEvent.events = EPOLLIN | EPOLLHUP | EPOLLERR;
  stEpEvent.data.ptr = (void*)p;
  epoll_ctl(g_epfd,EPOLL_CTL_ADD,p->fd,&stEpEvent);
  return;
}

void ep_del(ep_entry_s* p)
{
  struct epoll_event stEpEvent;
  stEpEvent.events = EPOLLIN | EPOLLHUP | EPOLLERR;
  stEpEvent.data.ptr = (void*)p;
  epoll_ctl(g_epfd,EPOLL_CTL_DEL,p->fd,&stEpEvent);
  return;
}

void ep_exit()
{
  g_epfd = -1;
  return;
}

void ep_wait()
{
  int fd;
  int num;
  ep_entry_s* pstp;
  ep_callback_func fcallback;
  struct epoll_event astEvent[1];
  while(1)
  {
    fd = epoll_wait(g_epfd,astEvent,1,-1);
    for(num = 0; num < fd;++num)
    {
      pstp = (ep_entry_s*)astEvent[num].data.ptr;
      fcallback = pstp->fCallback;
      fcallback(astEvent[num].events,pstp->fd);
    }
  }
}
int main()
{
  ep_init();
  timer_cre();
  ep_wait();
  return 0;
}