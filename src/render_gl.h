#ifndef OMNI_RENDER_GL_H
#define OMNI_RENDER_GL_H

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

int omni_render_gl(SDL_Window *window, OmniRuntime *runtime);
void omni_render_gl_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
