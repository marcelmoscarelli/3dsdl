/* cube.h - public functions for cubes */
#ifndef CUBE_H
#define CUBE_H
#include <SDL2/SDL.h>
#include "data_structures.h"

void make_cube(Cube* cube, float size, Point3D center);
void rotate_cube(Cube* cube, Point3D axis, float angle);
void draw_cube(const Cube* cube);

#endif