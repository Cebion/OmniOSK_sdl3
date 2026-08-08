#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "symbols.h"

#include "diagnostics.h"

#include <dlfcn.h>
#include <string.h>

static _Thread_local int resolving;

static void *resolve(const char *name)
{
    void *address;
    if (resolving) {
        return NULL;
    }
    resolving = 1;
    address = dlsym(RTLD_NEXT, name);
    resolving = 0;
    return address;
}

#define OMNI_RESOLVE_FIELD(symbols, field, name, required) \
    do { \
        void *omni_address = resolve(name); \
        memcpy(&(symbols)->field, &omni_address, sizeof(omni_address)); \
        if ((symbols)->field == NULL && (required)) { \
            omni_diag_once(1, OMNI_LOG_ERROR, "required SDL symbol %s is unavailable", name); \
            missing = 1; \
        } \
    } while (0)

int omni_symbols_resolve(OmniSymbols *symbols)
{
    int missing = 0;
    memset(symbols, 0, sizeof(*symbols));
    OMNI_RESOLVE_FIELD(symbols, init, "SDL_Init", 0);
    OMNI_RESOLVE_FIELD(symbols, init_subsystem, "SDL_InitSubSystem", 0);
    OMNI_RESOLVE_FIELD(symbols, quit, "SDL_Quit", 1);
    OMNI_RESOLVE_FIELD(symbols, create_window, "SDL_CreateWindow", 0);
    OMNI_RESOLVE_FIELD(symbols, destroy_window, "SDL_DestroyWindow", 0);
    OMNI_RESOLVE_FIELD(symbols, gl_create_context, "SDL_GL_CreateContext", 0);
    OMNI_RESOLVE_FIELD(symbols, gl_make_current, "SDL_GL_MakeCurrent", 0);
    OMNI_RESOLVE_FIELD(symbols, gl_delete_context, "SDL_GL_DeleteContext", 0);
    OMNI_RESOLVE_FIELD(symbols, gl_swap_window, "SDL_GL_SwapWindow", 0);
    OMNI_RESOLVE_FIELD(symbols, create_renderer, "SDL_CreateRenderer", 0);
    OMNI_RESOLVE_FIELD(symbols, create_window_and_renderer, "SDL_CreateWindowAndRenderer", 0);
    OMNI_RESOLVE_FIELD(symbols, destroy_renderer, "SDL_DestroyRenderer", 0);
    OMNI_RESOLVE_FIELD(symbols, render_present, "SDL_RenderPresent", 0);
    OMNI_RESOLVE_FIELD(symbols, poll_event, "SDL_PollEvent", 1);
    OMNI_RESOLVE_FIELD(symbols, wait_event, "SDL_WaitEvent", 0);
    OMNI_RESOLVE_FIELD(symbols, wait_event_timeout, "SDL_WaitEventTimeout", 0);
    OMNI_RESOLVE_FIELD(symbols, peep_events, "SDL_PeepEvents", 1);
    OMNI_RESOLVE_FIELD(symbols, push_event, "SDL_PushEvent", 0);
    OMNI_RESOLVE_FIELD(symbols, get_keyboard_state, "SDL_GetKeyboardState", 0);
    OMNI_RESOLVE_FIELD(symbols, get_mod_state, "SDL_GetModState", 0);
    OMNI_RESOLVE_FIELD(symbols, is_text_input_active, "SDL_IsTextInputActive", 0);
    OMNI_RESOLVE_FIELD(symbols, has_event, "SDL_HasEvent", 0);
    OMNI_RESOLVE_FIELD(symbols, has_events, "SDL_HasEvents", 0);
    OMNI_RESOLVE_FIELD(symbols, flush_event, "SDL_FlushEvent", 0);
    OMNI_RESOLVE_FIELD(symbols, flush_events, "SDL_FlushEvents", 0);
    OMNI_RESOLVE_FIELD(symbols, event_state, "SDL_EventState", 0);
    OMNI_RESOLVE_FIELD(symbols, get_error, "SDL_GetError", 0);
    return missing ? -1 : 0;
}

void *omni_symbol_address(const OmniSymbols *symbols, const char *name)
{
    union {
        void *object;
        int (*function)(SDL_Event *);
        int (*peep)(SDL_Event *, int, SDL_eventaction, Uint32, Uint32);
        void (*swap)(SDL_Window *);
        void (*present)(SDL_Renderer *);
    } address;
    memset(&address, 0, sizeof(address));
    if (strcmp(name, "SDL_PollEvent") == 0) {
        address.function = symbols->poll_event;
        return address.object;
    }
    if (strcmp(name, "SDL_PeepEvents") == 0) {
        address.peep = symbols->peep_events;
        return address.object;
    }
    if (strcmp(name, "SDL_GL_SwapWindow") == 0) {
        address.swap = symbols->gl_swap_window;
        return address.object;
    }
    if (strcmp(name, "SDL_RenderPresent") == 0) {
        address.present = symbols->render_present;
        return address.object;
    }
    return NULL;
}
