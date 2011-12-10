CC=clang
CFLAGS=-march=core2 --std=c99 -fomit-frame-pointer -Wall -Wextra -O2 -DNDEBUG

player.so: src/player.o src/inter_stack_tools.o
	$(CC) -march=core2 -shared -Wl,-soname,$@ -o $@ $^ -lc -lportaudio

.c.o:
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

.PHONY: clean test
clean:
	rm src/*.o player.so

test: player.so
	lua -i test.lua
