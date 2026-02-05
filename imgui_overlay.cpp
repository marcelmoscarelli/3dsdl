// Simple C-callable wrapper to use Dear ImGui with an SDL_Renderer-based backend.
#include <SDL2/SDL.h>
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
};

static OverlayStats g_stats = {0,0,0,0,0,0,0,0};

void overlay_init(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Load an external font
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF("../assets/DejaVuSansMono.ttf", 18.0f);
    if (font) {
        ImGui::GetIO().FontDefault = font;
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

void overlay_set_stats(float x, float y, float z, float yaw, float pitch, float fov, float speed) {
    g_stats.x = x; g_stats.y = y; g_stats.z = z;
    g_stats.yaw = yaw; g_stats.pitch = pitch;
    g_stats.fov = fov;
    g_stats.speed = speed;
}

void overlay_newframe() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Small top-left overlay window
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::SetNextWindowPos(ImVec2(10,10), ImGuiCond_Always);
    ImGui::Begin("Overlay", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove /*| ImGuiWindowFlags_NoTitleBar*/);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Pos.: (x:%.2f, y:%.2f, z:%.2f)", g_stats.x, g_stats.y, g_stats.z);
    ImGui::Text("Yaw: %.1f Pitch: %.1f", g_stats.yaw, g_stats.pitch);
    ImGui::Text("FOV: %.1f degrees", g_stats.fov);
    ImGui::Text("Speed: %.2f u/s", g_stats.speed);
    ImGui::End();
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
