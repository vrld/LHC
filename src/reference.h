#ifndef _TYPES_H
#define _TYPES_H

typedef enum {
	REF_PLAYER = 0,
	REF_SOUNDFILE
} RefType;

static const char* reference_names[] = {
	"lhc.player.PlayerInfo",
	"lhc.soundfile.FileInfo",
};

typedef struct Reference
{
	void* ref;
	RefType type;
} Reference;

typedef struct RefHeader
{
	int refcount;
} RefHeader;

#endif
