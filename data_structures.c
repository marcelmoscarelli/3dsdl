#include <stdio.h>
#include <stdlib.h>
#include "data_structures.h"

void init_cube_darray(Cube_DArray* darray, size_t initial_size) {
    if (!darray) {
        printf("init_cube_darray(): NULL darray pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }
    darray->arr = (Cube**)malloc(initial_size * sizeof(Cube*));
    darray->cur_size = 0;
    darray->max_size = initial_size;
}

void add_cube_to_darray(Cube_DArray* darray, Cube* cube) {
    if (!darray || !cube) {
        printf("add_cube_to_darray(): NULL pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }
    if (darray->cur_size >= darray->max_size) { // Resize to twice the current size
        size_t new_max_size = darray->max_size * 2;
        Cube** new_arr = (Cube**)realloc(darray->arr, new_max_size * sizeof(Cube*));
        if (!new_arr) {
            printf("add_cube_to_darray(): Memory reallocation failed. Exiting!\n");
            exit(EXIT_FAILURE);
        }
        darray->arr = new_arr;
        darray->max_size = new_max_size;
    }
    darray->arr[darray->cur_size] = cube;
    darray->cur_size++;
}