/***
 * Copyright (c) 2012 Matthias Richter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 *
 * If you find yourself in a situation where you can safe the author's life
 * without risking your own safety, you are obliged to do so.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "buffer.h"

static const char *INTERNAL_NAME = "lhc.buffer";

inline static size_t max(size_t a, size_t b)
{
	return a >= b ? a : b;
}

int lua_isbuffer(lua_State *L, int idx)
{
	void *ud = lua_touserdata(L, idx);
	if (NULL == ud)
		return 0;

	lua_getmetatable(L, idx);
	luaL_getmetatable(L, INTERNAL_NAME);
	int equal = lua_rawequal(L, -1, -2);
	lua_pop(L, 2);
	return equal;
}

float *lhc_checkbuffer(lua_State *L, int idx)
{
	return (float *)luaL_checkudata(L, idx, INTERNAL_NAME);
}

static int lhc_buffer___len(lua_State *L)
{
	lua_pushinteger(L, lhc_buffer_nsamples(L, 1));
	return 1;
}

static int lhc_buffer___index(lua_State *L)
{
	if (lua_isnumber(L, 2))
	{
		float *buf = (float *)lua_touserdata(L, 1);
		int size   = (int)lhc_buffer_nsamples(L, 1);
		float x    = lua_tonumber(L, 2);
		int n      = (int)x;

		if (n < 1 || n > size)
		{
			/* out of bounds */
			lua_pushnil(L);
		}
		else if ((float)n == x || n == size)
		{
			/* argument is integer or requested last sample in buffer */
			lua_pushnumber(L, buf[n-1]);
		}
		else
		{
			/* linear interpolation */
			float sample = (x - (float)n) * (buf[n] - buf[n-1]) + buf[n-1];
			lua_pushnumber(L, sample);
		}
	}
	else
	{
		/* else: fetch value from metatable */
		luaL_getmetatable(L, INTERNAL_NAME);
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);
	}
	return 1;
}

static int lhc_buffer___newindex(lua_State *L)
{
	float *buf = (float *)lua_touserdata(L, 1);
	int size   = (int)lhc_buffer_nsamples(L, 1);
	int n      = (int)luaL_checkinteger(L, 2);
	float val  = (float)luaL_checknumber(L, 3);

	if (n < 1 || n > size)
		return luaL_error(L, "Index out of bounds: %d", n);

	buf[n-1] = val;
	return 0;
}

