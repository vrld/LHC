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

#include <sndfile.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "soundfile.h"
#include "buffer.h"

static int get_format_enum(lua_State *L, const char *format_str, int bits)
{
	int format;
	if (0 == strcmp(format_str, "wav"))
		format = SF_FORMAT_WAV;
	else if (0 == strcmp(format_str, "aiff"))
		format = SF_FORMAT_AIFF;
	else if (0 == strcmp(format_str, "au"))
		format = SF_FORMAT_AU;
	else if (0 == strcmp(format_str, "raw"))
		format = SF_FORMAT_RAW;
	else if (0 == strcmp(format_str, "mat4"))
		format = SF_FORMAT_MAT4;
	else if (0 == strcmp(format_str, "mat5"))
		format = SF_FORMAT_MAT5;
	else if (0 == strcmp(format_str, "caf"))
		format = SF_FORMAT_CAF;
	else if (0 == strcmp(format_str, "ogg"))
		format = SF_FORMAT_OGG;
	else
		return luaL_error(L, "Unknown format: `%s'", format_str);

	switch (bits)
	{
		case 8:
			format |= SF_FORMAT_PCM_S8; break;
		case 16:
			format |= SF_FORMAT_PCM_16; break;
		case 24:
			format |= SF_FORMAT_PCM_24; break;
		case 32:
			format |= SF_FORMAT_PCM_32; break;
		default:
			return luaL_error(L, "Unsupported bit depth: %d.", bits);
	}

	return format;
}

typedef struct
{
	void *data;
	size_t len;
	ptrdiff_t pos;
} vio_bufferinfo;

static sf_count_t vio_get_filelen(void *ud)
{
	vio_bufferinfo *info = (vio_bufferinfo *)ud;
	return info->len;
}

