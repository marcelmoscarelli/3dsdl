#include <stdio.h>   // fprintf, stderr
#include <stdlib.h>  // exit, EXIT_FAILURE, EXIT_SUCCESS, rand, srand
#include <time.h>    // time
#include <stdbool.h> // bool type, true, false
#include <math.h>    // tanf, sinf, cosf, sqrtf, expf

#include <SDL2/SDL.h>
#include <SDL_ttf.h>

#include "data_structures.h"
#include "settings.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Camera camera = {0.0f, 2.0f, 0.0f, 0.0f, 0.0f, -0.15611995216165922};

// Global vars for SDL
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Color WHITE = { 255, 255, 255, 255 };

// Global vars for text overlay
static TTF_Font* font = NULL;
static bool overlay = true;

// Function prototypes
bool init();
void make_cube(Cube* cube, float size, Point3D center);
void draw_cube(const Cube* cube);
void rotate_cube(Cube* cube, Point3D axis, float angle);
TTF_Font* open_preferred_font(int ptsize);

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

    // Initialize a dynamic array for cubes
    Cube_DArray cubes;
    init_cube_darray(&cubes, 1000);
    
    // Ground grid parameters (100x100, cubes glued together, size 0.25)
    const int GROUND_W = 50;
    const int GROUND_H = 50;
    const float GROUND_CUBE_SIZE = 0.5f;
    const float GROUND_SPACING = 0.0f; // glued

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
        new_cube->color = WHITE;

        // Squash cubes flat (become 2D tiles)
        new_cube->points[0] = new_cube->points[2];
        new_cube->points[1] = new_cube->points[3];
        new_cube->points[4] = new_cube->points[6];
        new_cube->points[5] = new_cube->points[7];

        add_cube_to_darray(&cubes, new_cube);
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
    // Stats text texture cache to avoid TTF overhead every frame
    SDL_Texture* stats_texture = NULL;
    SDL_Rect stats_rect = { 15, 15, 0, 0 };
    int stats_frame_count = 0;
    while (running) {
        Uint32 frame_start = SDL_GetTicks();

        // Event handling
        while (SDL_PollEvent(&event)) {
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
        float dt = (now - last_ticks) / 1000.0f;
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

        // Draw all cubes in the array
        for (size_t ci = 0; ci < cubes.cur_size; ++ci) {
            draw_cube(cubes.arr[ci]);
        }

        // Draw static crosshair in the center of the screen
        int cx = WIDTH / 2;
        int cy = HEIGHT / 2;
        int half = 8; // crosshair half-length
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, cx - half, cy, cx + half, cy);
        SDL_RenderDrawLine(renderer, cx, cy - half, cx, cy + half);

        // Show stats in the top-left corner (update only every 5 frames)
        if (overlay) {
            stats_frame_count++;
            if (stats_texture == NULL || (stats_frame_count % 5) == 0) {
                char stats_text[256];
                snprintf(stats_text, sizeof(stats_text),
                        "FPS: %d\nPos.: (x:%.2f, y:%.2f, z:%.2f)\nYaw: %.1f Pitch: %.1f\nFOV: %.1f degrees\nSpeed: %.2f u/s",
                        (int)(1.0f / dt),
                        camera.x, camera.y, camera.z,
                        camera.yaw, camera.pitch,
                        fov_display,
                        move_speed
                );
                SDL_Color text_color = { 255, 255, 255, 255 };
                SDL_Surface* text_surface = TTF_RenderText_Blended_Wrapped(font, stats_text, text_color, 350);
                if (text_surface) {
                    // replace previous texture
                    if (stats_texture) SDL_DestroyTexture(stats_texture);
                    stats_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
                    if (stats_texture) {
                        SDL_SetTextureBlendMode(stats_texture, SDL_BLENDMODE_BLEND);
                    }
                    stats_rect.w = text_surface->w;
                    stats_rect.h = text_surface->h;
                    SDL_FreeSurface(text_surface);
                }
            }
            if (stats_texture) {
                // draw semi-transparent black background box behind the stats text
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_Color bg_col = { 0, 0, 0, 96 };
                SDL_SetRenderDrawColor(renderer, bg_col.r, bg_col.g, bg_col.b, bg_col.a);
                SDL_Rect bg_rect = { stats_rect.x - 8, stats_rect.y - 8, stats_rect.w + 16, stats_rect.h + 16 };
                SDL_RenderFillRect(renderer, &bg_rect);

                SDL_RenderCopy(renderer, stats_texture, NULL, &stats_rect);
            }
        }

        // Render present
        SDL_RenderPresent(renderer);

        // restore camera Y after rendering
        camera.y = saved_camera_y;

        Uint32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frame_time);
        }
    }

    TTF_CloseFont(font);
    TTF_Quit();
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

    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    // Try a small prioritized list of monospace fonts (Consolas -> Courier New -> DejaVu Sans Mono -> Lucida Console)
    // This will attempt a set of likely file paths on Windows and Linux and return the first font that opens.
    font = open_preferred_font(16);
    if (!font) {
        fprintf(stderr, "TTF_OpenFont Error: no fallback fonts found (%s)\n", TTF_GetError());
        overlay = false; // disable overlay if no font
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
    
    SDL_SetRenderDrawColor(renderer, cube->color.r, cube->color.g, cube->color.b, cube->color.a);
    Point2D projected[8];
    bool skip_point[8] = { false };
    // Initialize projected points to safe defaults
    for (size_t i = 0; i < 8; ++i) {
        projected[i].x = 0;
        projected[i].y = 0;
    }
    // Precompute camera rotation
    float yaw_rad = camera.yaw * (M_PI / 180.0f);
    float pitch_rad = camera.pitch * (M_PI / 180.0f);
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    float cos_pitch = cosf(pitch_rad);
    float sin_pitch = sinf(pitch_rad);

    for (size_t i = 0; i < 8; ++i) {
        // World -> camera space: translate then rotate by inverse camera yaw/pitch
        float rel_x = cube->points[i].x - camera.x;
        float rel_y = cube->points[i].y - camera.y;
        float rel_z = cube->points[i].z - camera.z;

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

        float x_ndc = (x1 * camera.focal_length / ASPECT_RATIO) / z2;
        float y_ndc = (y2 * camera.focal_length) / z2;
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

// Rotate all points of a cube around an arbitrary axis passing through `center`.
// `axis` is a direction vector (does not need to be unit length). `angle` is in radians.
void rotate_cube(Cube* cube, Point3D axis, float angle) {
    if (!cube) return;
    // calc the center of the cube
    float center_x = 0.0f;
    float center_y = 0.0f;
    float center_z = 0.0f;
    for (size_t i = 0; i < 8; ++i) {
        center_x += cube->points[i].x;
        center_y += cube->points[i].y;
        center_z += cube->points[i].z;
    }
    center_x /= 8.0f;
    center_y /= 8.0f;
    center_z /= 8.0f;

    // normalize axis
    float ax = axis.x;
    float ay = axis.y;
    float az = axis.z;
    float alen = sqrtf(ax*ax + ay*ay + az*az);
    if (alen < 1e-6f) return;
    float kx = ax / alen;
    float ky = ay / alen;
    float kz = az / alen;

    float c = cosf(angle);
    float s = sinf(angle);

    for (size_t i = 0; i < 8; ++i) {
        // vector from center to point
        float vx = cube->points[i].x - center_x;
        float vy = cube->points[i].y - center_y;
        float vz = cube->points[i].z - center_z;

        // k x v
        float cx = ky * vz - kz * vy;
        float cy = kz * vx - kx * vz;
        float cz = kx * vy - ky * vx;

        // k . v
        float kd = kx * vx + ky * vy + kz * vz;

        // Rodrigues' rotation formula
        float rx = vx * c + cx * s + kx * kd * (1.0f - c);
        float ry = vy * c + cy * s + ky * kd * (1.0f - c);
        float rz = vz * c + cz * s + kz * kd * (1.0f - c);

        cube->points[i].x = center_x + rx;
        cube->points[i].y = center_y + ry;
        cube->points[i].z = center_z + rz;
    }
}

// Try opening several likely TTF file paths for preferred monospace fonts in order:
// Consolas, Courier New, DejaVu Sans Mono, Lucida Console.
// Returns an opened TTF_Font* or NULL if none found.
TTF_Font* open_preferred_font(int ptsize) {
    const char* family_names[] = {"Consolas", "Courier New", "DejaVu Sans Mono", "Lucida Console"};

    #ifdef _WIN32
    const char* consolas_paths[] = {"C:/Windows/Fonts/Consolas Regular.ttf", "C:/Windows/Fonts/consola.TTF", NULL};
    const char* courier_paths[]  = {"C:/Windows/Fonts/Courier New Regular.ttf", "C:/Windows/Fonts/cour.ttf", NULL};
    const char* lucida_paths[]   = {"C:/Windows/Fonts/Lucida Console Regular.ttf", "C:/Windows/Fonts/lucon.ttf", NULL};
    const char** families[] = {consolas_paths, courier_paths, lucida_paths};
    #else
    const char* dejavu_paths[]   = {"/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/usr/local/share/fonts/DejaVuSansMono.ttf", NULL};
    const char* courier_paths[]  = {"/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf", "/usr/share/fonts/truetype/msttcorefonts/Courier New.ttf", "/usr/share/fonts/truetype/freefont/FreeMono.ttf", NULL};
    const char* lucida_paths[]   = {"/usr/share/fonts/truetype/lucida/LucidaConsole.ttf", "/usr/share/fonts/truetype/msttcorefonts/Lucida_Console.ttf", NULL};
    const char** families[] = {dejavu_paths, courier_paths, lucida_paths};
    #endif

    for (size_t f = 0; f < (sizeof(families) / sizeof(families[0])); ++f) {
        const char** paths = families[f];
        for (size_t i = 0; paths[i] != NULL; ++i) {
            const char* p = paths[i];
            if (!p) continue;
            TTF_Font* fnt = TTF_OpenFont(p, ptsize);
            if (fnt) {
                fprintf(stderr, "Loaded font '%s' (family %s)\n", p, family_names[f]);
                return fnt;
            }
        }
    }

    return NULL;
}