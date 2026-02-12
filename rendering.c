#include <SDL2/SDL.h>
#include "data_structures.h"
#include "settings.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// defined in main.c
extern Camera camera;          
extern SDL_Renderer *renderer;
extern int win_width;
extern int win_height;

// Helper function to clamp a float value between a minimum and maximum.
static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int get_horizon_y(void) {
    float pitch_rad = -camera.pitch * (M_PI / 180.0f);
    float y_ndc = tanf(pitch_rad) * camera.focal_length;
    float horizon_f = (0.5f + 0.5f * y_ndc) * (float)win_height;
    horizon_f = clampf(horizon_f, 0.0f, (float)win_height);
    return (int)SDL_roundf(horizon_f);
}

// Render a simple sky gradient above the horizon only.
void render_sky(void)
{
    if (!renderer || win_width <= 0 || win_height <= 0) {
        return;
    }

    int horizon_y = get_horizon_y();

    SDL_Color top = { 8, 35, 95, 255 };
    SDL_Color near_horizon = { 95, 120, 150, 255 };

    if (horizon_y > 0) {
        SDL_Vertex verts[4] = {
            { .position = { 0.0f, 0.0f }, .color = top, .tex_coord = {0.0f, 0.0f} },
            { .position = { (float)win_width, 0.0f }, .color = top, .tex_coord = {0.0f, 0.0f} },
            { .position = { (float)win_width, (float)horizon_y }, .color = near_horizon, .tex_coord = {0.0f, 0.0f} },
            { .position = { 0.0f, (float)horizon_y }, .color = near_horizon, .tex_coord = {0.0f, 0.0f} }
        };
        int indices[6] = { 0, 1, 2, 0, 2, 3 };
        SDL_RenderGeometry(renderer, NULL, verts, 4, indices, 6);
    }

}

// Render a simple ground gradient below the horizon only.
void render_ground_placeholder(void)
{
    if (!renderer || win_width <= 0 || win_height <= 0) {
        return;
    }

    int horizon_y = get_horizon_y();
    if (horizon_y >= win_height) {
        return;
    }

    SDL_Color near_horizon = { 50, 75, 40, 255 };
    SDL_Color bottom = { 12, 35, 10, 255 };

    SDL_Vertex verts[4] = {
        { .position = { 0.0f, (float)horizon_y }, .color = near_horizon, .tex_coord = {0.0f, 0.0f} },
        { .position = { (float)win_width, (float)horizon_y }, .color = near_horizon, .tex_coord = {0.0f, 0.0f} },
        { .position = { (float)win_width, (float)win_height }, .color = bottom, .tex_coord = {0.0f, 0.0f} },
        { .position = { 0.0f, (float)win_height }, .color = bottom, .tex_coord = {0.0f, 0.0f} }
    };
    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(renderer, NULL, verts, 4, indices, 6);
}

// Draw a line with integer thickness by drawing several parallel lines.
void draw_line_thickness(int x1, int y1, int x2, int y2, int thickness)
{
    if (thickness < 1)
    {
        thickness = 1;
    }
    int dx = x2 - x1;
    int dy = y2 - y1;
    int start = -(thickness / 2);
    int end = start + thickness - 1;

    if (abs(dx) > abs(dy))
    {
        // More horizontal: offset in Y
        for (int off = start; off <= end; ++off)
        {
            SDL_RenderDrawLine(renderer, x1, y1 + off, x2, y2 + off);
        }
    }
    else
    {
        // More vertical (or equal): offset in X
        for (int off = start; off <= end; ++off)
        {
            SDL_RenderDrawLine(renderer, x1 + off, y1, x2 + off, y2);
        }
    }
}

// Draw crosshair in the center of the screen
void draw_crosshair(int thickness, int size)
{
    if (thickness <= 0 || size <= 0)
    {
        return;
    }
    // Round up to next odd number if even to ensure a single central pixel
    if ((thickness & 1) == 0)
    {
        thickness += 1;
    }
    if ((size & 1) == 0)
    {
        size += 1;
    }

    int cx = win_width / 2;
    int cy = win_height / 2;
    int half = size / 2;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    draw_line_thickness(cx - half, cy, cx+half, cy, thickness);
    draw_line_thickness(cx, cy - half, cx, cy+half, thickness);
}

