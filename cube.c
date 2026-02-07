#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "cube.h"
#include "settings.h"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern Camera camera;          // defined in main.c
extern SDL_Renderer* renderer; // defined in main.c

// Define the points of an y axis-aligned cube centered at `center` with given `size`
void make_cube(Cube* cube, float size, Point3D center, SDL_Color color) {
    if (!cube) {
        printf("make_cube(): NULL cube pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }
    float half_size = size / 2.0f;

    // Front face
    cube->points[0] = (Point3D){center.x - half_size, center.y - half_size, center.z - half_size}; // A
    cube->points[1] = (Point3D){center.x + half_size, center.y - half_size, center.z - half_size}; // B
    cube->points[2] = (Point3D){center.x + half_size, center.y + half_size, center.z - half_size}; // C
    cube->points[3] = (Point3D){center.x - half_size, center.y + half_size, center.z - half_size}; // D

    // Back face
    cube->points[4] = (Point3D){center.x - half_size, center.y - half_size, center.z + half_size}; // E
    cube->points[5] = (Point3D){center.x + half_size, center.y - half_size, center.z + half_size}; // F
    cube->points[6] = (Point3D){center.x + half_size, center.y + half_size, center.z + half_size}; // G
    cube->points[7] = (Point3D){center.x - half_size, center.y + half_size, center.z + half_size}; // H

    cube->color = color;
}

// Rotate around arbitrary axis through cube center using Rodrigues' rotation formula
void rotate_cube(Cube* cube, Point3D axis, float angle) {
    if (!cube) {
        printf("rotate_cube(): NULL cube pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }

    float center_x = 0.0f, center_y = 0.0f, center_z = 0.0f;
    for (size_t i = 0; i < 8; ++i) {
        center_x += cube->points[i].x;
        center_y += cube->points[i].y;
        center_z += cube->points[i].z;
    }
    center_x /= 8.0f;
    center_y /= 8.0f;
    center_z /= 8.0f;

    float ax = axis.x;
    float ay = axis.y;
    float az = axis.z;
    float axis_len = sqrtf(ax*ax + ay*ay + az*az);
    if (axis_len < 1e-6f) { // avoid division by zero
        return;
    }
    float kx = ax / axis_len;
    float ky = ay / axis_len;
    float kz = az / axis_len;

    float cos = cosf(angle);
    float sin = sinf(angle);

    for (size_t i = 0; i < 8; ++i) {
        // vector from center to point
        float vx = cube->points[i].x - center_x;
        float vy = cube->points[i].y - center_y;
        float vz = cube->points[i].z - center_z;

        // Compute the vector perpendicular to the axis vector (k) and the vector from the center to the point (v), used for rotation
        float cx = ky * vz - kz * vy;
        float cy = kz * vx - kx * vz;
        float cz = kx * vy - ky * vx;

        // Compute the projection of v onto k
        float kd = kx * vx + ky * vy + kz * vz;

        // Rodrigues' rotation formula
        float rx = vx * cos + cx * sin + kx * kd * (1.0f - cos);
        float ry = vy * cos + cy * sin + ky * kd * (1.0f - cos);
        float rz = vz * cos + cz * sin + kz * kd * (1.0f - cos);

        cube->points[i].x = center_x + rx;
        cube->points[i].y = center_y + ry;
        cube->points[i].z = center_z + rz;
    }
}

// Draw a line with integer thickness by drawing several parallel lines.
static void draw_line_thickness(int x1, int y1, int x2, int y2, int thickness) {
    if (thickness < 1) {
        thickness = 1;
    }
    int dx = x2 - x1;
    int dy = y2 - y1;
    int start = -(thickness / 2);
    int end = start + thickness - 1;

    if (abs(dx) > abs(dy)) {
        // More horizontal: offset in Y
        for (int off = start; off <= end; ++off) {
            SDL_RenderDrawLine(renderer, x1, y1 + off, x2, y2 + off);
        }
    } else {
        // More vertical (or equal): offset in X
        for (int off = start; off <= end; ++off) {
            SDL_RenderDrawLine(renderer, x1 + off, y1, x2 + off, y2);
        }
    }
}

// Draw the cube by projecting its 3D points to 2D screen space according to the current camera parameters
void draw_cube(const Cube* cube) {
    if (!cube) {
        printf("draw_cube(): NULL cube pointer. Exiting!\n");
        exit(EXIT_FAILURE);
    }

    /* Precompute camera rotation */
    float yaw_rad = camera.yaw * (M_PI / 180.0f);
    float pitch_rad = camera.pitch * (M_PI / 180.0f);
    float yaw_cos = cosf(yaw_rad);
    float yaw_sin = sinf(yaw_rad);
    float pitch_cos = cosf(pitch_rad);
    float pitch_sin = sinf(pitch_rad);

    Point2D projected[8] = {0};
    bool skip_point[8] = {false};
    for (size_t i = 0; i < 8; ++i) {
        // Translate point to camera space
        float rel_x = cube->points[i].x - camera.x;
        float rel_y = cube->points[i].y - camera.y;
        float rel_z = cube->points[i].z - camera.z;

        // Inverse yaw
        float x1 =  yaw_cos * rel_x - yaw_sin * rel_z;
        float z1 =  yaw_sin * rel_x + yaw_cos * rel_z;

        // Inverse pitch
        float y2 =  pitch_cos * rel_y + pitch_sin * z1;
        float z2 = -pitch_sin * rel_y + pitch_cos * z1;

        // Skip points behind the camera
        if (z2 <= 0) {
            skip_point[i] = true;
            continue;
        }

        // Project to Normalized Device Coordinates (NDC) space
        float x_ndc = (x1 * camera.focal_length / ASPECT_RATIO) / z2;
        float y_ndc = (y2 * camera.focal_length) / z2;
        projected[i].x = (int)SDL_roundf((x_ndc + 1.0f) * 0.5f * WIDTH);
        projected[i].y = (int)SDL_roundf((1.0f - (y_ndc + 1.0f) * 0.5f) * HEIGHT);
    }

    // Draw cube edges if both endpoints are visible
    int thickness = 3;
    SDL_SetRenderDrawColor(renderer, cube->color.r, cube->color.g, cube->color.b, cube->color.a);
    for (size_t i = 0; i < 4; ++i) {
        int next_i = (i + 1) % 4;
        if (!skip_point[i] && !skip_point[next_i]) {
            draw_line_thickness((int)projected[i].x, (int)projected[i].y, (int)projected[next_i].x, (int)projected[next_i].y, thickness);
        }
        if (!skip_point[i + 4] && !skip_point[next_i + 4]) {
            draw_line_thickness((int)projected[i + 4].x, (int)projected[i + 4].y, (int)projected[next_i + 4].x, (int)projected[next_i + 4].y, thickness);
        }
        if (!skip_point[i] && !skip_point[i + 4]) {
            draw_line_thickness((int)projected[i].x, (int)projected[i].y, (int)projected[i + 4].x, (int)projected[i + 4].y, thickness);
        }
    }
}