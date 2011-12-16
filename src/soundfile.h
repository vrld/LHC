#ifndef _SOUNDFILE_H
#define _SOUNDFILE_H

#include <sndfile.h>

typedef enum {
	STREAM_READONCE,
	STREAM_READLOOP,
	STREAM_WRITE
} StreamType;

typedef struct FileInfo
{
	int refcount;

	SF_INFO info;
	SNDFILE *file;
	StreamType type;
	sf_count_t pos;
} FileInfo;

static FileInfo* l_checkfile(lua_State* L, int idx);
int luaopen_soundfile(lua_State* L);

#endif