// Compute the 8 cube points in camera space after applying camera translation + yaw/pitch rotation.
void compute_camera_points(const Cube *cube, Camera_Point out[8]) {
    float yaw_rad = camera.yaw * (M_PI / 180.0f);
    float pitch_rad = camera.pitch * (M_PI / 180.0f);
    float yaw_cos = cosf(yaw_rad);
    float yaw_sin = sinf(yaw_rad);
    float pitch_cos = cosf(pitch_rad);
    float pitch_sin = sinf(pitch_rad);

    for (size_t i = 0; i < 8; ++i)
    {
        float rel_x = cube->points[i].x - camera.x;
        float rel_y = cube->points[i].y - camera.y;
        float rel_z = cube->points[i].z - camera.z;

        float x1 = yaw_cos * rel_x - yaw_sin * rel_z;
        float z1 = yaw_sin * rel_x + yaw_cos * rel_z;

        float y2 = pitch_cos * rel_y + pitch_sin * z1;
        float z2 = -pitch_sin * rel_y + pitch_cos * z1;

        out[i].x = x1;
        out[i].y = y2;
        out[i].z = z2;
    }
}

// Project a camera space point to screen space.
Projected_Point project_to_screen(const Camera_Point *p) {
    float aspect_ratio = (float)win_width / (float)win_height;
    float x_ndc = (p->x * camera.focal_length / aspect_ratio) / p->z;
    float y_ndc = (p->y * camera.focal_length) / p->z;
    float sx = (x_ndc + 1.0f) * 0.5f * win_width;
    float sy = (1.0f - (y_ndc + 1.0f) * 0.5f) * win_height;
    return (Projected_Point){.x = sx, .y = sy};
}

// Near-plane clipping (Sutherland-Hodgman against z >= z_near).
size_t clip_polygon_near(const Camera_Point* in_pts, size_t in_count, float z_near, Camera_Point* out_pts) {
    if (in_count == 0) {
        return 0;
    }

    size_t out_count = 0;
    Camera_Point prev = in_pts[in_count - 1];
    bool prev_inside = prev.z >= z_near;

    for (size_t i = 0; i < in_count; ++i) {
        Camera_Point curr = in_pts[i];
        bool curr_inside = curr.z >= z_near;

        if (curr_inside && prev_inside) {
            out_pts[out_count++] = curr;
        } else if (prev_inside && !curr_inside) {
            float t = (z_near - prev.z) / (curr.z - prev.z);
            Camera_Point intersect = {
                .x = prev.x + (curr.x - prev.x) * t,
                .y = prev.y + (curr.y - prev.y) * t,
                .z = z_near
            };
            out_pts[out_count++] = intersect;
        } else if (!prev_inside && curr_inside) {
            float t = (z_near - prev.z) / (curr.z - prev.z);
            Camera_Point intersect = {
                .x = prev.x + (curr.x - prev.x) * t,
                .y = prev.y + (curr.y - prev.y) * t,
                .z = z_near
            };
            out_pts[out_count++] = intersect;
            out_pts[out_count++] = curr;
        }

        prev = curr;
        prev_inside = curr_inside;
    }

    return out_count;
}

// Check if a polygon is completely offscreen.
bool polygon_completely_offscreen(const Projected_Point* pts, size_t count) {
    bool all_left = true;
    bool all_right = true;
    bool all_top = true;
    bool all_bottom = true;

    for (size_t i = 0; i < count; ++i) {
        all_left = all_left && (pts[i].x < 0.0f);
        all_right = all_right && (pts[i].x > (float)win_width);
        all_top = all_top && (pts[i].y < 0.0f);
        all_bottom = all_bottom && (pts[i].y > (float)win_height);
    }

    return all_left || all_right || all_top || all_bottom;
}

// Comparison function for qsort to sort faces by depth descending.
int compare_face_depth_desc(const void* a, const void* b) {
    const Render_Face* fa = (const Render_Face*)a;
    const Render_Face* fb = (const Render_Face*)b;
    if (fa->depth < fb->depth) {
        return 1;
    }
    if (fa->depth > fb->depth) {
        return -1;
    }
    return 0;
}