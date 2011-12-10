CC=clang
CFLAGS=-march=core2 --std=c99 -fomit-frame-pointer -Wall -Wextra -O2 -DNDEBUG

.PHONY: clean all

all: player.so soundfile.so

player.so: src/player.o src/inter_stack_tools.o
	$(CC) -march=core2 -shared -Wl,-soname,$@ -o $@ $^ -lc -lportaudio

soundfile.so: src/soundfile.o
	$(CC) -march=core2 -shared -Wl,-soname,$@ -o $@ $^ -lc -lsndfile

.c.o:
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

clean:
	rm src/*.o player.so soundfile.so
