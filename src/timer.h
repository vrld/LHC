#ifndef TIMER_H
#define TIMER_H

#include <lua.h>

long long time_ms();
int l_time(lua_State* L);

#endif /* TIMER_H */
