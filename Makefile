CC=clang
CFLAGS=--std=c99 -Wall -Wextra -pedantic -O0 -gdwarf-2 -g3

OBJS  = src/lhc.o
OBJS += src/buffer.o
OBJS += src/player.o
OBJS += src/soundfile.o
OBJS += src/env.o
OBJS += src/osfunc_posix.o

.PHONY: clean all

all: lhc.so

lhc.so: $(OBJS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $^ -lc -lportaudio -lsndfile

.c.o:
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

clean:
	rm src/*.o lhc.so
