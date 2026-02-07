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

// Dear ImGui overlay (implemented in C++ wrapper)

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
    
    // Ground grid parameters (100x100, cubes glued together, size 0.25)
    const int GROUND_W = 25;
    const int GROUND_H = 25;
    const float GROUND_CUBE_SIZE = 1.0f;
    const float GROUND_SPACING = 0.1f; // glued

    const int ground_total = GROUND_W * GROUND_H;
    for (int i = 0; i < ground_total; ++i) {
        int gx = i % GROUND_W;
        int gz = i / GROUND_W;

        float step = GROUND_CUBE_SIZE + GROUND_SPACING; // center-to-center distance
        float offset_x = ((GROUND_W - 1) * step) / 2.0f;
        float offset_z = ((GROUND_H - 1) * step) / 2.0f;
        float half_size = GROUND_CUBE_SIZE / 2.0f;

        // Centered in X and Z, Y is half-size below origin (-0.25)
        Point3D center = {
            .x = gx * step - offset_x,
            .y = -half_size,
            .z = gz * step - offset_z
        };

        Cube* new_cube = (Cube*)malloc(sizeof(Cube));
        
        make_cube(new_cube, GROUND_CUBE_SIZE, center);
        new_cube->color = (SDL_Color){0, 255, 0, 255};

        // Squash cubes flat (become 2D tiles)
        new_cube->points[0] = new_cube->points[2];
        new_cube->points[1] = new_cube->points[3];
        new_cube->points[4] = new_cube->points[6];
        new_cube->points[5] = new_cube->points[7];

        Cube_Key key = {
            .x = gx,
            .y = 0,
            .z = gz
        };
        cube_map_add(&cubes, key, new_cube);
    }

    // Compute ground bounds (world space) so we can detect falling off edges
    float ground_step = GROUND_CUBE_SIZE + GROUND_SPACING;
    float ground_offset_x = ((GROUND_W - 1) * ground_step) / 2.0f;
    float ground_offset_z = ((GROUND_H - 1) * ground_step) / 2.0f;
    float ground_half = GROUND_CUBE_SIZE / 2.0f;
    const float GROUND_MIN_X = -ground_offset_x - ground_half; // TODO: fix this crappy method
    const float GROUND_MAX_X =  ground_offset_x + ground_half;
    const float GROUND_MIN_Z = -ground_offset_z - ground_half;
    const float GROUND_MAX_Z =  ground_offset_z + ground_half;

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
    const float JUMP_IMPULSE = 10.0f; // initial upward velocity for a jump (tweakable)
    float ground_y = camera.y;

    // Walking (bob) parameters
    float walk_phase = 0.0f;
    float walk_amp = 0.0f; // current amplitude
    const float WALK_AMPLITUDE = 0.15f; // meters
    const float WALK_FREQUENCY = 2.25f; // steps per second (base)
    float walk_frequency_current = WALK_FREQUENCY; // may be increased while sprinting
    const float WALK_SMOOTH = 8.0f; // smoothing rate

    // Falling / gravity
    bool is_falling = false;
    float fall_start_y = 0.0f;
    float fall_mom_x = 0.0f;
    float fall_mom_z = 0.0f;
    const float GRAVITY = 30.0f; // units/s^2 (gamey)
    const float FALL_RESET_DISTANCE = 200.0f; // units to trigger reset

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
        float camera_step = move_speed * dt;
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

        // Movement and jumping (disabled while falling)
        if (!is_falling) {
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
                camera.x += gx * camera_step;
                camera.z += gz * camera_step;
            }
            if (keystate[SDL_SCANCODE_S]) {
                camera.x -= gx * camera_step;
                camera.z -= gz * camera_step;
            }
            if (keystate[SDL_SCANCODE_A]) {
                camera.x -= rx * camera_step;
                camera.z -= rz * camera_step;
            }
            if (keystate[SDL_SCANCODE_D]) {
                camera.x += rx * camera_step;
                camera.z += rz * camera_step;
            }
            if (keystate[SDL_SCANCODE_SPACE] && is_grounded) { // Jump impulse
                ground_y = camera.y; // capture current ground level
                vertical_velocity = JUMP_IMPULSE;
                is_grounded = false;
            }
        }

        // Apply vertical physics (gravity) every frame. Positive velocity = upward.
        if (is_grounded) {
            vertical_velocity = 0.0f;
            camera.y = ground_y;
        } else {
            vertical_velocity -= GRAVITY * dt;
            camera.y += vertical_velocity * dt;
        }

        // while falling, apply horizontal momentum with damping
        if (is_falling) {
            // damping per second -> use exp decay for frame-rate independence
            const float MOM_DAMP_RATE = 1.5f; // larger = faster stop
            float damp = expf(-MOM_DAMP_RATE * dt);
            camera.x += fall_mom_x * dt;
            camera.z += fall_mom_z * dt;
            fall_mom_x *= damp;
            fall_mom_z *= damp;
        }

        // If not already falling, check bounds to start falling
        // Allow starting a fall even if the player is mid-jump so the jump naturally transitions to falling.
        if (!is_falling) {
            if (camera.x < GROUND_MIN_X || camera.x > GROUND_MAX_X || camera.z < GROUND_MIN_Z || camera.z > GROUND_MAX_Z) {
                // start falling: capture horizontal momentum based on current input and orientation
                // compute orientation vectors
                float yaw_rad_local = camera.yaw * (M_PI / 180.0f);
                float pitch_rad_local = camera.pitch * (M_PI / 180.0f);
                float fx_local = cosf(pitch_rad_local) * sinf(yaw_rad_local);
                float fz_local = cosf(pitch_rad_local) * cosf(yaw_rad_local);
                float rx_local = fz_local;
                float rz_local = -fx_local;
                // normalize ground forward
                float gx_local = fx_local;
                float gz_local = fz_local;
                float glen_local = sqrtf(gx_local*gx_local + gz_local*gz_local);
                if (glen_local > 0.000001f) { gx_local /= glen_local; gz_local /= glen_local; } else { gx_local = 0.0f; gz_local = 1.0f; }
                int ws = (keystate[SDL_SCANCODE_W] ? 1 : 0) - (keystate[SDL_SCANCODE_S] ? 1 : 0);
                int ad = (keystate[SDL_SCANCODE_D] ? 1 : 0) - (keystate[SDL_SCANCODE_A] ? 1 : 0);
                float vx = (ws * gx_local + ad * rx_local) * (move_speed);
                float vz = (ws * gz_local + ad * rz_local) * (move_speed);

                is_falling = true;
                is_grounded = false;
                fall_start_y = camera.y;
                fall_mom_x = vx;
                fall_mom_z = vz;
            }
        }

        // If player is within the ground bounds, treat the normal ground level as y=2.0f
        if (camera.x >= GROUND_MIN_X && camera.x <= GROUND_MAX_X && camera.z >= GROUND_MIN_Z && camera.z <= GROUND_MAX_Z) {
            ground_y = 2.0f;
        }

        // Landing when airborne but not flagged as "falling" (e.g. jumping up and back down inside bounds)
        if (!is_falling && !is_grounded) {
            if (camera.x >= GROUND_MIN_X && camera.x <= GROUND_MAX_X && camera.z >= GROUND_MIN_Z && camera.z <= GROUND_MAX_Z && camera.y <= ground_y) {
                is_grounded = true;
                vertical_velocity = 0.0f;
                camera.y = ground_y;
            }
        }

        // If falling and we've returned within bounds and hit the ground level, land
        if (is_falling) {
            if (camera.x >= GROUND_MIN_X && camera.x <= GROUND_MAX_X && camera.z >= GROUND_MIN_Z && camera.z <= GROUND_MAX_Z && camera.y <= ground_y) {
                is_falling = false;
                vertical_velocity = 0.0f;
                is_grounded = true;
                camera.y = ground_y;
            }
            // check fall reset distance and respawn instead of quitting
            if (fall_start_y - camera.y >= FALL_RESET_DISTANCE) {
                // reset camera to safe high position and then continue falling to y=2.0
                camera.x = 0.0f;
                camera.y = 15.0f;
                camera.z = 0.0f;
                camera.yaw = 0.0f;
                camera.pitch = 0.0f;
                // clear horizontal momentum, start falling from reset height
                fall_mom_x = 0.0f;
                fall_mom_z = 0.0f;
                vertical_velocity = 0.0f;
                is_grounded = false;
                // set target ground level to 2.0 and start falling
                ground_y = 2.0f;
                is_falling = true;
                fall_start_y = camera.y;
            }
        }

        // Walking bob calculation (smooth start/stop). Only when on ground and not jumping or falling.
        bool moving_input = (!is_falling) && (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_D]);
        float target_amp = (!is_falling && is_grounded && moving_input) ? WALK_AMPLITUDE : 0.0f;
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