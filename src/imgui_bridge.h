#ifndef OMNI_IMGUI_BRIDGE_H
#define OMNI_IMGUI_BRIDGE_H

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

int omni_imgui_gl_begin(SDL_Window *window, OmniRuntime *runtime);
int omni_imgui_other_target_active(int renderer_path);
void omni_imgui_gl_invalidate(void);
void omni_imgui_gl_draw(OmniRuntime *runtime);
void omni_imgui_gl_end(void);
void omni_imgui_gl_shutdown(void);

int omni_imgui_renderer_begin(SDL_Renderer *renderer, OmniRuntime *runtime);
void omni_imgui_renderer_draw(OmniRuntime *runtime, SDL_Renderer *renderer);
void omni_imgui_renderer_end(void);
void omni_imgui_renderer_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
