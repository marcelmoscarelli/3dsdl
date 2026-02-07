#include <stdbool.h>
#include <stdint.h>

// Global consts for window dimensions and frame rate
const int WIDTH = 1600;
const int HEIGHT = 900;
const int TARGET_FPS = 120;
const uint32_t FRAME_DELAY = (1000u / TARGET_FPS);

// Global consts for stats overlay
const bool OVERLAY_ON = true;

// Global consts for camera
const float ASPECT_RATIO = (float)WIDTH / (float)HEIGHT;
const float FOV_STEP = 1.0f;
const float FOV_MIN = 30.0f;
const float FOV_MAX = 120.0f;
const float PITCH_MAX = 89.0f;
const float PITCH_MIN = -89.0f;

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