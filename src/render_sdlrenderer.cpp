#include "render_sdlrenderer.h"

#include "diagnostics.h"
#include "imgui_bridge.h"

extern "C" int omni_render_sdlrenderer(SDL_Renderer *renderer, OmniRuntime *runtime)
{
    SDL_Texture *target;
    if (renderer == nullptr) {
        return 0;
    }
    target = SDL_GetRenderTarget(renderer);
    if (target != nullptr) {
        omni_diag_once(16, OMNI_LOG_DEBUG, "texture render target active; skipping OSK for this present");
        return 0;
    }
    if (!omni_imgui_renderer_begin(renderer, runtime)) {
        if (runtime->model.active && !omni_imgui_other_target_active(1)) {
            omni_runtime_disable(runtime, "SDL Renderer presentation initialization");
        }
        return 0;
    }
    omni_imgui_renderer_draw(runtime, renderer);
    omni_imgui_renderer_end();
    return 1;
}

extern "C" void omni_render_sdlrenderer_shutdown(void)
{
    omni_imgui_renderer_shutdown();
}
