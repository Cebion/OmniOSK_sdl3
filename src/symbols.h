#ifndef OMNI_SYMBOLS_H
#define OMNI_SYMBOLS_H

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OmniSymbols {
    int (*init)(Uint32 flags);
    int (*init_subsystem)(Uint32 flags);
    void (*quit)(void);
    SDL_Window *(*create_window)(const char *, int, int, int, int, Uint32);
    void (*destroy_window)(SDL_Window *);
    SDL_GLContext (*gl_create_context)(SDL_Window *);
    int (*gl_make_current)(SDL_Window *, SDL_GLContext);
    void (*gl_delete_context)(SDL_GLContext);
    void (*gl_swap_window)(SDL_Window *);
    SDL_Renderer *(*create_renderer)(SDL_Window *, int, Uint32);
    int (*create_window_and_renderer)(int, int, Uint32, SDL_Window **, SDL_Renderer **);
    void (*destroy_renderer)(SDL_Renderer *);
    void (*render_present)(SDL_Renderer *);
    int (*poll_event)(SDL_Event *);
    int (*wait_event)(SDL_Event *);
    int (*wait_event_timeout)(SDL_Event *, int);
    int (*peep_events)(SDL_Event *, int, SDL_eventaction, Uint32, Uint32);
    int (*push_event)(SDL_Event *);
    const Uint8 *(*get_keyboard_state)(int *);
    SDL_Keymod (*get_mod_state)(void);
    SDL_bool (*is_text_input_active)(void);
    SDL_bool (*has_event)(Uint32);
    SDL_bool (*has_events)(Uint32, Uint32);
    void (*flush_event)(Uint32);
    void (*flush_events)(Uint32, Uint32);
    Uint8 (*event_state)(Uint32, int);
    const char *(*get_error)(void);
} OmniSymbols;

int omni_symbols_resolve(OmniSymbols *symbols);
void *omni_symbol_address(const OmniSymbols *symbols, const char *name);

#ifdef __cplusplus
}
#endif

#endif
