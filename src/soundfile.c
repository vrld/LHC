#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

typedef enum {
	STREAM_READONCE,
	STREAM_READLOOP,
	STREAM_WRITE
} StreamType;

typedef struct FileInfo {
	SF_INFO info;
	SNDFILE *file;
	StreamType type;
	sf_count_t pos;
} FileInfo;

static const char* to_format_name(int format)
{
	static const char* major_format[] = { "INVALID",
		"Microsoft WAV", "Apple/SGI AIFF", "Sun/NeXT AU", "RAW PCM", "Ensoniq PARIS",
		"Amiga IFF/SVX8/SV16", "Sphere NIST", "VOC", "Berkeley/IRCAM/CARL",
		"Sonic Foundry 64 bit RIFF/WAV", "Matlab 4.2", "Matlab 5.0",
		"Portable Voice Format", "Fasttracker 2 Extended Instrument", "HMM Tool Kit",
		"Midi Sample Dump", "Audio Visual Research", "MS WAVE", "Sound Designer 2",
		"FLAC", "Core Audio", "Psion WVE", "OGG", "MPC", "RG64 WAV"
	};
	return major_format[(format & SF_FORMAT_TYPEMASK) >> 16];
}

static int major_format_by_extension(const char* path)
{
	const char* ext = strrchr(path, '.');
	if (NULL == ext)
		return 0;

	ext++;
	if (0 == strcmp(ext, "wav"))
		return SF_FORMAT_WAV;
	if (0 == strcmp(ext, "aiff"))
		return SF_FORMAT_AIFF;
	if (0 == strcmp(ext, "au"))
		return SF_FORMAT_AU;
	if (0 == strcmp(ext, "raw"))
		return SF_FORMAT_RAW;
	if (0 == strcmp(ext, "mat4"))
		return SF_FORMAT_MAT4;
	if (0 == strcmp(ext, "mat5"))
		return SF_FORMAT_MAT5;
	if (0 == strcmp(ext, "caf"))
		return SF_FORMAT_CAF;
	if (0 == strcmp(ext, "ogg"))
		return SF_FORMAT_OGG;

	return 0; // error
}