/* i am deeply sorry for this one... */
#define __BUFFER_ARITHMETIC_OPERATION(op, neutral)          \
	/* make sure the first value is the buffer  */          \
	if (!lua_isbuffer(L, 1))                                \
		lua_insert(L, 1);                                   \
                                                            \
	float *b1    = (float*)lua_touserdata(L, 1);            \
	size_t size1 = lhc_buffer_nsamples(L, 1);               \
	float *b2    = NULL;                                    \
	size_t size2 = 0;                                       \
                                                            \
	int type = lua_type(L, 2);                              \
	if (LUA_TNUMBER == type)                                \
	{                                                       \
		/* buffer `op` number */                            \
		lua_pushcfunction(L, lhc_buffer_new);               \
		lua_pushvalue(L, 1);                                \
		lua_call(L, 1, 1);                                  \
                                                            \
		float *buf  = (float *)lua_touserdata(L, -1);       \
		float x     = (float)lua_tonumber(L, 2);            \
		for (size_t i = 0; i < size1; ++i)                  \
			buf[i] = op(buf[i], x);                         \
	}                                                       \
	else if (LUA_TFUNCTION == type)                         \
	{                                                       \
		/* buffer `op` function */                          \
		lua_pushcfunction(L, lhc_buffer_new);               \
		lua_pushvalue(L, 1);                                \
		lua_call(L, 1, 1);                                  \
                                                            \
		float *buf  = (float *)lua_touserdata(L, -1);       \
		for (size_t i = 0; i < size1; ++i)                  \
		{                                                   \
			lua_pushvalue(L, 2);                            \
			lua_pushinteger(L, i+1);                        \
			lua_call(L, 1, 1);                              \
			buf[i] = op(b1[i], lua_tonumber(L, -1));        \
			lua_pop(L, 1);                                  \
		}                                                   \
	}                                                       \
	else if (LUA_TTABLE == type)                            \
	{                                                       \
		size2 = lua_objlen(L, 2);                           \
		/* buffer `op` table */                             \
		size_t size_new = max(size1, size2);                \
		lua_pushcfunction(L, lhc_buffer_new);               \
		lua_pushinteger(L, size_new);                       \
		lua_call(L, 1, 1);                                  \
                                                            \
		float *buf  = (float *)lua_touserdata(L, -1);       \
		for (size_t i = 0; i < size_new; ++i)               \
		{                                                   \
			float x = i < size1 ? b1[i] : (float)neutral;   \
			float y = (float)neutral;                       \
			if (i < size2)                                  \
			{                                               \
				lua_rawgeti(L, 2, i+1);                     \
				y = lua_tonumber(L, -1);                    \
				lua_pop(L, 1);                              \
			}                                               \
			buf[i] = op(x, y);                              \
		}                                                   \
	}                                                       \
	else if (LUA_TSTRING == type)                           \
		b2 = (float*)lua_tostring(L, 2);                    \
	else if (lua_isbuffer(L, 2))                            \
		b2 = (float *)lua_touserdata(L, 2);                 \
	else                                                    \
		return luaL_error(L, "Invalid operation");          \
                                                            \
	if (NULL != b2)                                         \
	{                                                       \
		/* buffer `op` (buffer or string */                 \
		size2 = lhc_buffer_nsamples(L, 2);                  \
		size_t size_new = max(size1, size2);                \
		lua_pushcfunction(L, lhc_buffer_new);               \
		lua_pushinteger(L, size_new);                       \
		lua_call(L, 1, 1);                                  \
                                                            \
		float *buf = (float *)lua_touserdata(L, -1);        \
		for (size_t i = 0; i < size_new; ++i)               \
		{                                                   \
			float x = i < size1 ? b1[i] : (float)neutral;   \
			float y = i < size2 ? b2[i] : (float)neutral;   \
			buf[i] = op(x, y);                              \
		}                                                   \
	}                                                       \
	return 1

#define __add(x,y) ((x)+(y))
#define __sub(x,y) ((x)-(y))
#define __mul(x,y) ((x)*(y))
#define __div(x,y) ((x)/(y))

static int lhc_buffer___add(lua_State *L)
{
	__BUFFER_ARITHMETIC_OPERATION(__add, 0.0f);
}

static int lhc_buffer___sub(lua_State *L)
{
	__BUFFER_ARITHMETIC_OPERATION(__sub, 0.0f);
}

static int lhc_buffer___mul(lua_State *L)
{
	__BUFFER_ARITHMETIC_OPERATION(__mul, 1.0f);
}

static int lhc_buffer___div(lua_State *L)
{
	__BUFFER_ARITHMETIC_OPERATION(__div, 1.0f);
}

static int lhc_buffer___mod(lua_State *L)
{
	__BUFFER_ARITHMETIC_OPERATION(fmod, FLT_MAX);
}

static int lhc_buffer___pow(lua_State *L)
{
	__BUFFER_ARITHMETIC_OPERATION(pow, 1.0f);
}

#undef __add
#undef __sub
#undef __mul
#undef __div
#undef __BUFFER_ARITHMETIC_OPERATION

static int lhc_buffer___unm(lua_State *L)
{
	/* clone buffer using lhc_buffer_new */
	lua_pushcfunction(L, lhc_buffer_new);
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);

	/* flip buffer */
	float *buf  = (float *)lua_touserdata(L, -1);
	size_t size = lhc_buffer_nsamples(L, -1);
	for (size_t i = 0; i < size/2; ++i)
	{
		float tmp     = buf[i];
		buf[i]        = buf[size-i-1];
		buf[size-i-1] = tmp;
	}

	return 1;
}