static sf_count_t vio_seek(sf_count_t offset, int whence, void *ud)
{
	vio_bufferinfo *info = (vio_bufferinfo *)ud;

	switch (whence)
	{
		case SEEK_CUR:
			info->pos += offset;
			break;
		case SEEK_SET:
			info->pos = offset;
			break;
		case SEEK_END:
			info->pos = info->len - offset;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	if (info->pos < 0)
		info->pos = 0;

	if (info->pos > (ptrdiff_t)info->len)
		info->pos = info->len;

	return info->pos;
}

static sf_count_t vio_read(void *ptr, sf_count_t count, void *ud)
{
	vio_bufferinfo *info = (vio_bufferinfo *)ud;

	if (info->pos + count > (ptrdiff_t)info->len)
		count = info->len - info->pos;

	memcpy(ptr, info->data, count);

	return count;
}

static sf_count_t vio_write(const void *ptr, sf_count_t count, void *ud)
{
	vio_bufferinfo *info = (vio_bufferinfo *)ud;

	size_t new_size = info->len + count;
	info->data = realloc(info->data, new_size);

	if (NULL == info->data)
		return 0;

	char *out = (char *)info->data + info->pos;
	memcpy(out, ptr, count);

	info->pos = info->len = new_size;
	return count;
}

static sf_count_t vio_tell(void *ud)
{
	vio_bufferinfo *info = (vio_bufferinfo *)ud;
	return info->pos;
}

static SF_VIRTUAL_IO virtual_io = 
{
	vio_get_filelen,
	vio_seek,
	vio_read,
	vio_write,
	vio_tell,
};

static int lhc_soundfile_decode_common(lua_State *L, SNDFILE *sf, SF_INFO *info)
{
	size_t n_samples = info->frames * info->channels;
	lua_pushcfunction(L, lhc_buffer_new);
	lua_pushinteger(L, n_samples);
	lua_call(L, 1, 1);

	float *buf = (float *)lua_touserdata(L, -1);
	size_t read = sf_read_float(sf, buf, n_samples);
	sf_close(sf);

	if (read != n_samples)
		return luaL_error(L, "Error decoding: Requested to read %lu samples,"
				"but read only %lu", n_samples, read);

	lua_pushinteger(L, info->samplerate);
	lua_pushinteger(L, info->channels);

	return 3;
}

static int lhc_soundfile_encode_common(lua_State *L, SNDFILE *sf, float *buf)
{
	size_t buf_len = lhc_buffer_nsamples(L, 1);
	size_t written = sf_write_float(sf, buf, buf_len);
	sf_close(sf);

	if (written != buf_len)
		return luaL_error(L, "Error writing buffer: Requested to write %lu samples,"
				"but wrote only %lu", buf_len, written);

	return 0;
}

static int lhc_soundfile_decode(lua_State *L)
{
	if (!lua_isstring(L, 1))
		return luaL_typerror(L, 1, "string");

	vio_bufferinfo ud;
	ud.data = (void *)lua_tolstring(L, 1, &ud.len);
	ud.pos = 0;

	SF_INFO info;
	SNDFILE *sf = sf_open_virtual(&virtual_io, SFM_READ, &info, (void*)&ud);
	if (NULL == sf)
		return luaL_error(L, "Cannot open context for decoding: %s",
				sf_strerror(NULL));

	return lhc_soundfile_decode_common(L, sf, &info);
}

static int lhc_soundfile_encode(lua_State *L)
{
	float *buf         = lhc_checkbuffer(L, 1);
	const char *format = luaL_checkstring(L, 2);

	vio_bufferinfo ud;
	ud.data = NULL;
	ud.len  = 0;
	ud.pos  = 0;

	SF_INFO info    = {0,0,0,0,0,0};
	info.samplerate = luaL_optinteger(L, 3, 44100);
	info.channels   = luaL_optinteger(L, 4, 1);
	int bits        = luaL_optinteger(L, 5, 16);
	info.format     = get_format_enum(L, format, bits);

	SNDFILE *sf = sf_open_virtual(&virtual_io, SFM_WRITE, &info, (void*)&ud);
	if (NULL == sf)
		return luaL_error(L, "Cannot open context for encoding: %s",
				sf_strerror(NULL));

	int ret = lhc_soundfile_encode_common(L, sf, buf);

	if (NULL != ud.data)
		free(ud.data);

	return ret;
}

static int lhc_soundfile_read(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);

	SF_INFO info;
	SNDFILE *sf = sf_open(path, SFM_READ, &info);
	if (NULL == sf)
		return luaL_error(L, "Cannot open `%s' for reading: %s",
				path, sf_strerror(NULL));

	return lhc_soundfile_decode_common(L, sf, &info);
}

static int lhc_soundfile_write(lua_State *L)
{
	float *buf       = lhc_checkbuffer(L, 1);
	const char *path = luaL_checkstring(L, 2);

	SF_INFO info    = {0,0,0,0,0,0};
	info.samplerate = luaL_optinteger(L, 3, 44100);
	info.channels   = luaL_optinteger(L, 4, 1);
	int bits        = luaL_optinteger(L, 5, 16);
	info.format     = get_format_enum(L, strrchr(path, '.') + 1, bits);

	SNDFILE *sf = sf_open(path, SFM_WRITE, &info);
	if (NULL == sf)
		return luaL_error(L, "Cannot open `%s' for writing: %s",
				path, sf_strerror(NULL));

	return lhc_soundfile_encode_common(L, sf, buf);
}

int luaopen_lhc_soundfile(lua_State* L)
{
	lua_createtable(L, 0, 4);

	lua_pushcfunction(L, lhc_soundfile_decode);
	lua_setfield(L, -2, "decode");

	lua_pushcfunction(L, lhc_soundfile_encode);
	lua_setfield(L, -2, "encode");

	lua_pushcfunction(L, lhc_soundfile_read);
	lua_setfield(L, -2, "read");

	lua_pushcfunction(L, lhc_soundfile_write);
	lua_setfield(L, -2, "write");

	return 1;
}
