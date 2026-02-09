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
#include "settings.h"

extern "C" {

extern bool show_controls_in_overlay; // defined in main.c
static SDL_Renderer* aux_renderer = nullptr;
const char* fps_cap = (FRAME_DELAY > 0) ? "" : "uncapped";

struct Overlay_Stats {
    int fps;
    float x, y, z;
    float yaw, pitch;
    float fov;
    float speed;
    size_t cube_count;
    size_t map_capacity;
    int win_width;
    int win_height;
};

static Overlay_Stats stats = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

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
    aux_renderer = renderer;
}

void overlay_process_event(SDL_Event* event) {
    // Forward events to ImGui
    ImGui_ImplSDL2_ProcessEvent(event);
}

void overlay_set_stats(float x, float y, float z, float yaw, float pitch, float fov, size_t cube_map_size, size_t cube_map_capacity, int win_width, int win_height) {
    stats.x = x; stats.y = y; stats.z = z;
    stats.yaw = yaw; stats.pitch = pitch;
    stats.fov = fov;
    stats.cube_count = cube_map_size;
    stats.map_capacity = cube_map_capacity;
    stats.win_width = win_width;
    stats.win_height = win_height;
}

void overlay_newframe() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Small top-left overlay window
    ImGui::SetNextWindowPos(ImVec2(10,10), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
    ImGui::Begin("Overlay", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove /*| ImGuiWindowFlags_NoTitleBar*/);
    ImGui::Text("FPS: %.1f (%.2f ms) %s", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate, fps_cap);
    ImGui::Text("Cam. Pos.: (x:%.2f, y:%.2f, z:%.2f)", stats.x, stats.y, stats.z);
    ImGui::Text("Cam. View: (yaw:%.1f, pitch:%.1f, fov:%.1f)", stats.yaw, stats.pitch, stats.fov);
    ImGui::Text("Cube Map: (cubes:%zu, size:%zu)", stats.cube_count, stats.map_capacity);
    ImGui::Text("Current res.: %dx%d pixels", stats.win_width, stats.win_height);
    ImGui::Separator();
    if (show_controls_in_overlay) {
        ImGui::Text("Controls:");
        ImGui::Text("- WASD to move");
        ImGui::Text("- Mouse to look around");
        ImGui::Text("- Left shift to sprint");
        ImGui::Text("- Space to jump");
        ImGui::Text("- Esc to toggle mouse capture");
        ImGui::Text("- Alt+Enter to toggle fullscreen");
    } else {
        ImGui::Text("Help with controls? Press Backspace.");
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void overlay_render() {
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), aux_renderer);
}

void overlay_shutdown() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

} // extern "C"