static int lhc_buffer___concat(lua_State *L)
{
	if (!lua_isbuffer(L, 1))                                \
		lua_insert(L, 1);                                   \

	float *b1    = lhc_checkbuffer(L, 1);
	size_t size1 = lhc_buffer_nsamples(L, 1);
	float *b2    = lhc_checkbuffer(L, 2);
	size_t size2 = lhc_buffer_nsamples(L, 2);

	lua_pushcfunction(L, lhc_buffer_new);
	lua_pushinteger(L, size1 + size2);
	lua_call(L, 1, 1);

	float *buf = (float*)lua_touserdata(L, -1);
	for (size_t i = 0; i < size1; ++i)
		buf[i] = b1[i];

	for (size_t i = 0; i < size2; ++i)
		buf[i+size1] = b2[i];

	return 1;
}

/* straigt from lua sourcecode, lstrlib.c:36 */
static size_t posrelat(ptrdiff_t pos, size_t len)
{
	if (pos < 0)
		pos += (size_t)len + 1;
	return max(pos, 0);
}

static int lhc_buffer_map(lua_State *L)
{
	float *buf  = lhc_checkbuffer(L, 1);
	size_t size = lhc_buffer_nsamples(L, 1);
	size_t posi = 0, pose = size;

	int stackpos_function = 2;
	if (lua_isnumber(L, 2))
	{
		posi = posrelat(lua_tointeger(L, 2), size);
		stackpos_function = 3;
	}

	if (lua_isnumber(L, 3))
	{
		pose = posrelat(lua_tointeger(L, 3), size);
		stackpos_function = 4;
	}

	if (!lua_isfunction(L, stackpos_function))
		return luaL_typerror(L, stackpos_function, "function");

	for (; posi <= pose; ++posi)
	{
		lua_pushvalue(L, stackpos_function);
		lua_pushinteger(L, posi);
		lua_pushnumber(L, buf[posi-1]);
		lua_call(L, 2, 1);

		buf[posi-1] = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	lua_settop(L, 1);
	return 1;
}

/* check implementation of str_byte in lua sourcecode, lstrlib.c:110 */
static int lhc_buffer_get(lua_State *L)
{
	float *buf  = lhc_checkbuffer(L, 1);
	size_t size = lhc_buffer_nsamples(L, 1);
	size_t posi = posrelat(luaL_checkinteger(L, 2), size);
	size_t pose = posrelat(luaL_optinteger(L, 3, posi), size);

	if (posi <= 0)
		posi = 1;

	if (pose > size)
		pose = size;

	if (posi > pose)
		return 0;

	int n = (int)(pose - posi + 1);
	luaL_checkstack(L, n, "buffer slice too long");

	for (size_t i = posi; i <= pose; ++i)
		lua_pushnumber(L, buf[i - 1]);

	return n;
}

static int lhc_buffer_set(lua_State *L)
{
	float *buf  = lhc_checkbuffer(L, 1);
	size_t size = lhc_buffer_nsamples(L, 1);
	size_t pos  = posrelat(luaL_checkinteger(L, 2), size);
	float val   = luaL_checknumber(L, 3);

	if (pos < 1 || pos > size)
		return luaL_error(L, "Index out of bounds: %lu", pos);

	buf[pos-1] = val;

	lua_settop(L, 1);
	return 1;
}

static int lhc_buffer_sub(lua_State *L)
{
	float *buf  = lhc_checkbuffer(L, 1);
	size_t size = lhc_buffer_nsamples(L, 1);
	size_t posi = posrelat(luaL_checkinteger(L, 2), size);
	size_t pose = posrelat(luaL_optinteger(L, 3, size), size);

	if (posi <= 0)
		posi = 1;

	if (pose > size)
		pose = size;

	if (posi > pose)
		return 0;

	size_t size_new = pose - posi + 1;

	lua_pushcfunction(L, lhc_buffer_new);
	lua_pushinteger(L, size_new);
	lua_call(L, 1, 1);
	float *buf_new = (float *)lua_touserdata(L, -1);

	for (size_t i = 0; i < size_new; ++i)
		buf_new[i] = buf[posi-1 + i];

	return 1;
}

static int lhc_buffer_insert(lua_State *L)
{
	float *buf      = lhc_checkbuffer(L, 1);
	size_t size_buf = lhc_buffer_nsamples(L, 1);
	size_t posi     = posrelat(luaL_checkinteger(L, 2), size_buf);

	float *insert      = NULL;
	size_t size_insert = 0;
	int should_free = 0;
	int type = lua_type(L, 3);
	if (lua_isbuffer(L, 3))
	{
		insert      = (float *)lua_touserdata(L, 3);
		size_insert = lhc_buffer_nsamples(L, 3);
	}
	else if (LUA_TSTRING == type)
	{
		insert       = (float*)lua_tolstring(L, 3, &size_insert);
		size_insert /= sizeof(float);
	}
	else if (LUA_TTABLE == type)
	{
		should_free = 1;
		size_insert = lua_objlen(L, 3);
		insert = malloc(size_insert * sizeof(float));
		for (size_t i = 0; i < size_insert; ++i)
		{
			lua_rawgeti(L, 3, i+1);
			insert[i] = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
	}
	else if (LUA_TNUMBER == type)
	{
		should_free = 1;
		if (lua_isfunction(L, 4))
		{
			size_insert = lua_tointeger(L, 3);
			insert = malloc(size_insert * sizeof(float));
			for (size_t i = 0; i < size_insert; ++i)
			{
				lua_pushvalue(L, 4);
				lua_pushinteger(L, i+posi+1);
				lua_call(L, 1, 1);
				insert[i] = lua_tonumber(L, -1);
				lua_pop(L, 1);
			}
		}
		else
		{
			size_insert = lua_gettop(L) - 2;
			insert = malloc(size_insert * sizeof(float));
			for (size_t i = 0; i < size_insert; ++i)
				insert[i] = lua_tonumber(L, 3 + i);
		}
	}
	else
		return luaL_typerror(L, 3, "buffer or table or string or number");

	/* create new buffer using lhc_buffer_new */
	size_t size_new = size_buf + size_insert;
	lua_pushcfunction(L, lhc_buffer_new);
	lua_pushnumber(L, size_new);
	lua_call(L, 1, 1);

	float *buf_new = (float *)lua_touserdata(L, -1);
	size_t pose = posi + size_insert;
	for (size_t i = 0; i < size_new; ++i)
	{
		if (i < posi)
			buf_new[i] = buf[i];
		else if (i < pose)
			buf_new[i] = insert[i - posi];
		else
			buf_new[i] = buf[i - size_insert];
	}

	if (should_free)
		free(insert);

	return 1;
}

static int lhc_buffer_convolve(lua_State *L)
{
	float *b1    = lhc_checkbuffer(L, 1);
	size_t size1 = lhc_buffer_nsamples(L, 1);
	float *b2    = NULL;
	size_t size2 = 0;
	int should_free = 0;

	int type = lua_type(L, 2);
	if (LUA_TFUNCTION == type || (LUA_TNUMBER == type && lua_isfunction(L, 3)))
	{
		int fidx = 2;
		size2 = size1;
		if (lua_isfunction(L, 3))
		{
			fidx = 3;
			size2 = luaL_checkinteger(L, 2);
		}

		should_free = 1;
		b2 = malloc(size2 * sizeof(float));
		for (size_t i = 0; i < size2; ++i)
		{
			lua_pushvalue(L, fidx);
			lua_pushinteger(L, i+1);
			lua_call(L, 1, 1);
			b2[i] = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
	}
	else if (LUA_TTABLE == type)
	{
		size2 = lua_objlen(L, 2);
		should_free = 1;
		b2 = malloc(size2 * sizeof(float));
		for (size_t i = 0; i < size2; ++i)
		{
			lua_rawgeti(L, 2, i+1);
			b2[i] = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
	}
	else if (LUA_TSTRING == type)
	{
		b2 = (float *)lua_tolstring(L, 2, &size2);
		size2 /= sizeof(float);
	}
	else if (lua_isbuffer(L, 2))
	{
		b2 = (float *)lua_touserdata(L, 2);
		size2 = lhc_buffer_nsamples(L, 2);
	}
	else
		return luaL_typerror(L, 2, "buffer or string or function or table");

	size_t size_new = size1 + size2 - 1;
	lua_pushcfunction(L, lhc_buffer_new);
	lua_pushinteger(L, size_new);
	lua_call(L, 1, 1);
	float *buf = (float *)lua_touserdata(L, -1);

	for (size_t n = 0; n < size_new; ++n)
	{
		buf[n] = 0.0f;
		for (size_t k = 0; k < max(size1, size2); ++k)
		{
			float x = (k < size1)     ? b1[k]     : 0.0;
			float y = (n - k < size2) ? b2[n - k] : 0.0;
			buf[n] += x * y;
		}
	}

	if (should_free)
		free(b2);

	return 1;
}

static int lhc_buffer_zip(lua_State *L)
{
	int n       = lua_gettop(L);
	size_t size = lhc_buffer_nsamples(L, 1);

	/* check argument sanity and get maximum size */
	for (int i = 1; i <= n; ++i)
	{
		int type = lua_type(L, i);
		if (lua_isbuffer(L, i) || LUA_TSTRING == type)
			size = max(size, lhc_buffer_nsamples(L, i));
		else if (LUA_TFUNCTION == type || LUA_TNUMBER == type)
			/* nothing */;
		else if (LUA_TTABLE == type)
			size = max(size, lua_objlen(L, i));
		else
			return luaL_typerror(L, i, "buffer or string or table or function or number");
	}

	size_t size_new = size * n;
	lua_pushcfunction(L, lhc_buffer_new);
	lua_pushinteger(L, size_new);
	lua_call(L, 1, 1);

	float *buf_new = (float *)lua_touserdata(L, -1);
	memset((void *)buf_new, size_new * sizeof(float), 0);

	/* fill buffer */
	for (int i = 1; i <= n; ++i)
	{
		int type = lua_type(L, i);
		if (LUA_TNUMBER == type)
		{
			float val = lua_tonumber(L, i);
			for (size_t k = 0; k < size; ++k)
				buf_new[k * n + i - 1] = val;
		}
		else if (LUA_TFUNCTION == type)
		{
			for (size_t k = 0; k < size; ++k)
			{
				lua_pushvalue(L, i);
				lua_pushinteger(L, k+1);
				lua_call(L, 1, 1);
				buf_new[k * n + i - 1] = lua_tonumber(L, -1);
				lua_pop(L, 1);
			}
		}
		else if (LUA_TTABLE == type)
		{
			size = lua_objlen(L, i);
			for (size_t k = 0; k < size; ++k)
			{
				lua_rawgeti(L, i, k+1);
				buf_new[k * n + i - 1] = lua_tonumber(L, -1);
				lua_pop(L, 1);
			}
		}
		else /* LUA_TSTRING == type || lua_isbuffer(L, i) */
		{
			float *buf = NULL;
			size = lhc_buffer_nsamples(L, i);
			if (lua_isbuffer(L, i))
				buf  = (float *)lua_touserdata(L, i);
			else if (LUA_TSTRING == type)
				buf = (float *)lua_tostring(L, i);

			for (size_t k = 0; k < size; ++k)
				buf_new[k * n + i - 1] = buf[k];
		}
	}

	return 1;
}

static int lhc_buffer_unzip(lua_State *L)
{
	float *buf  = lhc_checkbuffer(L, 1);
	size_t size = lhc_buffer_nsamples(L, 1);
	size_t n    = luaL_checkinteger(L, 2);

	if (n < 1 || n > size)
		return luaL_error(L, "invalid number of parts requested");

	if (size % n != 0)
		return luaL_error(L, "buffer (size=%lu) cannot be divided into %d parts", size, n);

	luaL_checkstack(L, n+1, "too many buffers requested");

	float **buffers = malloc(n * sizeof(float *));
	size_t size_new = size / n;
	for (size_t i = 0; i < n; ++i)
	{
		lua_pushcfunction(L, lhc_buffer_new);
		lua_pushinteger(L, size_new);
		lua_call(L, 1, 1);
		buffers[i] = (float *)lua_touserdata(L, -1);
	}

	for (size_t i = 0; i < size; i += n)
	{
		size_t ii = i / n;
		for (size_t k = 0; k < n; ++k)
		{
			float *b = buffers[k];
			b[ii] = buf[i+k];
		}
	}

	free(buffers);
	return n;
}

static int lhc_buffer_clone(lua_State *L)
{
	(void)lhc_checkbuffer(L, 1);
	return lhc_buffer_new(L);
}

int lhc_buffer_new(lua_State *L)
{
	int type = lua_type(L, 1);
	if (lua_isbuffer(L, 1)) /* copy buffer */
	{
		size_t size = lua_objlen(L, 1);
		void* orig  = lua_touserdata(L, 1);
		void* buf   = lua_newuserdata(L, size);
		memcpy(buf, orig, size);
	}
	else if (LUA_TSTRING == type) /* buffer from string */
	{
		size_t size;
		const char *str = lua_tolstring(L, 1, &size);
		void* buf       = lua_newuserdata(L, size);
		if (NULL == buf)
			return luaL_error(L, "Cannot create buffer");
		memcpy(buf, (void*)str, size);
	}
	else if (LUA_TTABLE == type) /* buffer from table */
	{
		size_t size = lua_objlen(L, 1);
		float* buf  = (float*)lua_newuserdata(L, size * sizeof(float));
		if (NULL == buf)
			return luaL_error(L, "Cannot create buffer");

		for (size_t i = 1; i <= size; ++i)
		{
			lua_rawgeti(L, 1, i);
			buf[i-1] = lua_tonumber(L, -1);
		}
		lua_pop(L, size);
	}
	else if (LUA_TNUMBER == type) /* buffer from size */
	{
		size_t size = lua_tointeger(L, 1);
		float *buf  = (float *)lua_newuserdata(L, size * sizeof(float));
		if (NULL == buf)
			return luaL_error(L, "Cannot create buffer");

		if (lua_type(L, 2) == LUA_TNUMBER)
		{
			float val = lua_tonumber(L, 2);
			for (size_t i = 0; i < size; ++i)
				buf[i] = val;
		}
		else if (lua_type(L, 2) == LUA_TFUNCTION)
		{
			for (size_t i = 0; i < size; ++i)
			{
				lua_pushvalue(L, 2);
				lua_pushinteger(L, i+1);
				lua_call(L, 1, 1);

				buf[i] = lua_tonumber(L, -1);
				lua_pop(L, 1);
			}
		}
	}
	else
		return luaL_typerror(L, 1, "buffer or table or string or number");

	if (luaL_newmetatable(L, INTERNAL_NAME))
	{
		lua_pushcfunction(L, lhc_buffer___len);
		lua_setfield(L, -2, "__len");

		lua_pushcfunction(L, lhc_buffer___index);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, lhc_buffer___newindex);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, lhc_buffer___add);
		lua_setfield(L, -2, "__add");

		lua_pushcfunction(L, lhc_buffer___sub);
		lua_setfield(L, -2, "__sub");

		lua_pushcfunction(L, lhc_buffer___mul);
		lua_setfield(L, -2, "__mul");

		lua_pushcfunction(L, lhc_buffer___div);
		lua_setfield(L, -2, "__div");

		lua_pushcfunction(L, lhc_buffer___mod);
		lua_setfield(L, -2, "__mod");

		lua_pushcfunction(L, lhc_buffer___pow);
		lua_setfield(L, -2, "__pow");

		lua_pushcfunction(L, lhc_buffer___unm);
		lua_setfield(L, -2, "__unm");

		lua_pushcfunction(L, lhc_buffer___concat);
		lua_setfield(L, -2, "__concat");

		lua_pushcfunction(L, lhc_buffer_map);
		lua_setfield(L, -2, "map");

		lua_pushcfunction(L, lhc_buffer_get);
		lua_setfield(L, -2, "get");

		lua_pushcfunction(L, lhc_buffer_set);
		lua_setfield(L, -2, "set");

		lua_pushcfunction(L, lhc_buffer_sub);
		lua_setfield(L, -2, "sub");

		lua_pushcfunction(L, lhc_buffer_insert);
		lua_setfield(L, -2, "insert");

		lua_pushcfunction(L, lhc_buffer_convolve);
		lua_setfield(L, -2, "convolve");

		lua_pushcfunction(L, lhc_buffer_zip);
		lua_setfield(L, -2, "zip");

		lua_pushcfunction(L, lhc_buffer_unzip);
		lua_setfield(L, -2, "unzip");

		lua_pushcfunction(L, lhc_buffer_clone);
		lua_setfield(L, -2, "clone");
	}
	lua_setmetatable(L, -2);

	return 1;
}

int luaopen_lhc_buffer(lua_State *L)
{
	lua_pushcfunction(L, lhc_buffer_new);
	return 1;
}
