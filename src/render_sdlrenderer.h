#ifndef OMNI_RENDER_SDLRENDERER_H
#define OMNI_RENDER_SDLRENDERER_H

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

int omni_render_sdlrenderer(SDL_Renderer *renderer, OmniRuntime *runtime);
void omni_render_sdlrenderer_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
