all: build run
build:
	gcc -o chip8 chip8.c `sdl2-config --cflags --libs` -lSDL2_mixer 
run:
	./chip8
