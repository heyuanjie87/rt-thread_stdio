#ifndef LIBC_TIME_H
#define LIBC_TIME_H

#include <time.h>

#ifndef RT_USING_LIBC

    #ifdef __CC_ARM
    typedef int clockid_t;
    #ifndef _TIMESPEC_DEFINED
    #define _TIMESPEC_DEFINED
    /*
     * Structure defined by POSIX.1b to be like a timeval.
     */
    struct timespec 
    {
        time_t  tv_sec;     /* seconds */
        long    tv_nsec;    /* and nanoseconds */
    };
    #endif /* _TIMESPEC_DEFINED */ 
    
	#define CLOCK_REALTIME    0

	int clock_gettime(clockid_t clk_id, struct timespec *ts);
	int clock_settime(clockid_t clk_id, struct timespec *ts);

    #endif /* __CC_ARM */

#else

    #ifdef __CC_ARM
    #include <sys/time.h>
    #endif

#endif

#endif /* LIBC_TIME_H */
