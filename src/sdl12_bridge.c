/* Compiled against real SDL 1.2 headers (see CMakeLists.txt -
 * OMNI_SDL12_INCLUDE_DIR), deliberately separate from the rest of this
 * codebase, which is compiled against SDL2 headers. This is the only
 * translation unit allowed to know SDL 1.2's actual SDL_Event layout. */
#include "sdl12_bridge.h"

#include <SDL/SDL.h>

#include <string.h>

void omni_sdl12_bridge_decode(const void *raw_event, OmniSdl12Decoded *out)
{
    const SDL_Event *event = (const SDL_Event *)raw_event;
    memset(out, 0, sizeof(*out));
    switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            const char *name;
            out->kind = (event->type == SDL_KEYDOWN) ? OMNI_SDL12_EVENT_KEYDOWN : OMNI_SDL12_EVENT_KEYUP;
            name = SDL_GetKeyName(event->key.keysym.sym);
            if (name != NULL) {
                strncpy(out->key_name, name, sizeof(out->key_name) - 1);
            }
            break;
        }
        case SDL_QUIT:
            out->kind = OMNI_SDL12_EVENT_QUIT;
            break;
        case SDL_MOUSEMOTION:
            out->kind = OMNI_SDL12_EVENT_MOUSEMOTION;
            break;
        case SDL_MOUSEBUTTONDOWN:
            out->kind = OMNI_SDL12_EVENT_MOUSEBUTTONDOWN;
            break;
        case SDL_MOUSEBUTTONUP:
            out->kind = OMNI_SDL12_EVENT_MOUSEBUTTONUP;
            break;
        default:
            out->kind = OMNI_SDL12_EVENT_OTHER;
            break;
    }
}
