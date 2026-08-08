#include "imgui_bridge.h"

#include "diagnostics.h"
#include "osk_view.h"

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

#include "backends/imgui_impl_sdlrenderer2.h"

namespace {
SDL_Window *gl_window = nullptr;
SDL_Renderer *renderer_handle = nullptr;
bool gl_ready = false;
bool renderer_ready = false;

static void configure_font(const OmniConfig &config)
{
    if (config.font_path[0] != '\0') {
        const float size = config.font_size > 0.0f ? config.font_size : 16.0f;
        ImFont *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(config.font_path, size);
        if (font == nullptr) {
            omni_diag(OMNI_LOG_WARN, "OMNI_FONT could not be loaded; using Dear ImGui's built-in font");
        } else {
            ImGui::GetIO().FontDefault = font;
        }
    }
}
}

extern "C" int omni_imgui_gl_begin(SDL_Window *window, OmniRuntime *runtime)
{
    if (!runtime->context || window == nullptr) {
        return 0;
    }
    if (runtime->window != window || SDL_GL_GetCurrentContext() != runtime->context) {
        omni_diag_once(21, OMNI_LOG_WARN, "OpenGL presentation target is not the tracked window/context");
        return 0;
    }
    int profile = SDL_GL_CONTEXT_PROFILE_ES;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    if (profile != 0 && profile != SDL_GL_CONTEXT_PROFILE_ES) {
        omni_diag_once(25, OMNI_LOG_WARN, "OpenGL presentation is not an OpenGL ES context");
        return 0;
    }
    if (!gl_ready && renderer_ready) {
        omni_diag_once(18, OMNI_LOG_WARN, "OpenGL presentation ignored while SDL Renderer ImGui target is active");
        return 0;
    }
    if (!gl_ready) {
        int major = 2;
        const char *glsl_version;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
        glsl_version = major >= 3 ? "#version 300 es" : "#version 100";
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        configure_font(runtime->config);
        if (!ImGui_ImplSDL2_InitForOpenGL(window, runtime->context) ||
            !ImGui_ImplOpenGL3_Init(glsl_version)) {
            omni_diag_once(12, OMNI_LOG_ERROR, "Dear ImGui OpenGL initialization failed");
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
            return 0;
        }
        gl_window = window;
        gl_ready = true;
    }
    if (gl_window != window) {
        omni_diag_once(13, OMNI_LOG_WARN, "a second OpenGL window was presented; overlay remains pass-through");
        return 0;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    return 1;
}

extern "C" void omni_imgui_gl_draw(OmniRuntime *runtime)
{
    omni_osk_view_draw(runtime, 0);
}

extern "C" void omni_imgui_gl_end(void)
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

extern "C" void omni_imgui_gl_shutdown(void)
{
    if (!gl_ready) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    gl_window = nullptr;
    gl_ready = false;
}

extern "C" int omni_imgui_renderer_begin(SDL_Renderer *renderer, OmniRuntime *runtime)
{
    if (!renderer || runtime->window == nullptr) {
        return 0;
    }
    if (runtime->renderer != renderer) {
        omni_diag_once(22, OMNI_LOG_WARN, "SDL Renderer presentation target is not the tracked renderer");
        return 0;
    }
    if (!renderer_ready && gl_ready) {
        omni_diag_once(19, OMNI_LOG_WARN, "SDL Renderer presentation ignored while OpenGL ImGui target is active");
        return 0;
    }
    if (!renderer_ready) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        configure_font(runtime->config);
        if (!ImGui_ImplSDL2_InitForSDLRenderer(runtime->window, renderer) ||
            !ImGui_ImplSDLRenderer2_Init(renderer)) {
            omni_diag_once(14, OMNI_LOG_ERROR, "Dear ImGui SDL Renderer initialization failed");
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
            return 0;
        }
        renderer_handle = renderer;
        renderer_ready = true;
    }
    if (renderer_handle != renderer) {
        omni_diag_once(15, OMNI_LOG_WARN, "a second SDL Renderer was presented; overlay remains pass-through");
        return 0;
    }
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    return 1;
}

extern "C" int omni_imgui_other_target_active(int renderer_path)
{
    (void)renderer_path;
    return gl_ready || renderer_ready;
}

extern "C" void omni_imgui_gl_invalidate(void)
{
    ImGuiContext *context;
    if (!gl_ready) {
        return;
    }
    // The owning GL context is already gone and may not be current. Avoid backend
    // shutdown calls here; they would issue GL deletes against an unrelated context.
    context = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(nullptr);
    ImGui::DestroyContext(context);
    gl_window = nullptr;
    gl_ready = false;
}

extern "C" void omni_imgui_renderer_draw(OmniRuntime *runtime, SDL_Renderer *renderer)
{
    omni_osk_view_draw(runtime, 1);
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
}

extern "C" void omni_imgui_renderer_end(void)
{
}

extern "C" void omni_imgui_renderer_shutdown(void)
{
    if (!renderer_ready) {
        return;
    }
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    renderer_handle = nullptr;
    renderer_ready = false;
}
