CC = gcc
PKG_CONFIG = pkg-config

#CFLAGS = -O3 -Wall -Wextra $(shell $(PKG_CONFIG) --cflags sdl2 sdl2_ttf sdl2_image)
#LDFLAGS = $(shell $(PKG_CONFIG) --libs sdl2 sdl2_ttf sdl2_image)

CFLAGS = -O3 -Wall -Wextra $(shell $(PKG_CONFIG) --cflags sdl2)
LDFLAGS = $(shell $(PKG_CONFIG) --libs sdl2)

SRC = main.c
OBJ = $(SRC:.c=.o)
TARGET = 3D_SDL

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
