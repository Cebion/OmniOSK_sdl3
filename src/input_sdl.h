#ifndef OMNI_INPUT_SDL_H
#define OMNI_INPUT_SDL_H

#include "generated_events.h"
#include "osk_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OmniSdlInput {
    int toggle_down;
    Uint8 held[SDL_NUM_SCANCODES];
    Uint8 pass_release[SDL_NUM_SCANCODES];
    Uint8 suppressed[SDL_NUM_SCANCODES];
} OmniSdlInput;

void omni_sdl_input_init(OmniSdlInput *input);
int omni_sdl_input_handle(OmniSdlInput *input, const OmniConfig *config,
                          OmniOskModel *model, OmniGeneratedQueue *generated,
                          SDL_Event *event, int text_input_enabled);

#ifdef __cplusplus
}
#endif

#endif
