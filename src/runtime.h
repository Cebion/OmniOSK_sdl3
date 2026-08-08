#ifndef OMNI_RUNTIME_H
#define OMNI_RUNTIME_H

#include "input_sdl.h"
#include "input_evdev.h"
#include "symbols.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OmniRuntimeState {
    OMNI_RUNTIME_UNINITIALIZED,
    OMNI_RUNTIME_PROBING,
    OMNI_RUNTIME_READY,
    OMNI_RUNTIME_DISABLED,
    OMNI_RUNTIME_SHUTTING_DOWN
} OmniRuntimeState;

typedef struct OmniRuntime {
    OmniRuntimeState state;
    OmniConfig config;
    OmniOskModel model;
    OmniSdlInput sdl_input;
    OmniGeneratedQueue generated;
    OmniEvdevBackend evdev;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_GLContext context;
    int initialized;
    int evdev_enabled;
} OmniRuntime;

int omni_runtime_ensure(OmniRuntime *runtime);
void omni_runtime_shutdown(OmniRuntime *runtime);
int omni_runtime_process_event(OmniRuntime *runtime, SDL_Event *event, int text_enabled);
void omni_runtime_tick(OmniRuntime *runtime);
int omni_runtime_pop_generated(OmniRuntime *runtime, SDL_Event *event);
int omni_runtime_peek_generated(const OmniRuntime *runtime, SDL_Event *event);
int omni_runtime_active(const OmniRuntime *runtime);
int omni_runtime_has_presentation(const OmniRuntime *runtime);
void omni_runtime_set_window(OmniRuntime *runtime, SDL_Window *window);
void omni_runtime_set_renderer(OmniRuntime *runtime, SDL_Renderer *renderer);
void omni_runtime_set_context(OmniRuntime *runtime, SDL_GLContext context);
void omni_runtime_presentation_lost(OmniRuntime *runtime);
void omni_runtime_disable(OmniRuntime *runtime, const char *reason);

#ifdef __cplusplus
}
#endif

#endif
