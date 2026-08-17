#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "symbols.h"

#include "diagnostics.h"

#include <dlfcn.h>
#include <string.h>

/*
  Resolves against the real libSDL3.so.0, not RTLD_NEXT / the global scope.

  This library is preloaded into a process that already links a real
  libSDL3.so.0 (potentially itself a shim that forwards to the target's
  system SDL2 - see SDL3SHIM_dlsym's own doc comment in that shim's source).
  SDL3 kept many SDL2 function *names* while changing the ABI, so ordinary
  dlsym(RTLD_NEXT, "SDL_PollEvent") from inside a preloaded library can only
  ever be safe if every other SDL in the process shares SDL3's ABI - true
  here specifically because we resolve against libSDL3.so.0's own handle by
  SONAME, not by global-scope search order, via the exported SDL3SHIM_dlsym
  escape hatch when present. Falls back to dlsym(RTLD_NEXT, ...) for a
  genuine SDL3 build that doesn't ship that escape hatch.
*/
static _Thread_local int resolving;

static void *(*shim_dlsym)(const char *name);
static int shim_dlsym_checked;

static void *resolve(const char *name)
{
    void *address;
    if (resolving) {
        return NULL;
    }
    resolving = 1;
    if (!shim_dlsym_checked) {
        shim_dlsym = (void *(*)(const char *))dlsym(RTLD_DEFAULT, "SDL3SHIM_dlsym");
        shim_dlsym_checked = 1;
    }
    address = shim_dlsym ? shim_dlsym(name) : dlsym(RTLD_NEXT, name);
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
    OMNI_RESOLVE_FIELD(symbols, gl_destroy_context, "SDL_GL_DestroyContext", 0);
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
    OMNI_RESOLVE_FIELD(symbols, is_text_input_active, "SDL_TextInputActive", 0);
    OMNI_RESOLVE_FIELD(symbols, has_event, "SDL_HasEvent", 0);
    OMNI_RESOLVE_FIELD(symbols, has_events, "SDL_HasEvents", 0);
    OMNI_RESOLVE_FIELD(symbols, flush_event, "SDL_FlushEvent", 0);
    OMNI_RESOLVE_FIELD(symbols, flush_events, "SDL_FlushEvents", 0);
    OMNI_RESOLVE_FIELD(symbols, set_event_enabled, "SDL_SetEventEnabled", 0);
    OMNI_RESOLVE_FIELD(symbols, event_enabled, "SDL_EventEnabled", 0);
    OMNI_RESOLVE_FIELD(symbols, get_error, "SDL_GetError", 0);
    return missing ? -1 : 0;
}

void *omni_symbol_address(const OmniSymbols *symbols, const char *name)
{
    union {
        void *object;
        bool (*function)(SDL_Event *);
        int (*peep)(SDL_Event *, int, SDL_EventAction, Uint32, Uint32);
        bool (*swap)(SDL_Window *);
        bool (*present)(SDL_Renderer *);
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
