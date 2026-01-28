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
    Cube arr[10000];
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
Camera camera_pos = {0.0f, 2.0f, 0.0f, 0.0f, 0.0f};
bool allow_flying = false; // when false, W/S move along ground (XZ plane)

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
    const float DEPTH_Z = 12.5f;

    CubeArray cube_wall = {.size = 0};
    const int total = GRID_W * GRID_H;
    for (int i = 0; i < total; ++i) {
        int gx = i % GRID_W;
        int gy = i / GRID_W;

        float step = CUBE_SIZE + SPACING; // center-to-center distance
        float offset_x = ((GRID_W - 1) * step) / 2.0f;
        float half_size = CUBE_SIZE / 2.0f;

        // Keep grid centered in X, but place wall above ground (y=0).
        // Set the lowest cube's bottom at y=0 => its center at half_size.
        Point3D center = {
            .x = gx * step - offset_x,
            .y = gy * step + half_size + SPACING,
            .z = DEPTH_Z
        };

        make_cube(&cube_wall.arr[cube_wall.size], CUBE_SIZE, center);
        cube_wall.size++;
    }
    
    // Ground grid parameters (100x100, cubes glued together, size 0.25)
    const int GROUND_W = 75;
    const int GROUND_H = 75;
    const float GROUND_CUBE_SIZE = 0.5f;
    const float GROUND_SPACING = 0.0f; // glued

    CubeArray cube_ground = {.size = 0};
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

        make_cube(&cube_ground.arr[cube_ground.size], GROUND_CUBE_SIZE, center);
        cube_ground.size++;
    }

    // Compute ground bounds (world space) so we can detect falling off edges
    float ground_step = GROUND_CUBE_SIZE + GROUND_SPACING;
    float ground_offset_x = ((GROUND_W - 1) * ground_step) / 2.0f;
    float ground_offset_z = ((GROUND_H - 1) * ground_step) / 2.0f;
    float ground_half = GROUND_CUBE_SIZE / 2.0f;
    const float GROUND_MIN_X = -ground_offset_x - ground_half;
    const float GROUND_MAX_X =  ground_offset_x + ground_half;
    const float GROUND_MIN_Z = -ground_offset_z - ground_half;
    const float GROUND_MAX_Z =  ground_offset_z + ground_half;

    // Camera parameters
    float fov = 60.0f;                      // Field of view in degrees
    float fov_rad = fov * (M_PI / 180.0f);  // Convert to radians
    camera_f = 1.0f / tanf(fov_rad * 0.5f); // Focal length of the camera
    // Enable relative mouse mode for FPS-style look
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_ShowCursor(SDL_DISABLE);

    // Jump parameters
    bool is_jumping = false;
    float jump_time = 0.0f;
    const float jump_dur = 0.6f; // seconds
    const float jump_height = 2.0f;
    float ground_y = camera_pos.y;

    // Walking (bob) parameters
    float walk_phase = 0.0f;
    float walk_amp = 0.0f; // current amplitude
    const float WALK_AMPLITUDE = 0.15f; // meters
    const float WALK_FREQUENCY = 3.0f; // steps per second
    const float WALK_SMOOTH = 8.0f; // smoothing rate
    // Falling / gravity
    bool is_falling = false;
    float fall_velocity = 0.0f;
    float fall_start_y = 0.0f;
    float fall_mom_x = 0.0f;
    float fall_mom_z = 0.0f;
    const float GRAVITY = 30.0f; // units/s^2 (gamey)
    const float FALL_RESET_DISTANCE = 350.0f; // units to trigger reset

    // Main loop
    bool running = true;
    SDL_Event event;
    Uint32 last_ticks = SDL_GetTicks();
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
                    // Standard Y (not inverted): moving mouse up decreases yrel, so add it
                    camera_pos.pitch += event.motion.yrel * mouse_sens;
                    if (camera_pos.pitch > PITCH_MAX) camera_pos.pitch = PITCH_MAX;
                    if (camera_pos.pitch < PITCH_MIN) camera_pos.pitch = PITCH_MIN;
                    break;
                }
                default:
                    break;
            }
        }

        // WASD controls now move the camera relative to view (crosshair)
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) / 1000.0f;
        last_ticks = now;

        const Uint8* keystate = SDL_GetKeyboardState(NULL);
        const float move_speed = 5.0f; // units per second
        float camera_step = move_speed * dt;

        // Movement and jumping (disabled while falling)
        if (!is_falling) {
            // Build forward vector from yaw/pitch
            float yaw_rad = camera_pos.yaw * (M_PI / 180.0f);
            float pitch_rad = camera_pos.pitch * (M_PI / 180.0f);
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
                camera_pos.x += gx * camera_step;
                camera_pos.z += gz * camera_step;
            }
            if (keystate[SDL_SCANCODE_S]) {
                camera_pos.x -= gx * camera_step;
                camera_pos.z -= gz * camera_step;
            }
            if (keystate[SDL_SCANCODE_A]) {
                camera_pos.x -= rx * camera_step;
                camera_pos.z -= rz * camera_step;
            }
            if (keystate[SDL_SCANCODE_D]) {
                camera_pos.x += rx * camera_step;
                camera_pos.z += rz * camera_step;
            }
            if (keystate[SDL_SCANCODE_SPACE] && !is_jumping) { // Jump
                is_jumping = true;
                jump_time = 0.0f;
                ground_y = camera_pos.y;
            }

            // Jumping logic
            if (is_jumping) {
                jump_time += dt;
                float t = jump_time / jump_dur;
                if (t >= 1.0f) { camera_pos.y = ground_y; is_jumping = false; }
                else {
                    camera_pos.y = ground_y + jump_height * sinf(M_PI * t);
                }
            }
        } else {
            // while falling, apply gravity and horizontal momentum with damping
            fall_velocity += GRAVITY * dt;
            camera_pos.y -= fall_velocity * dt;
            // apply horizontal momentum
            // damping per second -> use exp decay for frame-rate independence
            const float MOM_DAMP_RATE = 1.5f; // larger = faster stop
            float damp = expf(-MOM_DAMP_RATE * dt);
            camera_pos.x += fall_mom_x * dt;
            camera_pos.z += fall_mom_z * dt;
            fall_mom_x *= damp;
            fall_mom_z *= damp;
        }

        // If not already falling, check bounds to start falling
        if (!is_falling && !is_jumping) {
            if (camera_pos.x < GROUND_MIN_X || camera_pos.x > GROUND_MAX_X || camera_pos.z < GROUND_MIN_Z || camera_pos.z > GROUND_MAX_Z) {
                // start falling: capture horizontal momentum based on current input and orientation
                // compute orientation vectors
                float yaw_rad_local = camera_pos.yaw * (M_PI / 180.0f);
                float pitch_rad_local = camera_pos.pitch * (M_PI / 180.0f);
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
                fall_velocity = 0.0f;
                fall_start_y = camera_pos.y;
                fall_mom_x = vx;
                fall_mom_z = vz;
            }
        }

        // If falling and we've returned within bounds and hit the ground level, land
        if (is_falling) {
            if (camera_pos.x >= GROUND_MIN_X && camera_pos.x <= GROUND_MAX_X && camera_pos.z >= GROUND_MIN_Z && camera_pos.z <= GROUND_MAX_Z && camera_pos.y <= ground_y) {
                is_falling = false;
                fall_velocity = 0.0f;
                camera_pos.y = ground_y;
            }
            // check fall reset distance and respawn instead of quitting
            if (fall_start_y - camera_pos.y >= FALL_RESET_DISTANCE) {
                // reset camera to safe high position and then continue falling to y=2.0
                camera_pos.x = 0.0f;
                camera_pos.y = 15.0f;
                camera_pos.z = 0.0f;
                camera_pos.yaw = 0.0f;
                camera_pos.pitch = 0.0f;
                // clear horizontal momentum, start falling from reset height
                fall_mom_x = 0.0f;
                fall_mom_z = 0.0f;
                fall_velocity = 0.0f;
                is_jumping = false;
                // set target ground level to 2.0 and start falling
                ground_y = 2.0f;
                is_falling = true;
                fall_start_y = camera_pos.y;
            }
        }

        // Q = up, E = down (keep as direct Y movement) - disabled while falling
        if (!is_falling) {
            if (keystate[SDL_SCANCODE_Q]) camera_pos.y += camera_step;
            if (keystate[SDL_SCANCODE_E]) camera_pos.y -= camera_step;
        }
        // Walking bob calculation (smooth start/stop). Only when on ground and not jumping or falling.
        bool moving_input = (!is_falling) && (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_D]);
        float target_amp = (!is_jumping && !is_falling && moving_input) ? WALK_AMPLITUDE : 0.0f;
        // smooth amplitude
        walk_amp += (target_amp - walk_amp) * fminf(1.0f, dt * WALK_SMOOTH);
        if (walk_amp > 0.0001f) {
            walk_phase += dt * WALK_FREQUENCY * (2.0f * (float)M_PI);
            if (walk_phase > 1e6f) walk_phase -= 1e6f;
        }
        float bob_offset = walk_amp * sinf(walk_phase);
        // apply bob to camera for this frame only (don't modify permanent camera_pos when jumping)
        float saved_camera_y = camera_pos.y;
        camera_pos.y += bob_offset;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        // Draw all cubes in the array
        for (size_t ci = 0; ci < cube_wall.size; ++ci) {
            draw_cube(&cube_wall.arr[ci]);
        }

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        // Draw all cubes in the array
        for (size_t ci = 0; ci < cube_ground.size; ++ci) {
            draw_cube(&cube_ground.arr[ci]);
        }

        // Draw static crosshair in the center of the screen
        int cx = WIDTH / 2;
        int cy = HEIGHT / 2;
        int half = 8; // crosshair half-length
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, cx - half, cy, cx + half, cy);
        SDL_RenderDrawLine(renderer, cx, cy - half, cx, cy + half);

        SDL_RenderPresent(renderer);

        // restore camera Y after rendering
        camera_pos.y = saved_camera_y;

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