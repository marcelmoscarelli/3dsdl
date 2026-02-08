#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "data_structures.h"
#include "imgui_overlay.h"
#include "rendering.h"
#include "settings.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Camera initial position and orientation
Camera camera = {
    .x = 0.0f,
    .y = 100.0f,
    .z = 0.0f,
    .yaw = 0.0f,
    .pitch = 75.0f,
    .focal_length = -0.15611995216165922 // fov = 60 degrees
};

// Globals for SDL
SDL_Renderer* renderer = NULL;
static SDL_Window* window = NULL;

// Function prototypes
bool init();
void create_ground_grid(Cube_Map* map, int size, int x, int y, int z, SDL_Color color, int hole_size);

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
    const int GROUND_SIZE = 9;
    const float GRID_OFFSET_X = ((GROUND_SIZE - 1) * CUBE_SIZE) / 2.0f;
    const float GRID_OFFSET_Z = ((GROUND_SIZE - 1) * CUBE_SIZE) / 2.0f;
    const float GRID_OFFSET_Y = CUBE_SIZE * 0.5f;
    create_ground_grid(&cubes, GROUND_SIZE, 0, 0, 0, (SDL_Color){255, 255, 0, 255}, 0); // Yellow
    create_ground_grid(&cubes, GROUND_SIZE, 0, 2, GROUND_SIZE, (SDL_Color){0, 255, 0, 255}, 1); // Green
    create_ground_grid(&cubes, GROUND_SIZE, GROUND_SIZE, 4, GROUND_SIZE, (SDL_Color){0, 255, 255, 255}, 3); // Cyan
    create_ground_grid(&cubes, GROUND_SIZE, GROUND_SIZE, 6, 0, (SDL_Color){255, 0, 255, 255}, 5); // Magenta
    create_ground_grid(&cubes, GROUND_SIZE, 0, 8, 0, (SDL_Color){255, 255, 255, 255}, 7); // White

    // Camera parameters
    float fov_display = 60.0f;
    float fov_rad = fov_display * (M_PI / 180.0f);     // Convert to radians
    camera.focal_length = 1.0f / tanf(fov_rad * 0.5f); // Focal length of the camera

    // Enable relative mouse mode for FPS-style look
    bool mouse_captured = true;
    SDL_SetRelativeMouseMode(SDL_TRUE);
    SDL_ShowCursor(SDL_DISABLE);

    // Vertical movement (jump / fall) parameters
    bool is_grounded = true;
    float vertical_velocity = 0.0f; // positive = up

    // Walking (bob) parameters
    float walk_phase = 0.0f;
    float walk_amp = 0.0f; // current amplitude, will smoothly approach WALK_AMPLITUDE when moving
    float walk_frequency_current = WALK_FREQUENCY; // may be increased while sprinting

    // Buffers for rendering
    Render_Face* faces = NULL;
    size_t faces_cap = 0;
    SDL_Vertex* tri_verts = NULL;
    size_t tri_cap = 0;
    static const int FACE_INDICES[6][4] = {
        {0, 1, 2, 3},
        {4, 5, 6, 7},
        {0, 1, 5, 4},
        {2, 3, 7, 6},
        {1, 2, 6, 5},
        {0, 3, 7, 4}
    };

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
                    // Alt+F4 or clicking X button on window
                    running = false;
                    break;
                case SDL_KEYDOWN: {
                    // Toggle mouse capture with Esc key
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        mouse_captured = !mouse_captured;
                        SDL_SetRelativeMouseMode(mouse_captured ? SDL_TRUE : SDL_FALSE);
                        SDL_ShowCursor(mouse_captured ? SDL_DISABLE : SDL_ENABLE);
                    }
                    break;
                }
                case SDL_MOUSEMOTION: {
                    // Update camera orientation from mouse movement
                    camera.yaw += event.motion.xrel * MOUSE_SENSITIVITY;
                    if (camera.yaw > 360.0f) {
                        camera.yaw -= 360.0f;
                    } else if (camera.yaw < 0.0f) {
                        camera.yaw += 360.0f;
                    }
                    // Standard Y (not inverted)
                    camera.pitch += event.motion.yrel * MOUSE_SENSITIVITY;
                    if (camera.pitch > PITCH_MAX) {
                        camera.pitch = PITCH_MAX;
                    }
                    if (camera.pitch < PITCH_MIN) {
                        camera.pitch = PITCH_MIN;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        // Calculate delta time for this frame
        Uint32 now = SDL_GetTicks();
        float dt = (now - last_ticks) / 1000.0f; // delta time in seconds
        last_ticks = now;

        // WASD controls move the camera relative to view (crosshair)
        const Uint8* keystate = SDL_GetKeyboardState(NULL);
        bool sprint = (keystate[SDL_SCANCODE_LSHIFT] && keystate[SDL_SCANCODE_W]);
        float move_speed = BASE_SPEED * (sprint ? SPRINT_MULT : 1.0f);
        walk_frequency_current = sprint ? (WALK_FREQUENCY * 1.4f) : WALK_FREQUENCY;
        float fov_target = sprint ? SPRINT_FOV : FOV;
        fov_display += (fov_target - fov_display) * fminf(1.0f, dt * FOV_LERP_SPEED);
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

        // Ground-constrained movement: ignore forward.y (no flying/swimming towards the crosshair, just walking on the ground)
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
        if (keystate[SDL_SCANCODE_SPACE] && is_grounded) {
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

        // Apply vertical physics (gravity) every frame (positive velocity = upward)
        if (!is_grounded) {
            vertical_velocity -= GRAVITY * dt;
        } else if (vertical_velocity < 0.0f) {
            vertical_velocity = 0.0f;
        }

        float new_y = camera.y + vertical_velocity * dt;
        AABB box_y = player_aabb(camera.x, new_y, camera.z, PLAYER_RADIUS, PLAYER_HEIGHT, PLAYER_EYE_HEIGHT);
        if (aabb_intersects_map(&cubes, box_y, CUBE_SIZE, GRID_OFFSET_X, GRID_OFFSET_Y, GRID_OFFSET_Z)) {
            // Collide vertically
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
            camera.y = 50.0f;
            camera.z = 0.0f;
            camera.yaw = 0.0f;
            camera.pitch = 45.0f;
            vertical_velocity = 0.0f;
            is_grounded = false;
        }

        // Walking bob calculation for smooth start/stop (only when on ground and not jumping or falling)
        bool moving_input = (keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_D]);
        float target_amp = (is_grounded && moving_input) ? WALK_AMPLITUDE : 0.0f;
        walk_amp += (target_amp - walk_amp) * fminf(1.0f, dt * WALK_SMOOTH); // smooth amplitude
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

        // Build and draw all cube faces using Painter's Sorting
        // TODO: Backface culling to skip faces that are facing away from the camera
        size_t max_faces = cubes.size * 6;
        if (max_faces > faces_cap) {
            faces_cap = max_faces;
            faces = (Render_Face*)realloc(faces, faces_cap * sizeof(Render_Face));
        }
        size_t face_count = 0;

        size_t cubes_capacity = cube_map_capacity(&cubes);
        for (size_t ci = 0; ci < cubes_capacity; ++ci) {
            const Cube_Map_Entry* entry = cube_map_entry_at(&cubes, ci);
            if (!entry || !entry->occupied || !entry->cube) {
                continue;
            }

            Camera_Point cam_pts[8] = {0};
            compute_camera_points(entry->cube, cam_pts);

            for (size_t fi = 0; fi < 6; ++fi) {
                int i0 = FACE_INDICES[fi][0];
                int i1 = FACE_INDICES[fi][1];
                int i2 = FACE_INDICES[fi][2];
                int i3 = FACE_INDICES[fi][3];

                Camera_Point face_in[4] = {
                    cam_pts[i0],
                    cam_pts[i1],
                    cam_pts[i2],
                    cam_pts[i3]
                };

                // Near-plane clipping: clip each face polygon to z >= z_near.
                const float z_near = 0.05f;
                Camera_Point clipped[6] = {0};
                size_t clipped_count = clip_polygon_near(face_in, 4, z_near, clipped);
                if (clipped_count < 3) {
                    continue;
                }

                Projected_Point projected[6] = {0};
                for (size_t pi = 0; pi < clipped_count; ++pi) {
                    projected[pi] = project_to_screen(&clipped[pi]);
                }

                if (polygon_completely_offscreen(projected, clipped_count)) {
                    continue;
                }

                Render_Face* face = &faces[face_count++];
                face->vert_count = 0;
                face->line_count = 0;
                float depth_sum = 0.0f;
                for (size_t pi = 0; pi < clipped_count; ++pi) {
                    depth_sum += clipped[pi].z;
                }
                face->depth = depth_sum / (float)clipped_count;

                SDL_Color c = entry->cube->color;
                //SDL_Color c = (SDL_Color){ 0, 0, 0, 255 };
                face->color = entry->cube->color;
                c.a = 32;
                for (size_t tri = 1; tri + 1 < clipped_count; ++tri) {
                    size_t vbase = face->vert_count;
                    face->verts[vbase + 0] = (SDL_Vertex){ .position = {projected[0].x, projected[0].y}, .color = c, .tex_coord = {0.0f, 0.0f} };
                    face->verts[vbase + 1] = (SDL_Vertex){ .position = {projected[tri].x, projected[tri].y}, .color = c, .tex_coord = {0.0f, 0.0f} };
                    face->verts[vbase + 2] = (SDL_Vertex){ .position = {projected[tri + 1].x, projected[tri + 1].y}, .color = c, .tex_coord = {0.0f, 0.0f} };
                    face->vert_count += 3;
                }

                for (size_t pi = 0; pi < clipped_count && pi < 6; ++pi) {
                    face->line_pts[face->line_count++] = projected[pi];
                }
            }
        }

        if (face_count > 1) {
            qsort(faces, face_count, sizeof(Render_Face), compare_face_depth_desc);
        }

        size_t total_verts = 0;
        for (size_t i = 0; i < face_count; ++i) {
            total_verts += faces[i].vert_count;
        }

        if (total_verts > tri_cap) {
            tri_cap = total_verts;
            tri_verts = (SDL_Vertex*)realloc(tri_verts, tri_cap * sizeof(SDL_Vertex));
        }

        size_t write_index = 0;
        for (size_t i = 0; i < face_count; ++i) {
            for (size_t v = 0; v < faces[i].vert_count; ++v) {
                tri_verts[write_index++] = faces[i].verts[v];
            }
        }

        // Fill triangles in Painter's order (sorted by depth)
        if (total_verts > 0) {
            SDL_RenderGeometry(renderer, NULL, tri_verts, (int)total_verts, NULL, 0);
        }

        // Draw face outlines on top of filled faces for better visibility, also in Painter's order
        for (size_t i = 0; i < face_count; ++i) {
            Render_Face* face = &faces[i];
            if (face->line_count < 2) {
                continue;
            }
            SDL_SetRenderDrawColor(renderer, face->color.r, face->color.g, face->color.b, 255);
            const int outline_thickness = 3;
            for (size_t p = 0; p < face->line_count; ++p) {
                size_t next = (p + 1) % face->line_count;
                draw_line_thickness(
                    (int)SDL_roundf(face->line_pts[p].x),
                    (int)SDL_roundf(face->line_pts[p].y),
                    (int)SDL_roundf(face->line_pts[next].x),
                    (int)SDL_roundf(face->line_pts[next].y),
                    outline_thickness);
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

    free(tri_verts);
    free(faces);

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
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (OVERLAY_ON) {
        overlay_init(window, renderer);
    }

    return true;
}

// Creates a grid of cubes centered around (x, y, z) in world coordinates, with the specified size and color.
// Optionally leaves a hole in the middle.
void create_ground_grid(Cube_Map* map, int size, int x, int y, int z, SDL_Color color, int hole_size) {
    const float GRID_OFFSET_X = ((size - 1) * CUBE_SIZE) / 2.0f;
    const float GRID_OFFSET_Z = ((size - 1) * CUBE_SIZE) / 2.0f;
    const float GRID_OFFSET_Y = CUBE_SIZE * 0.5f;

    if (hole_size <= 0) {
        hole_size = 0;
    } else if (hole_size >= size) {
        hole_size = size - 1;
    } else {
        hole_size = (hole_size / 2) * 2 + 1; // ensure odd hole size for symmetry
    }
    size_t n_cubes = size * size;
    for (size_t i = 0; i < n_cubes; ++i) {
        int gx = i % size;
        int gz = i / size;
        if (hole_size > 0 && gx >= (size - hole_size) / 2 && gx < (size + hole_size) / 2 && gz >= (size - hole_size) / 2 && gz < (size + hole_size) / 2) {
            continue;
        }

        Point_3D center = {
            .x = x * CUBE_SIZE + gx * CUBE_SIZE - GRID_OFFSET_X,
            .y = y * CUBE_SIZE - GRID_OFFSET_Y,
            .z = z * CUBE_SIZE + gz * CUBE_SIZE - GRID_OFFSET_Z
        };
        Cube* new_cube = (Cube*)malloc(sizeof(Cube));
        make_cube(new_cube, CUBE_SIZE, center, color);

        Cube_Key key = {
            .x = world_to_grid_coord(center.x, CUBE_SIZE, GRID_OFFSET_X),
            .y = world_to_grid_coord(center.y, CUBE_SIZE, GRID_OFFSET_Y),
            .z = world_to_grid_coord(center.z, CUBE_SIZE, GRID_OFFSET_Z)
        };
        cube_map_add(map, key, new_cube);
    }
}