#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "cube.h"
#include "data_structures.h"

// Hashing for Cube_key
static uint64_t cube_key_hash(Cube_Key key) {
    uint64_t x = (uint32_t)key.x;
    uint64_t y = (uint32_t)key.y;
    uint64_t z = (uint32_t)key.z;
    uint64_t h = x * 0x9E3779B185EBCA87ULL;
    h ^= y * 0xC2B2AE3D27D4EB4FULL;
    h ^= z * 0x165667B19E3779F9ULL;
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 33;
    return h;
}

// Simple equality check for Cube_Key
static bool cube_key_equals(Cube_Key a, Cube_Key b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

// Simple power-of-2 rounding function for hash map capacity
static size_t next_pow2(size_t v) {
    if (v < 8) { 
        return 8;
    }
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    if (sizeof(size_t) >= 8) {
        v |= v >> 32;
    }
    return v + 1;
}

// Rehash the map to a new capacity (must be power of 2)
static void cube_map_rehash(Cube_Map* map, size_t new_capacity) {
    Cube_Map_Entry* old_entries = map->entries;
    size_t old_capacity = map->capacity;

    map->entries = (Cube_Map_Entry*)calloc(new_capacity, sizeof(Cube_Map_Entry));
    map->capacity = new_capacity;
    map->size = 0;

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_entries[i].occupied) {
            cube_map_add(map, old_entries[i].key, old_entries[i].cube);
        }
    }

    free(old_entries);
}

// Initialize the cube map with a given initial capacity (will be rounded up to next power of 2)
void init_cube_map(Cube_Map* map, size_t initial_capacity) {
    if (!map) {
        printf("init_cube_map(): NULL map pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }
    size_t cap = next_pow2(initial_capacity);
    map->entries = (Cube_Map_Entry*)calloc(cap, sizeof(Cube_Map_Entry));
    map->capacity = cap;
    map->size = 0;
}

// Free the cube map's internal resources (also frees cubes)
void free_cube_map(Cube_Map* map) {
    if (!map) {
        return;
    }
    if (map->entries) {
        for (size_t i = 0; i < map->capacity; ++i) {
            if (map->entries[i].occupied && map->entries[i].cube) {
                free(map->entries[i].cube);
                map->entries[i].cube = NULL;
            }
        }
    }
    free(map->entries);
    map->entries = NULL;
    map->capacity = 0;
    map->size = 0;
}

// Add a cube in the map with the given key
bool cube_map_add(Cube_Map* map, Cube_Key key, Cube* cube) {
    if (!map || !cube) {
        printf("cube_map_add(): NULL pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }

    // Load factor > 0.7 triggers resize
    if ((map->size + 1) * 10 >= map->capacity * 7) {
        cube_map_rehash(map, map->capacity * 2);
    }

    uint64_t hash = cube_key_hash(key);
    size_t mask = map->capacity - 1; 
    size_t index = (size_t)hash & mask;
    size_t first_tombstone = (size_t)-1;

    for (;;) {
        Cube_Map_Entry* entry = &map->entries[index];
        if (!entry->occupied) {
            if (entry->tombstone && first_tombstone == (size_t)-1) {
                first_tombstone = index;
            } else if (!entry->tombstone) {
                size_t target = (first_tombstone != (size_t)-1) ? first_tombstone : index;
                map->entries[target].key = key;
                map->entries[target].cube = cube;
                map->entries[target].occupied = true;
                map->entries[target].tombstone = false;
                map->size++;
                return true;
            }
        } else if (cube_key_equals(entry->key, key)) {
            entry->cube = cube;
            return true;
        }
        index = (index + 1) & mask;
    }
}

// Retrieve a cube from the map by its key (NULL if not found)
Cube* cube_map_get(const Cube_Map* map, Cube_Key key) {
    if (!map || map->capacity == 0) {
        return NULL;
    }
    uint64_t hash = cube_key_hash(key);
    size_t mask = map->capacity - 1;
    size_t index = (size_t)hash & mask;

    while(1) {
        const Cube_Map_Entry* entry = &map->entries[index];
        if (!entry->occupied) {
            if (!entry->tombstone) {
                return NULL;
            }
        } else if (cube_key_equals(entry->key, key)) {
            return entry->cube;
        }
        index = (index + 1) & mask;
    }
}

// Remove a cube from the map by its key (returns true if removed, false if not found)
bool cube_map_remove(Cube_Map* map, Cube_Key key) {
    if (!map || map->capacity == 0) {
        return false;
    }
    uint64_t hash = cube_key_hash(key);
    size_t mask = map->capacity - 1;
    size_t index = (size_t)hash & mask;

    while(1) {
        Cube_Map_Entry* entry = &map->entries[index];
        if (!entry->occupied) {
            if (!entry->tombstone) {
                return false;
            }
        } else if (cube_key_equals(entry->key, key)) {
            entry->occupied = false;
            entry->tombstone = true;
            if (entry->cube) {
                free(entry->cube);
                entry->cube = NULL;
            }
            map->size--;
            return true;
        }
        index = (index + 1) & mask;
    }
}

size_t cube_map_capacity(const Cube_Map* map) {
    return map ? map->capacity : 0;
}

// Retrieve a pointer to the entry at the given index (NULL if out of bounds)
const Cube_Map_Entry* cube_map_entry_at(const Cube_Map* map, size_t index) {
    if (!map || index >= map->capacity) {
        return NULL;
    }
    return &map->entries[index];
}