CC = gcc
CXX = g++
PKG_CONFIG = pkg-config

CFLAGS = -O3 -Wall -Wextra $(shell $(PKG_CONFIG) --cflags sdl2 | sed 's/-mwindows//g')
CXXFLAGS = -O3 -Wall -Wextra -std=c++11 $(shell $(PKG_CONFIG) --cflags sdl2 | sed 's/-mwindows//g')
LDFLAGS = $(shell $(PKG_CONFIG) --libs sdl2 | sed 's/-mwindows//g')

IMGUI_DIR = imgui
IMGUI_BACKENDS = $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp $(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp
IMGUI_CORE = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp

BINDIR = bin
SRC_C = main.c rendering.c data_structures.c settings.c
SRC_CPP = imgui_overlay.cpp $(IMGUI_CORE) $(IMGUI_BACKENDS)
OBJ_C = $(patsubst %.c,%.o,$(SRC_C))
OBJ_CPP = $(SRC_CPP:.cpp=.o)
TARGET = $(BINDIR)/3dsdl

all: $(TARGET)

$(TARGET): $(OBJ_C) $(OBJ_CPP) | $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lm

$(BINDIR):
	mkdir -p $(BINDIR)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -c -o $@ $<

clean:
	rm -f $(OBJ_C) $(OBJ_CPP) $(TARGET)

.PHONY: all clean
