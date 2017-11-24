#include <sys/time.h>
#include <rtthread.h>

time_t time(time_t *tr)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    if (tr)
    {
        *tr = ts.tv_sec;    
    }

    return (time_t)ts.tv_sec;
}

int gettimeofday(struct timeval *tp, void *ignore)
{
    struct timespec ts;

    if (tp)
    {
        clock_gettime(CLOCK_REALTIME, &ts);
        tp->tv_sec = ts.tv_sec;
        tp->tv_usec = ts.tv_nsec/1000;
    }

    return 0;   
}
