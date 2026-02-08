#include <stdbool.h>
#include <stdint.h>

// Global consts for window dimensions and frame rate
const int WIDTH = 1600;
const int HEIGHT = 900;
const int TARGET_FPS = 240;
const uint32_t FRAME_DELAY = (1000u / TARGET_FPS);

// Global consts for stats overlay
const bool OVERLAY_ON = true;

// Global consts for camera
const float ASPECT_RATIO = (float)WIDTH / (float)HEIGHT;
const float FOV = 60.0f;
const float SPRINT_FOV = 70.0f;
const float FOV_LERP_SPEED = 6.0f;
const float PITCH_MAX = 89.0f;
const float PITCH_MIN = -89.0f;
const float MOUSE_SENSITIVITY = 0.1f; // degrees per pixel

// Global consts for cubes
const float CUBE_SIZE = 2.0f;

// Gravity, falling and jumping
const float GRAVITY = 30.0f;
const float FALL_RESET_DISTANCE = 200.0f;
const float JUMP_IMPULSE = 17.5f;

// Player collider (AABB) parameters
const float PLAYER_HEIGHT = 2.0f;
const float PLAYER_EYE_HEIGHT = 2.0f;
const float PLAYER_RADIUS = 0.4f;
const float GROUND_EPS = 0.05f;

// Player walking parameters
const float WALK_AMPLITUDE = 0.15f; // meters
const float WALK_FREQUENCY = 2.25f; // steps per second (base)
const float WALK_SMOOTH = 8.0f;     // smoothing rate
const float BASE_SPEED = 5.0f;      // units per second (base)
const float SPRINT_MULT = 2.0f;     // how much faster when sprinting