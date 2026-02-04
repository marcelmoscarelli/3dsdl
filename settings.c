#include <stdint.h>

// Global consts for window dimensions and frame rate
const int WIDTH = 1280;
const int HEIGHT = 720;
const int TARGET_FPS = 120;
const uint32_t FRAME_DELAY = (1000u / TARGET_FPS);

// Global vars/consts for camera
const float ASPECT_RATIO = (float)WIDTH / (float)HEIGHT;
const float FOV_STEP = 1.0f;
const float FOV_MIN = 30.0f;
const float FOV_MAX = 120.0f;
const float PITCH_MAX = 89.0f;
const float PITCH_MIN = -89.0f;