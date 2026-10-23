CC = gcc
CFLAGS = -Wall -Wextra -std=c99 $(shell pkg-config --cflags sdl2 lua)
LDFLAGS = $(shell pkg-config --libs sdl2 lua) -lm

sdl-lua-demo: main.c
	$(CC) $(CFLAGS) -o sdl-lua-demo main.c $(LDFLAGS)

run: sdl-lua-demo
	./sdl-lua-demo

clean:
	rm -f sdl-lua-demo
