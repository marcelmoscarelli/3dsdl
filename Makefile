CC = gcc
PKG_CONFIG = pkg-config

CFLAGS = -O3 -Wall -Wextra $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf)
LDFLAGS = $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf)

BINDIR = bin
SRC = main.c data_structures.c settings.c
OBJ = $(SRC:.c=.o)
TARGET = $(BINDIR)/3dsdl

all: $(TARGET)

$(TARGET): $(OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm
	rm -f $(OBJ)

$(BINDIR):
	mkdir -p $(BINDIR)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
