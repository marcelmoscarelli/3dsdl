// Simple C-callable wrapper to use Dear ImGui with an SDL_Renderer-based backend.
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <SDL2/SDL.h>
#include "imgui_overlay.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_sdlrenderer2.h"

extern "C" {

static SDL_Renderer* g_renderer = nullptr;

struct OverlayStats {
    int fps;
    float x, y, z;
    float yaw, pitch;
    float fov;
    float speed;
    size_t cube_count;
    size_t map_capacity;
};

static OverlayStats g_stats = {0,0,0,0,0,0,0,0,0,0};

void overlay_init(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    // Load an external font
    char* base = SDL_GetBasePath();
    std::string base_str(base);
    SDL_free(base);
    std::replace(base_str.begin(), base_str.end(), '\\', '/'); // replace backslashes with forward slashes (not really needed, but...)
    if (base_str.empty()) {
        std::cout << "SDL_GetBasePath failed: " << SDL_GetError() << std::endl;
        std::exit(EXIT_FAILURE);
    }
    std::string font_path = base_str + "../assets/DejaVuSansMono.ttf";
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(font_path.c_str(), 18.0f);
    if (font) {
        ImGui::GetIO().FontDefault = font;
        std::cout << "Font loaded from: " << font_path << std::endl;
    } else {
        std::cout << "AddFontFromFileTTF() failed to load font at path: " << font_path << std::endl;
        exit(EXIT_FAILURE);
    }

    // Initialize platform/renderer backends for SDL renderer
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
    g_renderer = renderer;
}

void overlay_process_event(SDL_Event* event) {
    // Forward events to ImGui
    ImGui_ImplSDL2_ProcessEvent(event);
}

void overlay_set_stats(float x, float y, float z, float yaw, float pitch, float fov, size_t cube_map_size, size_t cube_map_capacity) {
    g_stats.x = x; g_stats.y = y; g_stats.z = z;
    g_stats.yaw = yaw; g_stats.pitch = pitch;
    g_stats.fov = fov;
    g_stats.cube_count = cube_map_size;
    g_stats.map_capacity = cube_map_capacity;
}

void overlay_newframe() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Small top-left overlay window
    ImGui::SetNextWindowPos(ImVec2(10,10), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
    ImGui::Begin("Overlay", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove /*| ImGuiWindowFlags_NoTitleBar*/);
    ImGui::Text("FPS: %.1f (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Text("Cam. Pos.: (x:%.2f, y:%.2f, z:%.2f)", g_stats.x, g_stats.y, g_stats.z);
    ImGui::Text("Cam. View: (yaw:%.1f, pitch:%.1f, fov:%.1f)", g_stats.yaw, g_stats.pitch, g_stats.fov);
    ImGui::Text("Cube Map: (cubes:%zu, size:%zu)", g_stats.cube_count, g_stats.map_capacity);
    ImGui::Separator();
    ImGui::Text("Use WASD to move, mouse to look around,");
    ImGui::Text("left shift to sprint and space to jump.");
    ImGui::End();
    ImGui::PopStyleColor();
}

void overlay_render() {
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), g_renderer);
}

void overlay_shutdown() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

} // extern "C"
