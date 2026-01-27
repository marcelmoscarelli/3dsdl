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

// Function prototypes
bool init();

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

        // Rendering code would go here

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