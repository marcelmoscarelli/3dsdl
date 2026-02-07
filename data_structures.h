/* data_structures.h - public data structures for 3dsdl */
#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H
#include <stdlib.h>
#include <stdbool.h>
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
    int x;
    int y;
    int z;
} Cube_Key;

typedef struct {
    Cube_Key key;
    Cube* cube;
    bool occupied;
    bool tombstone;
} Cube_Map_Entry;

typedef struct {
    Cube_Map_Entry* entries;
    size_t capacity;
    size_t size;
} Cube_Map;

typedef struct {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float focal_length;
} Camera;

// Prototypes
void init_cube_map(Cube_Map* map, size_t initial_capacity);
void free_cube_map(Cube_Map* map);
bool cube_map_add(Cube_Map* map, Cube_Key key, Cube* cube);
Cube* cube_map_get(const Cube_Map* map, Cube_Key key);
bool cube_map_remove(Cube_Map* map, Cube_Key key);
size_t cube_map_capacity(const Cube_Map* map);
const Cube_Map_Entry* cube_map_entry_at(const Cube_Map* map, size_t index);

#endif
