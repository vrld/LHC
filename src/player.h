#ifndef _PLAYER_H
#define _PLAYER_H

#include <portaudio.h>

typedef struct PlayerInfo
{
	int refcount;

	int channels;
	int pos;
	double samplerate;
	PaStream *stream;
	lua_State* L;
	mutex_t lock;
} PlayerInfo;

int luaopen_player(lua_State* L);
PlayerInfo* l_checkplayer(lua_State* L, int idx);

#endif
