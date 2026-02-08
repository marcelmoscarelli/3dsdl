# 3dsdl

## Description

A basic 3D engine experiment using SDL2 for rendering and Dear ImGui for HUD elements, gorilla-style.

## Build & Run

### Linux

Get the following with your package manager (you probably already have all but one):

- gcc

- g++

- make

- pkg-config

- libsdl2-dev

Then build and run:

- `make`
- `./bin/3dsdl`

### Windows

Get the following:

- [w64devkit](https://github.com/skeeto/w64devkit) (then add its bin folder to the PATH variable, so you can access build tools from cmd or pwsh)

- [libsdl2-dev](https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.zip) (extract somewhere then add its \lib\pkgconfig folder to the PKG_CONFIG_PATH variable)

Then, finally, build and run:

- `make`
- Double-click the .exe file..?

## Controls

- WASD: movement
- Left-shift (hold): sprint
- Space: jump
- Esc: toggle cursor capture on/off

## License

WTFPL.
