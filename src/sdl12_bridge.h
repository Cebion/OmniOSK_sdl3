#ifndef OMNI_SDL12_BRIDGE_H
#define OMNI_SDL12_BRIDGE_H

/* No SDL headers here on purpose - this must be includable from code built
 * against SDL2 headers (preload.c) even though sdl12_bridge.c itself is
 * compiled against real SDL 1.2 headers. See sdl12_bridge.c for why: SDL
 * 1.2's SDL_Event has a completely different memory layout than SDL2's
 * (Uint8 type vs Uint32 type, among other differences), so code built
 * against SDL2 headers cannot safely read a buffer a real SDL 1.2
 * implementation (sdl12-compat) wrote - not just different keysym numbers,
 * different byte offsets for every field.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OMNI_SDL12_EVENT_OTHER = 0,
    OMNI_SDL12_EVENT_KEYDOWN,
    OMNI_SDL12_EVENT_KEYUP,
    OMNI_SDL12_EVENT_QUIT,
    OMNI_SDL12_EVENT_MOUSEMOTION,
    OMNI_SDL12_EVENT_MOUSEBUTTONDOWN,
    OMNI_SDL12_EVENT_MOUSEBUTTONUP
} OmniSdl12EventKind;

typedef struct {
    OmniSdl12EventKind kind;
    /* SDL 1.2's SDL_GetKeyName() result for the pressed/released key, valid
     * for KEYDOWN/KEYUP only, empty string if SDL 1.2 didn't recognize it.
     * SDL 1.2 and SDL2 use the same name strings for the same keys even
     * though their numeric keysym values differ, so the caller can feed
     * this straight into real SDL2's own SDL_GetKeyFromName() to get a
     * genuinely correct SDL2 keysym - no hardcoded per-version value table
     * needed on either side. */
    char key_name[32];
} OmniSdl12Decoded;

/* raw_event must point at a buffer of at least sizeof(SDL_Event) bytes as
 * written by a real SDL 1.2 (or sdl12-compat) SDL_PollEvent/SDL_WaitEvent
 * call - i.e. exactly the buffer a real SDL 1.2 caller passed in and that
 * the underlying implementation already wrote into. Leaves *out at
 * OMNI_SDL12_EVENT_OTHER for any event type OmniOSK doesn't need to
 * inspect.
 */
void omni_sdl12_bridge_decode(const void *raw_event, OmniSdl12Decoded *out);

#ifdef __cplusplus
}
#endif

#endif
