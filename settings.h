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
extern const float OVERLAY_INTERVAL;

// Global consts for camera
extern const float ASPECT_RATIO;
extern const float FOV_STEP;
extern const float FOV_MIN;
extern const float FOV_MAX;
extern const float PITCH_MAX;
extern const float PITCH_MIN;

#endif