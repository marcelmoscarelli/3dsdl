#ifndef IMGUI_OVERLAY_H
#define IMGUI_OVERLAY_H

#include <stddef.h>
#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

void overlay_init(SDL_Window* window, SDL_Renderer* renderer);
void overlay_process_event(SDL_Event* event);
void overlay_newframe();
void overlay_render();
void overlay_shutdown();
void overlay_set_stats(float x, float y, float z, float yaw, float pitch, float fov, size_t cube_map_size, size_t cube_map_capacity);

#ifdef __cplusplus
}
#endif

#endif
