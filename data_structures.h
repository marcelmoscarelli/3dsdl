/* data_structures.h - public data structures for 3dsdl */
#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

// Camera structure
typedef struct {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
    float focal_length;
} Camera;

// Stores a vertex in camera space (x,y,z) after applying camera translation + yaw/pitch rotation, but before projection.
// z is kept here for near-plane clipping and depth sorting (projection to screen happens later).
typedef struct {
    float x;
    float y;
    float z;
} Camera_Point;

// Stores a 2D point on the screen after projection.
typedef struct {
    float x;
    float y;
} Projected_Point;

// A face to be rendered, with its vertices in screen space, depth for sorting, and color.
typedef struct {
    SDL_Vertex verts[12];
    size_t vert_count;
    float depth;
    Projected_Point line_pts[6];
    size_t line_count;
    SDL_Color color;
} Render_Face;

// 3D point structure.
typedef struct {
    float x;
    float y;
    float z;
} Point_3D;

// A cube defined by its 8 corner points and color.
typedef struct {
    Point_3D points[8];
    SDL_Color color;
} Cube;

// Axis-aligned bounding box (AABB) for testing player collision.
typedef struct {
    float min_x;
    float min_y;
    float min_z;
    float max_x;
    float max_y;
    float max_z;
} AABB;

// Key for identifying cubes in the hash map.
typedef struct {
    int x;
    int y;
    int z;
} Cube_Key;

// Entry in the cube hash map.
typedef struct {
    Cube_Key key;
    Cube* cube;
    bool occupied;
    bool tombstone;
} Cube_Map_Entry;

// Hash map for storing cubes by their keys.
typedef struct {
    Cube_Map_Entry* entries;
    size_t capacity;
    size_t size;
} Cube_Map;

// Prototypes
void make_cube(Cube* cube, float size, Point_3D center, SDL_Color color);
int world_to_grid_coord(float world, float step, float offset);
int world_to_grid_index_floor(float world, float step, float offset);
AABB player_aabb(float px, float py, float pz, float radius, float height, float eye_height);
bool aabb_intersects_map(const Cube_Map* map, AABB box, float step, float offset_x, float offset_y, float offset_z);
void init_cube_map(Cube_Map* map, size_t initial_capacity);
void free_cube_map(Cube_Map* map);
bool cube_map_add(Cube_Map* map, Cube_Key key, Cube* cube);
Cube* cube_map_get(const Cube_Map* map, Cube_Key key);
bool cube_map_remove(Cube_Map* map, Cube_Key key);
size_t cube_map_capacity(const Cube_Map* map);
const Cube_Map_Entry* cube_map_entry_at(const Cube_Map* map, size_t index);

#endif
