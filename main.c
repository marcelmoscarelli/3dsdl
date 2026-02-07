/* TODO
- Keep cube grid map as authoritative data.
- Build a render list each frame (visible cube faces only).
- Sort that list by depth (camera space Z).
- Rasterize triangles.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "cube.h"
#include "data_structures.h"
#include "settings.h"
#include "imgui_overlay.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Camera camera = {0.0f, 2.0f, 0.0f, 0.0f, 0.0f, -0.15611995216165922};

// Globals for SDL
SDL_Renderer* renderer = NULL;
static SDL_Window* window = NULL;

// Function prototypes
bool init();
void draw_crosshair(int thickness, int size);
void draw_overlay(float dt, float fov_display, float move_speed);

static int world_to_grid_coord(float world, float step, float offset) {
    return (int)lroundf((world + offset) / step);
}

static int world_to_grid_index_floor(float world, float step, float offset) {
    float half = step * 0.5f;
    return (int)floorf((world + offset + half) / step);
}

typedef struct {
    float min_x;
    float min_y;
    float min_z;
    float max_x;
    float max_y;
    float max_z;
} AABB;

static AABB player_aabb(float px, float py, float pz, float radius, float height, float eye_height) {
    float min_y = py - eye_height;
    AABB box = {
        .min_x = px - radius,
        .max_x = px + radius,
        .min_y = min_y,
        .max_y = min_y + height,
        .min_z = pz - radius,
        .max_z = pz + radius
    };
    return box;
}

static bool aabb_intersects_map(const Cube_Map* map, AABB box, float step, float offset_x, float offset_y, float offset_z) {
    int min_x = world_to_grid_index_floor(box.min_x, step, offset_x);
    int max_x = world_to_grid_index_floor(box.max_x, step, offset_x);
    int min_y = world_to_grid_index_floor(box.min_y, step, offset_y);
    int max_y = world_to_grid_index_floor(box.max_y, step, offset_y);
    int min_z = world_to_grid_index_floor(box.min_z, step, offset_z);
    int max_z = world_to_grid_index_floor(box.max_z, step, offset_z);

    for (int gx = min_x; gx <= max_x; ++gx) {
        for (int gy = min_y; gy <= max_y; ++gy) {
            for (int gz = min_z; gz <= max_z; ++gz) {
                Cube_Key key = { .x = gx, .y = gy, .z = gz };
                if (cube_map_get(map, key)) {
                    return true;
                }
            }
        }
    }
    return false;
}

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

    // Initialize a hashmap for cubes
    Cube_Map cubes;
    init_cube_map(&cubes, 2048);
    
    // Ground grid parameters (just as an example until maps start being used)
    const int GROUND_W = 5;
    const int GROUND_D = 5;
    const int GROUND_N = GROUND_W * GROUND_D;
    const float GRID_OFFSET_X = ((GROUND_W - 1) * CUBE_SIZE) / 2.0f;
    const float GRID_OFFSET_Z = ((GROUND_D - 1) * CUBE_SIZE) / 2.0f;
    const float GRID_OFFSET_Y = CUBE_SIZE * 0.5f;
    for (int i = 0; i < GROUND_N; ++i) {
        int gx = i % GROUND_W;
        int gz = i / GROUND_W;

        // Centered in X and Z, Y is half-size below origin
        Point3D center = {
            .x = gx * CUBE_SIZE - GRID_OFFSET_X,
            .y = -GRID_OFFSET_Y,
            .z = gz * CUBE_SIZE - GRID_OFFSET_Z
        };
        Cube* new_cube = (Cube*)malloc(sizeof(Cube));
        make_cube(new_cube, CUBE_SIZE, center, (SDL_Color){0, 255, 0, 255});

        Cube_Key key = {
            .x = world_to_grid_coord(center.x, CUBE_SIZE, GRID_OFFSET_X),
            .y = world_to_grid_coord(center.y, CUBE_SIZE, GRID_OFFSET_Y),
            .z = world_to_grid_coord(center.z, CUBE_SIZE, GRID_OFFSET_Z)
        };
        cube_map_add(&cubes, key, new_cube);
    }
    
    for (int i = 0; i < GROUND_N; ++i) {
        int gx = i % GROUND_W;
        int gz = i / GROUND_W;

        // Centered in X and Z, Y is half-size below origin
        Point3D center = {
            .x = gx * CUBE_SIZE - GRID_OFFSET_X,
            .y = -2 * CUBE_SIZE - GRID_OFFSET_Y,
            .z = GROUND_D * CUBE_SIZE + gz * CUBE_SIZE - GRID_OFFSET_Z
        };
        Cube* new_cube = (Cube*)malloc(sizeof(Cube));
        make_cube(new_cube, CUBE_SIZE, center, (SDL_Color){255, 255, 0, 255});

        Cube_Key key = {
            .x = world_to_grid_coord(center.x, CUBE_SIZE, GRID_OFFSET_X),
            .y = world_to_grid_coord(center.y, CUBE_SIZE, GRID_OFFSET_Y),
            .z = world_to_grid_coord(center.z, CUBE_SIZE, GRID_OFFSET_Z)
        };
        cube_map_add(&cubes, key, new_cube);
    }

    for (int i = 0; i < GROUND_N; ++i) {
        int gx = i % GROUND_W;
        int gz = i / GROUND_W;

        // Centered in X and Z, Y is half-size below origin
        Point3D center = {
            .x = GROUND_W * CUBE_SIZE + gx * CUBE_SIZE - GRID_OFFSET_X,
            .y = 2 * CUBE_SIZE - GRID_OFFSET_Y,
            .z = gz * CUBE_SIZE - GRID_OFFSET_Z
        };
        Cube* new_cube = (Cube*)malloc(sizeof(Cube));
        make_cube(new_cube, CUBE_SIZE, center, (SDL_Color){0, 255, 255, 255});

        Cube_Key key = {
            .x = world_to_grid_coord(center.x, CUBE_SIZE, GRID_OFFSET_X),
            .y = world_to_grid_coord(center.y, CUBE_SIZE, GRID_OFFSET_Y),
            .z = world_to_grid_coord(center.z, CUBE_SIZE, GRID_OFFSET_Z)
        };
        cube_map_add(&cubes, key, new_cube);
    }

    for (int i = 0; i < GROUND_N; ++i) {
        int gx = i % GROUND_W;
        int gz = i / GROUND_W;

        // Centered in X and Z, Y is half-size below origin
        Point3D center = {
            .x = GROUND_W * CUBE_SIZE + gx * CUBE_SIZE - GRID_OFFSET_X,
            .y = 4 * CUBE_SIZE - GRID_OFFSET_Y,
            .z = GROUND_D * CUBE_SIZE + gz * CUBE_SIZE - GRID_OFFSET_Z
        };
        Cube* new_cube = (Cube*)malloc(sizeof(Cube));
        make_cube(new_cube, CUBE_SIZE, center, (SDL_Color){255, 0, 255, 255});

        Cube_Key key = {
            .x = world_to_grid_coord(center.x, CUBE_SIZE, GRID_OFFSET_X),
            .y = world_to_grid_coord(center.y, CUBE_SIZE, GRID_OFFSET_Y),
            .z = world_to_grid_coord(center.z, CUBE_SIZE, GRID_OFFSET_Z)
        };
        cube_map_add(&cubes, key, new_cube);
    }

    // Camera parameters
    float fov_base = 60.0f;                  // Base FOV controlled by mouse wheel
    float fov_display = 60.0f;               // Actual FOV used for rendering (can be interpolated during sprint)
    float fov_rad = fov_display * (M_PI / 180.0f);  // Convert to radians
    camera.focal_length = 1.0f / tanf(fov_rad * 0.5f); // Focal length of the camera
    // Enable relative mouse mode for FPS-style look
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_ShowCursor(SDL_DISABLE);

    // Vertical movement (jump / fall) parameters
    bool is_grounded = true;
    float vertical_velocity = 0.0f; // positive = up

    // Walking (bob) parameters
    float walk_phase = 0.0f;
    float walk_amp = 0.0f; // current amplitude
    const float WALK_AMPLITUDE = 0.15f; // meters
    const float WALK_FREQUENCY = 2.25f; // steps per second (base)
    float walk_frequency_current = WALK_FREQUENCY; // may be increased while sprinting
    const float WALK_SMOOTH = 8.0f; // smoothing rate

    // Main loop
    bool running = true;
    SDL_Event event;
    Uint32 last_ticks = SDL_GetTicks();
    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        // Event handling
        while (SDL_PollEvent(&event)) {
                // Give ImGui a chance to handle events first
                overlay_process_event(&event);
                switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEWHEEL: // Adjust base FOV with mouse wheel
                    if (event.wheel.y > 0) {
                        fov_base -= FOV_STEP;
                    } else if (event.wheel.y < 0) {
                        fov_base += FOV_STEP;
                    }
                    if (fov_base < FOV_MIN) fov_base = FOV_MIN;
                    if (fov_base > FOV_MAX) fov_base = FOV_MAX;
                    printf("Base FOV set to: %f degrees\n", fov_base);
                    break;
                case SDL_MOUSEMOTION: {
                    // Update camera orientation from mouse movement
                    const float mouse_sens = 0.1f; // degrees per pixel
                    camera.yaw += event.motion.xrel * mouse_sens;
                    if (camera.yaw > 360.0f) {
                        camera.yaw -= 360.0f;
                    } else if (camera.yaw < 0.0f) {
                        camera.yaw += 360.0f;
                    }
                    // Standard Y (not inverted): moving mouse up decreases yrel, so add it
                    camera.pitch += event.motion.yrel * mouse_sens;
                    if (camera.pitch > PITCH_MAX) camera.pitch = PITCH_MAX;
                    if (camera.pitch < PITCH_MIN) camera.pitch = PITCH_MIN;
                    break;
                }
                default:
                    break;
            }
        }

        // WASD controls now move the camera relative to view (crosshair)
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) / 1000.0f; // delta time in seconds
        last_ticks = now;

        const Uint8* keystate = SDL_GetKeyboardState(NULL);
        const float move_speed_base = 5.0f; // units per second (base)
        const float sprint_multiplier = 2.0f; // how much faster when sprinting
        bool sprint = (keystate[SDL_SCANCODE_LSHIFT] && keystate[SDL_SCANCODE_W]);
        float move_speed = move_speed_base * (sprint ? sprint_multiplier : 1.0f);
        // increase walk frequency a bit while sprinting
        walk_frequency_current = sprint ? (WALK_FREQUENCY * 1.4f) : WALK_FREQUENCY;
        // Smoothly interpolate FOV display towards sprint FOV when sprinting
        const float SPRINT_FOV = (fov_base + 20.0f > FOV_MAX) ? FOV_MAX : (fov_base + 20.0f);
        float fov_target = sprint ? SPRINT_FOV : fov_base;
        const float FOV_LERP_SPEED = 6.0f; // higher = faster interpolation
        fov_display += (fov_target - fov_display) * fminf(1.0f, dt * FOV_LERP_SPEED);
        // update derived camera focal length
        fov_rad = fov_display * (M_PI / 180.0f);
        camera.focal_length = 1.0f / tanf(fov_rad * 0.5f);

        // Movement and jumping
        float wish_vx = 0.0f;
        float wish_vz = 0.0f;
        // Build forward vector from yaw/pitch
        float yaw_rad = camera.yaw * (M_PI / 180.0f);
        float pitch_rad = camera.pitch * (M_PI / 180.0f);
        float fx = cosf(pitch_rad) * sinf(yaw_rad);
        float fy = -sinf(pitch_rad);
        float fz = cosf(pitch_rad) * cosf(yaw_rad);
        // normalize forward
        float flen = sqrtf(fx*fx + fy*fy + fz*fz);
        if (flen > 0.000001f) { fx /= flen; fy /= flen; fz /= flen; }

        // right = normalize(cross(up, forward)) ; up = (0,1,0)
        float rx = fz;
        float rz = -fx;
        float rlen = sqrtf(rx*rx + rz*rz);
        if (rlen > 0.000001f) { rx /= rlen; rz /= rlen; } else { rx = 1.0f; rz = 0.0f; }

        // Ground-constrained movement: ignore forward.y
        float gx = fx;
        float gz = fz;
        float glen = sqrtf(gx*gx + gz*gz);
        if (glen > 0.000001f) { gx /= glen; gz /= glen; } else { gx = 0.0f; gz = 1.0f; }

        if (keystate[SDL_SCANCODE_W]) {
            wish_vx += gx * move_speed;
            wish_vz += gz * move_speed;
        }
        if (keystate[SDL_SCANCODE_S]) {
            wish_vx -= gx * move_speed;
            wish_vz -= gz * move_speed;
        }
        if (keystate[SDL_SCANCODE_A]) {
            wish_vx -= rx * move_speed;
            wish_vz -= rz * move_speed;
        }
        if (keystate[SDL_SCANCODE_D]) {
            wish_vx += rx * move_speed;
            wish_vz += rz * move_speed;
        }
        if (keystate[SDL_SCANCODE_SPACE] && is_grounded) { // Jump impulse
            vertical_velocity = JUMP_IMPULSE;
            is_grounded = false;
        }

        // Resolve horizontal movement with collisions (axis-by-axis)
        float new_x = camera.x + wish_vx * dt;
        AABB box_x = player_aabb(new_x, camera.y, camera.z, PLAYER_RADIUS, PLAYER_HEIGHT, PLAYER_EYE_HEIGHT);
        if (!aabb_intersects_map(&cubes, box_x, CUBE_SIZE, GRID_OFFSET_X, GRID_OFFSET_Y, GRID_OFFSET_Z)) {
            camera.x = new_x;
        }

        float new_z = camera.z + wish_vz * dt;
        AABB box_z = player_aabb(camera.x, camera.y, new_z, PLAYER_RADIUS, PLAYER_HEIGHT, PLAYER_EYE_HEIGHT);
        if (!aabb_intersects_map(&cubes, box_z, CUBE_SIZE, GRID_OFFSET_X, GRID_OFFSET_Y, GRID_OFFSET_Z)) {
            camera.z = new_z;
        }

        // Apply vertical physics (gravity) every frame. Positive velocity = upward.
        if (!is_grounded) {
            vertical_velocity -= GRAVITY * dt;
        } else if (vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }

        float new_y = camera.y + vertical_velocity * dt;
        AABB box_y = player_aabb(camera.x, new_y, camera.z, PLAYER_RADIUS, PLAYER_HEIGHT, PLAYER_EYE_HEIGHT);
        if (aabb_intersects_map(&cubes, box_y, CUBE_SIZE, GRID_OFFSET_X, GRID_OFFSET_Y, GRID_OFFSET_Z)) {
            // collide vertically
            if (vertical_velocity < 0.0f) {
                is_grounded = true;
            }
            vertical_velocity = 0.0f;
            new_y = camera.y;
        } else {
            camera.y = new_y;
            if (vertical_velocity <= 0.0f) {
                AABB probe = player_aabb(camera.x, camera.y - GROUND_EPS, camera.z, PLAYER_RADIUS, PLAYER_HEIGHT, PLAYER_EYE_HEIGHT);
                is_grounded = aabb_intersects_map(&cubes, probe, CUBE_SIZE, GRID_OFFSET_X, GRID_OFFSET_Y, GRID_OFFSET_Z);
                if (is_grounded) {
                    vertical_velocity = 0.0f;
                }
            } else {
                is_grounded = false;
            }
        }

        // Simple fall reset if we drop too far below the world
        if (camera.y < -FALL_RESET_DISTANCE) {
            camera.x = 0.0f;
            camera.y = 15.0f;
            camera.z = 0.0f;
            camera.yaw = 0.0f;
            camera.pitch = 30.0f;
            vertical_velocity = 0.0f;
            is_grounded = false;
        }

        // Walking bob calculation (smooth start/stop). Only when on ground and not jumping or falling.
        bool moving_input = (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_D]);
        float target_amp = (is_grounded && moving_input) ? WALK_AMPLITUDE : 0.0f;
        // smooth amplitude
        walk_amp += (target_amp - walk_amp) * fminf(1.0f, dt * WALK_SMOOTH);
        if (walk_amp > 0.0001f) {
            walk_phase += dt * walk_frequency_current * (2.0f * (float)M_PI);
            if (walk_phase > 1e6f) walk_phase -= 1e6f;
        }
        float bob_offset = walk_amp * sinf(walk_phase);
        // apply bob to camera for this frame only (don't modify permanent camera_pos when jumping)
        float saved_camera_y = camera.y;
        camera.y += bob_offset;

        // Clear screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Draw all cubes in the map
        size_t cubes_capacity = cube_map_capacity(&cubes);
        for (size_t ci = 0; ci < cubes_capacity; ++ci) {
            const Cube_Map_Entry* entry = cube_map_entry_at(&cubes, ci);
            if (entry && entry->occupied && entry->cube) {
                draw_cube(entry->cube);
            }
        }

        // Draw static crosshair in the center of the screen
        draw_crosshair(3, 17);

        // ImGui overlay: update stats, start a new frame, let it draw UI, then render on top
        if (OVERLAY_ON) {
            overlay_set_stats(camera.x, camera.y, camera.z, camera.yaw, camera.pitch, fov_display, cubes.size, cube_map_capacity(&cubes));
        }
        overlay_newframe();
        overlay_render();

        // Render present
        SDL_RenderPresent(renderer);

        // restore camera Y after rendering
        camera.y = saved_camera_y;

        // FPS capping
        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    // Cleanup
    // Shutdown ImGui overlay if present
    overlay_shutdown();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}

bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("3dsdl", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    if (OVERLAY_ON) {
        overlay_init(window, renderer);
    }

    return true;
}

// Draw crosshair in the center of the screen
void draw_crosshair(int thickness, int size) {
    if (thickness <= 0 || size <= 0) {
        return;
    }
    // Round up to next odd number if even to ensure a single central pixel
    if ((thickness & 1) == 0) {
        thickness += 1;
    }
    if ((size & 1) == 0) {
        size += 1;
    }

    int cx = WIDTH / 2;
    int cy = HEIGHT / 2;
    int half = size / 2;
    int t_half = thickness / 2;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    // Draw horizontal band of `thickness` pixels centered at cy
    for (int dy = -t_half; dy <= t_half; ++dy) {
        SDL_RenderDrawLine(renderer, cx - half, cy + dy, cx + half, cy + dy);
    }
    // Draw vertical band of `thickness` pixels centered at cx
    for (int dx = -t_half; dx <= t_half; ++dx) {
        SDL_RenderDrawLine(renderer, cx + dx, cy - half, cx + dx, cy + half);
    }
}