#include <stdbool.h>
#include <stdint.h>

// Global consts for window dimensions and frame rate
const int WIDTH = 1600;
const int HEIGHT = 900;
const int TARGET_FPS = 120;
const uint32_t FRAME_DELAY = (1000u / TARGET_FPS);

// Global consts for stats overlay
const bool OVERLAY_ON = true;
const float OVERLAY_INTERVAL = 0.1f;

// Global vars/consts for camera
const float ASPECT_RATIO = (float)WIDTH / (float)HEIGHT;
const float FOV_STEP = 1.0f;
const float FOV_MIN = 30.0f;
const float FOV_MAX = 120.0f;
const float PITCH_MAX = 89.0f;
const float PITCH_MIN = -89.0f;