#ifndef _INTER_STACK_TOOLS_H
#define _INTER_STACK_TOOLS_H

#include <lua.h>
#include <stdlib.h>

typedef struct StringBuilder {
	size_t size;
	char* str;
} StringBuilder;

typedef enum {
	LUA_FT_C,
	LUA_FT_VM,
	LUA_FT_FASTJIT
} FunctionType;

FunctionType l_functiontype(lua_State* L, int idx);

#define string_builder_init(sb) \
	(sb.size = 0, sb.str = NULL)

#define string_builder_cleanup(sb) \
	(sb.size = 0, free(sb.str), sb.str = NULL)

int string_writer(lua_State* L, const void* b, size_t size, void* ud);

#define l_copy_nil(from, to, i) \
	lua_pushnil(to);

#define l_copy_boolean(from, to, i) \
	lua_pushboolean(to, lua_toboolean(from, i))

#define l_copy_number(from, to, i) \
	lua_pushnumber(to, lua_tonumber(from, i))

void l_copy_string(lua_State* from, lua_State* to, int i);
void l_copy_table(lua_State* from, lua_State* to, int i);
void l_copy_function(lua_State* from, lua_State* to, int i);
void l_copy_udata(lua_State* from, lua_State* to, int i);

#define l_copy_lightudata(from, to, i) \
	lua_pushlightuserdata(to, (void*)lua_topointer(from, i))

void l_copy_values(lua_State* from, lua_State* to, int n);
#define l_move_values(from, to, n) \
	l_copy_values(from, to, n); \
	lua_pop(from, n);

#endif
