/* data_structures.h - public data structures for 3dsdl */
#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H
#include <stdlib.h>
#include <SDL2/SDL.h>

typedef struct {
    float x;
    float y;
} Point2D;

typedef struct {
    float x;
    float y;
    float z;
} Point3D;

typedef struct {
    Point3D points[8];
    SDL_Color color;
} Cube;

typedef struct {
    Cube** arr;
    size_t cur_size;
    size_t max_size;
} Cube_DArray;

typedef struct {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float focal_length;
} Camera;

// Prototypes
void init_cube_darray(Cube_DArray* darray, size_t initial_size);
void add_cube_to_darray(Cube_DArray* darray, Cube* cube);

#endif
