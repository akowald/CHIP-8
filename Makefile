SDL_PATH := ../SDL2-2.0.4/include
TCLAP_PATH := ../tclap-1.2.1/include
CC = gcc
OUTPUT = chip8

DEBUG_FLAGS := -g -O0 -fbuiltin
CFLAGS := -I. -I$(SDL_PATH) -I$(TCLAP_PATH) -std=c++11
#CFLAGS += $(DEBUG_FLAGS)


LFLAGS := -lstdc++ -lm -lSDL2

OBJS := $(shell find . -follow -type f -name '*.cpp')

all : $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LFLAGS) -o $(OUTPUT)

clean:
	rm -f $(OUTPUT)
