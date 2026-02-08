// settings.h - global consts for configuring aspects of 3dsdl
#ifndef SETTINGS_H
#define SETTINGS_H
#include <stdint.h>

// Global consts for window dimensions and frame rate
extern const int WIDTH;
extern const int HEIGHT;
extern const int TARGET_FPS;
extern const uint32_t FRAME_DELAY;

// Global consts for stats overlay
extern const bool OVERLAY_ON;

// Global consts for camera
extern const float ASPECT_RATIO;
extern const float FOV;
extern const float SPRINT_FOV;
extern const float FOV_LERP_SPEED;
extern const float PITCH_MAX;
extern const float PITCH_MIN;
extern const float MOUSE_SENSITIVITY;

// Global consts for cubes
extern const float CUBE_SIZE;

// Falling / gravity
extern const float GRAVITY;
extern const float FALL_RESET_DISTANCE;
extern const float JUMP_IMPULSE;

// Player collider (AABB) parameters
extern const float PLAYER_HEIGHT;
extern const float PLAYER_EYE_HEIGHT;
extern const float PLAYER_RADIUS;
extern const float GROUND_EPS;

// Player walking parameters
extern const float WALK_AMPLITUDE;
extern const float WALK_FREQUENCY;
extern const float WALK_SMOOTH;
extern const float BASE_SPEED;
extern const float SPRINT_MULT;

#endif