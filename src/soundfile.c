#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

typedef struct FileInfo {
	SF_INFO info;
	SNDFILE *handle;
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

static int l_fileinfo(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	SF_INFO info;
	info.format = 0;
	SNDFILE *file = sf_open(path, SFM_READ, &info);
	if (NULL == file)
		return luaL_error(L, "Cannot open file `%s' for reading: %s.", path, sf_strerror(NULL));
	sf_close(file);

	lua_pushnumber(L, info.frames);
	lua_pushnumber(L, info.samplerate);
	lua_pushnumber(L, info.channels);
	lua_pushstring(L, to_format_name(info.format));
	return 4;
}

static const int READ_ALL = 0;
static int l_read(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	int pos = luaL_optint(L, 2, 0);
	int frames = luaL_optint(L, 3, READ_ALL);

	SF_INFO info;
	info.format = 0;
	SNDFILE *file = sf_open(path, SFM_READ, &info);

	if (READ_ALL == frames)
		frames = info.frames;

	if (!lua_istable(L, 4)) {
		lua_settop(L, 3);
		lua_createtable(L, frames, 0);
	}

	sf_count_t frames_read;
	float* buffer = malloc(frames * info.channels * sizeof(float));
	if (-1 == sf_seek(file, pos, SEEK_SET))
		frames_read = 0;
	else
		frames_read = sf_readf_float(file, buffer, frames);
	sf_close(file);

	for (int i = 0; i < frames_read*info.channels; ++i) {
		lua_pushnumber(L, buffer[i]);
		lua_rawseti(L, 4, i+1);
	}

	// append silence
	for (int i = frames_read*info.channels; i < frames*info.channels; ++i) {
		lua_pushnumber(L, 0);
		lua_rawseti(L, 4, i+1);
	}

	free(buffer);
	return 1;
}

static int l_write(lua_State* L)
{
	const char* path = luaL_checkstring(L, 1);
	if (!lua_istable(L, 2))
		return luaL_typerror(L, 2, "table");
	sf_count_t samples = lua_objlen(L, 2);

	int rate     = luaL_optint(L, 3, 44100);
	int channels = luaL_optint(L, 4, 1);
	int bits     = luaL_optint(L, 5, 16);

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


	float* out = malloc(samples * sizeof(float));
	for (sf_count_t i = 1; i <= samples; ++i) {
		lua_rawgeti(L, 2, i);
		out[i-1] = lua_tonumber(L, -1);
		lua_pop(L,1);
	}

	SF_INFO info = {0,0,0,0,0,0};
	info.channels = channels;
	info.samplerate = rate;
	info.format = format;

	SNDFILE *file = sf_open(path, SFM_WRITE, &info);
	if (NULL == file) {
		free(out);
		return luaL_error(L, "Cannot open `%s' for writing: %s.", path, sf_strerror(NULL));
	}

	sf_count_t written = sf_write_float(file, out, samples);
	free(out);
	sf_close(file);

	if (written != samples)
		return luaL_error(L, "Could not write all samples (%d requested, %d written).", samples, written);

	return 0;
}

int luaopen_soundfile(lua_State* L)
{
	lua_createtable(L, 0, 3);
	lua_pushcfunction(L, l_fileinfo);
	lua_setfield(L, -2, "info");
	lua_pushcfunction(L, l_read);
	lua_setfield(L, -2, "read");
	lua_pushcfunction(L, l_write);
	lua_setfield(L, -2, "write");

	return 1;
}
