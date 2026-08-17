#ifndef OMNI_SYMBOLS_H
#define OMNI_SYMBOLS_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OmniSymbols {
    bool (*init)(SDL_InitFlags flags);
    bool (*init_subsystem)(SDL_InitFlags flags);
    void (*quit)(void);
    SDL_Window *(*create_window)(const char *, int, int, SDL_WindowFlags);
    void (*destroy_window)(SDL_Window *);
    SDL_GLContext (*gl_create_context)(SDL_Window *);
    bool (*gl_make_current)(SDL_Window *, SDL_GLContext);
    bool (*gl_destroy_context)(SDL_GLContext);
    bool (*gl_swap_window)(SDL_Window *);
    SDL_Renderer *(*create_renderer)(SDL_Window *, const char *);
    bool (*create_window_and_renderer)(const char *, int, int, SDL_WindowFlags, SDL_Window **, SDL_Renderer **);
    void (*destroy_renderer)(SDL_Renderer *);
    bool (*render_present)(SDL_Renderer *);
    bool (*poll_event)(SDL_Event *);
    bool (*wait_event)(SDL_Event *);
    bool (*wait_event_timeout)(SDL_Event *, Sint32);
    int (*peep_events)(SDL_Event *, int, SDL_EventAction, Uint32, Uint32);
    bool (*push_event)(SDL_Event *);
    const bool *(*get_keyboard_state)(int *);
    SDL_Keymod (*get_mod_state)(void);
    bool (*is_text_input_active)(SDL_Window *);
    bool (*has_event)(Uint32);
    bool (*has_events)(Uint32, Uint32);
    void (*flush_event)(Uint32);
    void (*flush_events)(Uint32, Uint32);
    void (*set_event_enabled)(Uint32, bool);
    bool (*event_enabled)(Uint32);
    const char *(*get_error)(void);
} OmniSymbols;

int omni_symbols_resolve(OmniSymbols *symbols);
void *omni_symbol_address(const OmniSymbols *symbols, const char *name);

#ifdef __cplusplus
}
#endif

#endif
