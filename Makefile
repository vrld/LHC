CC=clang
CFLAGS=--std=c99 -Wall -Wextra -pedantic -O0 -gdwarf-2 -g3

.PHONY: clean all

all: lhc.so

lhc.so: src/lhc.o src/buffer.o src/player.o src/soundfile.o
	$(CC) -march=core2 -shared -Wl,-soname,$@ -o $@ $^ -lc -lportaudio -lsndfile

.c.o:
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

clean:
	rm src/*.o lhc.so
