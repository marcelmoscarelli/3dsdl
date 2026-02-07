#ifndef RENDERING_H
#define RENDERING_H

// Prototypes
void draw_line_thickness(int x1, int y1, int x2, int y2, int thickness);
void draw_crosshair(int thickness, int size);
void compute_camera_points(const Cube *cube, Camera_Point out[8]);
Projected_Point project_to_screen(const Camera_Point *p);
size_t clip_polygon_near(const Camera_Point* in_pts, size_t in_count, float z_near, Camera_Point* out_pts);
bool polygon_completely_offscreen(const Projected_Point* pts, size_t count);
int compare_face_depth_desc(const void* a, const void* b);

#endif