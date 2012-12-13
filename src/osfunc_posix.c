#include "osfunc.h"
#ifdef __linux__
#include <linux/time.h>
#endif
#include <time.h>

#include <stdio.h>

int hres_sleep(double t)
{
	struct timespec delay;
	delay.tv_sec  = (time_t)t;
	delay.tv_nsec = (long)((t - (double)delay.tv_sec) * 1000000000);
	return 0 == nanosleep(&delay, NULL);
}
