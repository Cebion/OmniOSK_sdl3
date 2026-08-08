#include "render_gl.h"

#include "diagnostics.h"
#include "gl_loader.h"
#include "imgui_bridge.h"

namespace {
OmniGlFunctions gl_functions;
bool gl_functions_loaded = false;
}

extern "C" int omni_render_gl(SDL_Window *window, OmniRuntime *runtime)
{
    if (!gl_functions_loaded) {
        if (omni_gl_loader_init(&gl_functions) != 0) {
            omni_runtime_disable(runtime, "OpenGL entry-point loading");
            return 0;
        }
        gl_functions_loaded = true;
    }
    if (!omni_imgui_gl_begin(window, runtime)) {
        if (runtime->model.active && !omni_imgui_other_target_active(0)) {
            omni_runtime_disable(runtime, "OpenGL presentation initialization");
        }
        return 0;
    }
    omni_imgui_gl_draw(runtime);
    omni_imgui_gl_end();
    return 1;
}

extern "C" void omni_render_gl_shutdown(void)
{
    omni_imgui_gl_shutdown();
    gl_functions_loaded = false;
}
