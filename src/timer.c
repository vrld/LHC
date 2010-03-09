#include "timer.h"

/* taken from here: http://segfaulting.com/post6/cross-platform-c-c++-function-to-get-the-current-time-in-milliseconds */

#ifdef WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

long long time_ms()
{
#ifdef WIN32
    /* Windows */
    FILETIME ft;
    LARGE_INTEGER li;

    GetSystemTimeAsFileTime(&ft);
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;

    unsigned long long ret = li.QuadPart;
    ret -= 116444736000000000LL;
    ret /= 10000;
#else
    /* Linux */
    struct timeval tv;
    gettimeofday(&tv, NULL);

    unsigned long long ret = tv.tv_usec;
    ret /= 1000;
    ret += (tv.tv_sec * 1000);
#endif
    return ret;
}

int l_time(lua_State* L)
{
    lua_pushnumber(L, time_ms());
    return 1;
}

/* timer table in lua:
 *   arrival | function
 *   t1      | f1
 *   t2      | f2
 *   ...     | ...
 *
 * scheduler:
 *   for arrival, func in pairs(timers) do
 *      if arrival > time() then
 *          func()
 *          timers[arrival] = nil
 *      end
 *   end
 *
 * add timer:
 *   timers[time() + eta] = func
 */
