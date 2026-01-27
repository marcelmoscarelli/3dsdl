#include <SDL2/SDL.h>
#include <stdio.h>   // fprintf, stderr
#include <stdlib.h>  // EXIT_FAILURE, EXIT_SUCCESS
#include <stdbool.h> // bool type

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Macros for window dimensions and frame rate
static const int WIDTH = 1000;
static const int HEIGHT = 1000;
static const int TARGET_FPS = 120;
static const Uint32 FRAME_DELAY = (1000u / TARGET_FPS);

// Global variables for SDL window and renderer
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

// Data structures
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
} Cube;

typedef struct {
    Cube arr[100];
    size_t size;
} CubeArray;

// Function prototypes
bool init();
void make_cube(Cube* cube, float size, Point3D center);
void draw_cube(const Cube* cube);

// Main function
int main(int argc, char** argv) {
    // Avoid -Wunused-parameter warnings
    (void)argc;
    (void)argv;

    // Initialize SDL
    if (!init()) {
        fprintf(stderr, "Failed to initialize SDL. Exiting!\n");
        return EXIT_FAILURE;
    }

    // Grid parameters
    const int GRID_W = 5;
    const int GRID_H = 5;
    const float CUBE_SIZE = 1.5f;
    const float SPACING = 1.0f; // space between cubes
    const float DEPTH_Z = 10.0f; // place all cubes at z = 10

    CubeArray cube_arr = {.size = 0};
    const int total = GRID_W * GRID_H;
    for (int i = 0; i < total; ++i) {
        int gx = i % GRID_W;
        int gy = i / GRID_W;

        float step = CUBE_SIZE + SPACING; // center-to-center distance
        float offset_x = ((GRID_W - 1) * step) / 2.0f;
        float offset_y = ((GRID_H - 1) * step) / 2.0f;

        Point3D center = {
            .x = gx * step - offset_x,
            .y = gy * step - offset_y,
            .z = DEPTH_Z
        };

        make_cube(&cube_arr.arr[cube_arr.size], CUBE_SIZE, center);
        cube_arr.size++;
    }

    // Main loop
    bool running = true;
    SDL_Event event;
    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        // Draw all cubes in the array
        for (size_t ci = 0; ci < cube_arr.size; ++ci) {
            draw_cube(&cube_arr.arr[ci]);
        }

        SDL_RenderPresent(renderer);

        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}

bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("3D SDL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    return true;
}

void make_cube(Cube* cube, float size, Point3D center) {
    if (!cube) {
        fprintf(stderr, "make_cube(): NULL cube pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }
    float half_size = size / 2.0f;

    // Front face
    cube->points[0] = (Point3D){ center.x - half_size, center.y - half_size, center.z - half_size }; // A
    cube->points[1] = (Point3D){ center.x + half_size, center.y - half_size, center.z - half_size }; // B
    cube->points[2] = (Point3D){ center.x + half_size, center.y + half_size, center.z - half_size }; // C
    cube->points[3] = (Point3D){ center.x - half_size, center.y + half_size, center.z - half_size }; // D

    // Back face
    cube->points[4] = (Point3D){ center.x - half_size, center.y - half_size, center.z + half_size }; // E
    cube->points[5] = (Point3D){ center.x + half_size, center.y - half_size, center.z + half_size }; // F
    cube->points[6] = (Point3D){ center.x + half_size, center.y + half_size, center.z + half_size }; // G
    cube->points[7] = (Point3D){ center.x - half_size, center.y + half_size, center.z + half_size }; // H
}

void draw_cube(const Cube* cube) {
    if (!cube) {
        fprintf(stderr, "draw_cube(): NULL cube pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }

    Point2D projected[8];
    for (size_t i = 0; i < 8; ++i) {
        // Simple perspective projection
        if (cube->points[i].z <= 0) continue; // Avoid division by zero or negative z
        float x = cube->points[i].x / cube->points[i].z;
        float y = cube->points[i].y / cube->points[i].z;

        // Convert to screen coordinates
        projected[i].x = (int)SDL_roundf((x + 1.0f) * 0.5f * WIDTH);
        projected[i].y = (int)SDL_roundf((1.0f - (y + 1.0f) * 0.5f) * HEIGHT);
    }

    for (size_t i = 0; i < 4; ++i) {
        int next_i = (i + 1) % 4;

        // Front face
        SDL_RenderDrawLine(renderer, (int)projected[i].x, (int)projected[i].y, (int)projected[next_i].x, (int)projected[next_i].y);
        
        // Back face
        SDL_RenderDrawLine(renderer, (int)projected[i + 4].x, (int)projected[i + 4].y, (int)projected[next_i + 4].x, (int)projected[next_i + 4].y);
        
        // Connect front and back faces
        SDL_RenderDrawLine(renderer, (int)projected[i].x, (int)projected[i].y, (int)projected[i + 4].x, (int)projected[i + 4].y);
    }
}