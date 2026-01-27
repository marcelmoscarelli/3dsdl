#include <SDL2/SDL.h>
#include <stdio.h>   // fprintf, stderr
#include <stdlib.h>  // EXIT_FAILURE, EXIT_SUCCESS
#include <stdbool.h> // bool type
#include <math.h>    // tanf

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

typedef struct {
    float x;
    float y;
    float z;
    float yaw;
    float pitch;
} Camera;

// Global consts for window dimensions and frame rate
static const int WIDTH = 1000;
static const int HEIGHT = 1000;
static const int TARGET_FPS = 60;
static const Uint32 FRAME_DELAY = (1000u / TARGET_FPS);

// Global for camera
static const float ASPECT_RATIO = (float)WIDTH / (float)HEIGHT;
static const float FOV_STEP = 1.0f;
static const float FOV_MIN = 30.0f;
static const float FOV_MAX = 120.0f;
static const float PITCH_MAX = 89.0f;
static const float PITCH_MIN = -89.0f;
float camera_f = -0.15611995216165922; // Initial focal length (FOV=60 degrees)
Camera camera_pos = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

// Global variables for SDL window and renderer
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

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
    const float SPACING = 1.0f;
    const float DEPTH_Z = 15.0f;

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

    // Camera parameters
    float fov = 60.0f;                      // Field of view in degrees
    float fov_rad = fov * (M_PI / 180.0f);  // Convert to radians
    camera_f = 1.0f / tanf(fov_rad * 0.5f); // Focal length of the camera
    // Enable relative mouse mode for FPS-style look
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_ShowCursor(SDL_DISABLE);

    // Main loop
    bool running = true;
    SDL_Event event;
    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        // Event handling
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEWHEEL: // Adjust FOV with mouse wheel
                    if (event.wheel.y > 0) {
                        fov -= FOV_STEP;
                    } else if (event.wheel.y < 0) {
                        fov += FOV_STEP;
                    }
                    if (fov < FOV_MIN) fov = FOV_MIN;
                    if (fov > FOV_MAX) fov = FOV_MAX;
                    printf("FOV: %f degrees\n", fov);
                    fov_rad = fov * (M_PI / 180.0f);
                    camera_f = 1.0f / tanf(fov_rad * 0.5f);
                    break;
                case SDL_MOUSEMOTION: {
                    // Update camera orientation from mouse movement
                    const float mouse_sens = 0.1f; // degrees per pixel
                    camera_pos.yaw += event.motion.xrel * mouse_sens;
                    camera_pos.pitch += event.motion.yrel * mouse_sens;
                    if (camera_pos.pitch > PITCH_MAX) camera_pos.pitch = PITCH_MAX;
                    if (camera_pos.pitch < PITCH_MIN) camera_pos.pitch = PITCH_MIN;
                    break;
                }
                default:
                    break;
            }
        }

        // WASD controls now move the camera (first-person)
        const Uint8* keystate = SDL_GetKeyboardState(NULL);
        float camera_step = 0.15f;
        // W = forward (toward +z), S = backward
        if (keystate[SDL_SCANCODE_W]) {
            camera_pos.z += camera_step;
        }
        if (keystate[SDL_SCANCODE_S]) {
            camera_pos.z -= camera_step;
        }
        // A = left, D = right
        if (keystate[SDL_SCANCODE_A]) {
            camera_pos.x -= camera_step;
        }
        if (keystate[SDL_SCANCODE_D]) {
            camera_pos.x += camera_step;
        }
        // Q = up, E = down
        if (keystate[SDL_SCANCODE_Q]) {
            camera_pos.y += camera_step;
        }
        if (keystate[SDL_SCANCODE_E]) {
            camera_pos.y -= camera_step;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        // Draw all cubes in the array
        for (size_t ci = 0; ci < cube_arr.size; ++ci) {
            draw_cube(&cube_arr.arr[ci]);
        }

        // Draw static crosshair in the center of the screen
        int cx = WIDTH / 2;
        int cy = HEIGHT / 2;
        int half = 8; // crosshair half-length
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, cx - half, cy, cx + half, cy);
        SDL_RenderDrawLine(renderer, cx, cy - half, cx, cy + half);

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
    bool skip_point[8] = { false };
    // Initialize projected points to safe defaults
    for (size_t i = 0; i < 8; ++i) {
        projected[i].x = 0;
        projected[i].y = 0;
    }
    // Precompute camera rotation
    float yaw_rad = camera_pos.yaw * (M_PI / 180.0f);
    float pitch_rad = camera_pos.pitch * (M_PI / 180.0f);
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    float cos_pitch = cosf(pitch_rad);
    float sin_pitch = sinf(pitch_rad);

    for (size_t i = 0; i < 8; ++i) {
        // World -> camera space: translate then rotate by inverse camera yaw/pitch
        float rel_x = cube->points[i].x - camera_pos.x;
        float rel_y = cube->points[i].y - camera_pos.y;
        float rel_z = cube->points[i].z - camera_pos.z;

        // Apply inverse yaw rotation (rotate by -yaw)
        float x1 =  cos_yaw * rel_x - sin_yaw * rel_z;
        float z1 =  sin_yaw * rel_x + cos_yaw * rel_z;

        // Apply inverse pitch rotation (rotate by -pitch)
        float y2 =  cos_pitch * rel_y + sin_pitch * z1;
        float z2 = -sin_pitch * rel_y + cos_pitch * z1;

        if (z2 <= 0) { // Behind camera
            skip_point[i] = true;
            continue;
        }

        float x_ndc = (x1 * camera_f / ASPECT_RATIO) / z2;
        float y_ndc = (y2 * camera_f) / z2;
        projected[i].x = (int)SDL_roundf((x_ndc + 1.0f) * 0.5f * WIDTH);
        projected[i].y = (int)SDL_roundf((1.0f - (y_ndc + 1.0f) * 0.5f) * HEIGHT);
    }

    for (size_t i = 0; i < 4; ++i) {
        int next_i = (i + 1) % 4;
        // Front face: draw only if both endpoints are in front of camera
        if (!skip_point[i] && !skip_point[next_i]) {
            SDL_RenderDrawLine(renderer, (int)projected[i].x, (int)projected[i].y, (int)projected[next_i].x, (int)projected[next_i].y);
        }
        // Back face
        if (!skip_point[i + 4] && !skip_point[next_i + 4]) {
            SDL_RenderDrawLine(renderer, (int)projected[i + 4].x, (int)projected[i + 4].y, (int)projected[next_i + 4].x, (int)projected[next_i + 4].y);
        }
        // Connect front and back faces
        if (!skip_point[i] && !skip_point[i + 4]) {
            SDL_RenderDrawLine(renderer, (int)projected[i].x, (int)projected[i].y, (int)projected[i + 4].x, (int)projected[i + 4].y);
        }
    }
}