static int l_to_format_enum(lua_State* L, const char* path, int bits)
{
	int format = major_format_by_extension(path);
	if (0 == format)
		return luaL_error(L, "Unsupported file extension: `%s' (%s).", strrchr(path, '.'), path);

	switch (bits) {
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

static FileInfo* l_checkfile(lua_State* L, int idx)
{
	FileInfo* si = (FileInfo*)luaL_checkudata(L, idx, "lhc.soundfile.stream");
	if (NULL == si)
		luaL_typerror(L, idx, "Stream");
	if (NULL == si->file)
		luaL_error(L, "Stream argument %d invalid: Stream closed.", idx);
	return si;
}

static int l_file_info(lua_State* L)
{
	FileInfo* si = l_checkfile(L, 1);

	lua_createtable(L, 0, 6);

	lua_pushinteger(L, si->pos);
	lua_setfield(L, -2, "pos");

	lua_pushinteger(L, si->info.frames);
	lua_setfield(L, -2, "frames");

	lua_pushinteger(L, si->info.samplerate);
	lua_setfield(L, -2, "samplerate");

	lua_pushinteger(L, si->info.channels);
	lua_setfield(L, -2, "channels");

	lua_pushstring(L, to_format_name(si->info.format));
	lua_setfield(L, -2, "format");

	switch (si->type) {
		case STREAM_READONCE:
			lua_pushliteral(L, "once"); break;
		case STREAM_READLOOP:
			lua_pushliteral(L, "loop"); break;
		case STREAM_WRITE:
		default:
			lua_pushliteral(L, "write"); break;
	}
	lua_setfield(L, -2, "type");

	return 1;
}

static int l_file_seek(lua_State* L)
{
	FileInfo* si = l_checkfile(L, 1);
	int pos = luaL_checknumber(L, 2);

	if (!si->info.seekable)
		return luaL_error(L, "Stream not seekable!");

	if (STREAM_READLOOP == si->type) {
		while (pos >= si->info.frames)
			pos -= si->info.frames;
		while (pos < 0)
			pos += si->info.frames;
	} else {
		if (pos >= si->info.frames)
			return luaL_error(L, "Cannot seek to position %d: out of bounds.", pos);

		if (pos < 0)
			pos += si->info.frames;
		if (pos < 0)
			return luaL_error(L, "Cannot seek to position %d: out of bounds.",
					pos - si->info.frames);
	}

	si->pos = sf_seek(si->file, pos, SEEK_SET);
	if (pos != si->pos)
		return luaL_error(L, "Could not seek to position %d: %s.", pos, sf_strerror(si->file));

	lua_settop(L, 1);
	return 1;
}

static int l_file_read(lua_State* L)
{
	FileInfo* si = l_checkfile(L, 1);
	int frames = luaL_checkinteger(L, 2);
	if (frames <= 0)
		return luaL_error(L, "Invalid number of requested frames: %d.", frames);

	if (!lua_istable(L, 3)) {
		lua_settop(L, 2);
		lua_createtable(L, frames * si->info.channels, 0);
	}

	float* buffer = malloc(frames * si->info.channels * sizeof(float));
	memset(buffer, 0, frames * si->info.channels * sizeof(float));
	sf_count_t frames_read = 0;

	if (si->pos >= si->info.frames && STREAM_READLOOP != si->type)
		frames_read = frames;
	else
		frames_read = sf_readf_float(si->file, buffer, frames);
	int i;
	for (i = 0; i < frames_read * si->info.channels; ++i) {
		lua_pushnumber(L, buffer[i]);
		lua_rawseti(L, 3, i+1);
	}

	if (frames_read < frames && STREAM_READLOOP == si->type) {
		si->pos = 0;
		sf_seek(si->file, 0, SEEK_SET);
		frames_read = sf_readf_float(si->file, buffer+i, frames-frames_read);
		for (; i < frames * si->info.channels; ++i) {
			lua_pushnumber(L, buffer[i]);
			lua_rawseti(L, 3, i+1);
		}
	}

	free(buffer);
	si->pos += frames_read;

	lua_settop(L,1);
	lua_pushinteger(L, frames_read);
	return 2;
}

static int l_file_write(lua_State* L)
{
	FileInfo* si = l_checkfile(L, 1);
	if (!lua_istable(L, 2))
		return luaL_typerror(L, 2, "table");

	size_t samples = lua_objlen(L, 2);
	if (0 != samples % si->info.channels)
		return luaL_error(L, "Invalid number of samples: %d. Need to be multiples of %d.",
				samples, si->info.channels);

	float* buffer = malloc(samples * sizeof(float));
	for (size_t i = 1; i <= samples; ++i) {
		lua_rawgeti(L, 2, i);
		buffer[i-1] = lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	sf_count_t written = sf_write_float(si->file, buffer, samples);
	free(buffer);
	si->pos += written / si->info.channels;
	si->info.frames = si->pos > si->info.frames ? si->pos : si->info.frames;

	lua_settop(L, 1);
	lua_pushinteger(L, written);
	return 2;
}

static int l_file_close(lua_State* L)
{
	FileInfo* si = l_checkfile(L, 1);
	sf_close(si->file);
	si->file = NULL;
	return 0;
}

static int l_file_index(lua_State* L)
{
	FileInfo* si = (FileInfo*)lua_touserdata(L, 1);
	assert(NULL != si);
	const char* key = lua_tostring(L, 2);

	if (0 == strcmp("pos", key)) {
		lua_pushnumber(L, si->pos);
	} else if (0 == strcmp("frames", key)) {
		lua_pushinteger(L, si->info.frames);
	} else if (0 == strcmp("samplerate", key)) {
		lua_pushinteger(L, si->info.samplerate);
	} else if (0 == strcmp("channels", key)) {
		lua_pushinteger(L, si->info.channels);
	} else if (0 == strcmp("format", key)) {
		lua_pushstring(L, to_format_name(si->info.format));
	} else if (0 == strcmp("type", key)) {
		switch (si->type) {
			case STREAM_READONCE:
				lua_pushliteral(L, "once"); break;
			case STREAM_READLOOP:
				lua_pushliteral(L, "loop"); break;
			case STREAM_WRITE:
			default:
				lua_pushliteral(L, "write"); break;
		}
	} else {
		if (0 != lua_getmetatable(L, 1))
			lua_getfield(L, -1, key);
		else /* just to be sure */
			lua_pushnil(L);
	}

	return 1;
}

static int l_file_gc(lua_State* L)
{
	FileInfo* si = (FileInfo*)lua_touserdata(L, 1);
	if (NULL == si || NULL == si->file)
		return 0;

	sf_close(si->file);
	return 0;
}

static int l_file_new(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	const char* type_str = luaL_optstring(L, 2, "once");
	StreamType type;
	if (0 == strcmp("once", type_str))
		type = STREAM_READONCE;
	else if (0 == strcmp("loop", type_str))
		type = STREAM_READLOOP;
	else if (0 == strcmp("write", type_str))
		type = STREAM_WRITE;
	else
		return luaL_typerror(L, 2, "stream type");

	SF_INFO info = {0,0,0,0,0,0};
	info.format = 0;

	int mode = SFM_READ;
	if (STREAM_WRITE == type) {
		mode = SFM_WRITE;
		info.samplerate = luaL_optint(L, 3, 44100);
		info.channels   = luaL_optint(L, 4, 1);
		int bits        = luaL_optint(L, 5, 16);
		info.format     = l_to_format_enum(L, path, bits);
	}

	SNDFILE *file = sf_open(path, mode, &info);
	if (NULL == file)
		return luaL_error(L, "Cannot open `%s' for %s: %s.", path,
				mode == SFM_READ ? "reading" : "writing", sf_strerror(NULL));

	FileInfo* si = (FileInfo*)lua_newuserdata(L, sizeof(FileInfo));
	si->info = info;
	si->file = file;
	si->type = type;
	si->pos  = 0;


	if (luaL_newmetatable(L, "lhc.soundfile.stream")) {
		lua_pushcfunction(L, l_file_info);
		lua_setfield(L, -2, "info");

		lua_pushcfunction(L, l_file_seek);
		lua_setfield(L, -2, "seek");

		lua_pushcfunction(L, l_file_read);
		lua_setfield(L, -2, "read");

		lua_pushcfunction(L, l_file_write);
		lua_setfield(L, -2, "write");

		lua_pushcfunction(L, l_file_close);
		lua_setfield(L, -2, "close");

		lua_pushcfunction(L, l_file_index);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, l_file_gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);

	return 1;
}

int luaopen_soundfile(lua_State* L)
{
	lua_createtable(L, 0, 1);
	lua_pushcfunction(L, l_file_new);
	lua_setfield(L, -2, "new");

	return 1;
}
