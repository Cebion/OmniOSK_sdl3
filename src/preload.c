#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "runtime.h"

#include "diagnostics.h"
#include "imgui_bridge.h"
#include "render_gl.h"
#include "render_sdlrenderer.h"

#include <pthread.h>
#include <string.h>

static OmniRuntime runtime;
static OmniSymbols symbols;
static pthread_once_t symbols_once = PTHREAD_ONCE_INIT;
static _Thread_local int initializing;
static Uint8 keyboard_shadow[SDL_NUM_SCANCODES];

static void initialize(void)
{
    initializing = 1;
    if (omni_symbols_resolve(&symbols) != 0) {
        runtime.state = OMNI_RUNTIME_DISABLED;
        runtime.initialized = 1;
        omni_diag(OMNI_LOG_ERROR, "SDL symbol resolution failed; all hooks are pass-through");
        initializing = 0;
        return;
    }
    (void)omni_runtime_ensure(&runtime);
    initializing = 0;
}

static void ensure_initialized(void)
{
    if (initializing) {
        return;
    }
    (void)pthread_once(&symbols_once, initialize);
}

static int process_event(SDL_Event *event)
{
    int text_enabled = symbols.is_text_input_active == NULL || symbols.is_text_input_active() != SDL_FALSE;
    return omni_runtime_process_event(&runtime, event, text_enabled);
}

int SDL_Init(Uint32 flags)
{
    int result;
    ensure_initialized();
    if (symbols.init == NULL) {
        return -1;
    }
    result = symbols.init(flags);
    if (result == 0 && (flags & (SDL_INIT_VIDEO | SDL_INIT_EVENTS)) != 0) {
        (void)omni_runtime_ensure(&runtime);
    }
    return result;
}

int SDL_InitSubSystem(Uint32 flags)
{
    int result;
    ensure_initialized();
    if (symbols.init_subsystem == NULL) {
        return -1;
    }
    result = symbols.init_subsystem(flags);
    if (result == 0 && (flags & (SDL_INIT_VIDEO | SDL_INIT_EVENTS)) != 0) {
        (void)omni_runtime_ensure(&runtime);
    }
    return result;
}

void SDL_Quit(void)
{
    ensure_initialized();
    if (runtime.context == NULL || SDL_GL_GetCurrentContext() == runtime.context) {
        omni_render_gl_shutdown();
    } else {
        omni_imgui_gl_invalidate();
    }
    omni_render_sdlrenderer_shutdown();
    omni_runtime_shutdown(&runtime);
    runtime.window = NULL;
    runtime.context = NULL;
    runtime.renderer = NULL;
    if (symbols.quit != NULL) {
        symbols.quit();
    }
}

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags)
{
    SDL_Window *window;
    ensure_initialized();
    if (symbols.create_window == NULL) {
        return NULL;
    }
    window = symbols.create_window(title, x, y, w, h, flags);
    return window;
}

void SDL_DestroyWindow(SDL_Window *window)
{
    ensure_initialized();
    if (window == runtime.window) {
        if (runtime.context == NULL || SDL_GL_GetCurrentContext() == runtime.context) {
            omni_render_gl_shutdown();
        } else {
            omni_imgui_gl_invalidate();
        }
        omni_render_sdlrenderer_shutdown();
        omni_runtime_presentation_lost(&runtime);
        runtime.window = NULL;
        runtime.context = NULL;
        runtime.renderer = NULL;
    }
    if (symbols.destroy_window != NULL) {
        symbols.destroy_window(window);
    }
}

SDL_GLContext SDL_GL_CreateContext(SDL_Window *window)
{
    SDL_GLContext context;
    ensure_initialized();
    if (symbols.gl_create_context == NULL) {
        return NULL;
    }
    context = symbols.gl_create_context(window);
    if (context != NULL && runtime.context == NULL && runtime.renderer == NULL) {
        omni_runtime_set_context(&runtime, context);
        omni_runtime_set_window(&runtime, window);
    }
    return context;
}

int SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context)
{
    int result;
    ensure_initialized();
    if (symbols.gl_make_current == NULL) {
        return -1;
    }
    result = symbols.gl_make_current(window, context);
    if (result == 0) {
        if (runtime.context == context) {
            omni_runtime_set_window(&runtime, window);
        } else if (runtime.context == NULL && runtime.renderer == NULL) {
            omni_runtime_set_context(&runtime, context);
            omni_runtime_set_window(&runtime, window);
        }
    }
    return result;
}

void SDL_GL_DeleteContext(SDL_GLContext context)
{
    ensure_initialized();
    if (runtime.context == context) {
        if (SDL_GL_GetCurrentContext() == context) {
            omni_render_gl_shutdown();
        } else {
            omni_imgui_gl_invalidate();
        }
        omni_runtime_presentation_lost(&runtime);
        runtime.context = NULL;
        if (runtime.renderer == NULL) {
            runtime.window = NULL;
        }
    }
    if (symbols.gl_delete_context != NULL) {
        symbols.gl_delete_context(context);
    }
}

SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, int index, Uint32 flags)
{
    SDL_Renderer *renderer;
    ensure_initialized();
    if (symbols.create_renderer == NULL) {
        return NULL;
    }
    renderer = symbols.create_renderer(window, index, flags);
    if (renderer != NULL && runtime.renderer == NULL && runtime.context == NULL) {
        omni_runtime_set_renderer(&runtime, renderer);
        omni_runtime_set_window(&runtime, window);
    }
    return renderer;
}

int SDL_CreateWindowAndRenderer(int width, int height, Uint32 flags,
                                SDL_Window **window, SDL_Renderer **renderer)
{
    int result;
    ensure_initialized();
    if (symbols.create_window_and_renderer == NULL) {
        return -1;
    }
    result = symbols.create_window_and_renderer(width, height, flags, window, renderer);
    if (result == 0 && runtime.renderer == NULL && runtime.context == NULL) {
        omni_runtime_set_window(&runtime, *window);
        omni_runtime_set_renderer(&runtime, *renderer);
    }
    return result;
}

void SDL_DestroyRenderer(SDL_Renderer *renderer)
{
    ensure_initialized();
    if (runtime.renderer == renderer) {
        omni_render_sdlrenderer_shutdown();
        omni_runtime_presentation_lost(&runtime);
        runtime.renderer = NULL;
        if (runtime.context == NULL) {
            runtime.window = NULL;
        }
    }
    if (symbols.destroy_renderer != NULL) {
        symbols.destroy_renderer(renderer);
    }
}

static int poll_native(SDL_Event *event)
{
    if (symbols.poll_event == NULL) {
        return 0;
    }
    return symbols.poll_event(event);
}

int SDL_PollEvent(SDL_Event *event)
{
    SDL_Event scratch;
    SDL_Event *target = event == NULL ? &scratch : event;
    ensure_initialized();
    omni_runtime_tick(&runtime);
    for (;;) {
        if (poll_native(target) != 0) {
            if (!process_event(target)) {
                return 1;
            }
            continue;
        }
        if (omni_runtime_pop_generated(&runtime, target)) {
            return 1;
        }
        return 0;
    }
}

int SDL_WaitEvent(SDL_Event *event)
{
    SDL_Event scratch;
    SDL_Event *target = event == NULL ? &scratch : event;
    ensure_initialized();
    omni_runtime_tick(&runtime);
    if (omni_runtime_pop_generated(&runtime, target)) {
        return 1;
    }
    if (symbols.wait_event == NULL) {
        return 0;
    }
    while (symbols.wait_event(target) != 0) {
        if (!process_event(target)) {
            return 1;
        }
        if (omni_runtime_pop_generated(&runtime, target)) {
            return 1;
        }
    }
    return 0;
}

int SDL_WaitEventTimeout(SDL_Event *event, int timeout)
{
    SDL_Event scratch;
    SDL_Event *target = event == NULL ? &scratch : event;
    ensure_initialized();
    omni_runtime_tick(&runtime);
    if (omni_runtime_pop_generated(&runtime, target)) {
        return 1;
    }
    if (symbols.wait_event_timeout == NULL) {
        return 0;
    }
    while (symbols.wait_event_timeout(target, timeout) != 0) {
        if (!process_event(target)) {
            return 1;
        }
        if (omni_runtime_pop_generated(&runtime, target)) {
            return 1;
        }
        timeout = 0;
    }
    return 0;
}

int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_eventaction action,
                   Uint32 minType, Uint32 maxType)
{
    int count;
    int input_index;
    int output_index = 0;
    ensure_initialized();
    if (symbols.peep_events == NULL || events == NULL || numevents <= 0) {
        return symbols.peep_events == NULL ? -1 : symbols.peep_events(events, numevents, action, minType, maxType);
    }
    count = symbols.peep_events(events, numevents, action, minType, maxType);
    if (count <= 0 || action == SDL_ADDEVENT) {
        return count;
    }
    if (action == SDL_PEEKEVENT) {
        int source_index;
        output_index = 0;
        for (source_index = 0; source_index < count; ++source_index) {
            SDL_Event current = events[source_index];
            int hidden = omni_runtime_active(&runtime) &&
                         (current.type == SDL_KEYDOWN || current.type == SDL_KEYUP);
            if (!hidden) {
                events[output_index++] = current;
            }
        }
        {
            SDL_Event generated;
            while (output_index < numevents && omni_generated_peek_matching(&runtime.generated, minType, maxType, &generated)) {
                events[output_index++] = generated;
                break;
            }
        }
        return output_index;
    }
    for (input_index = 0; input_index < count; ++input_index) {
        SDL_Event current = events[input_index];
        if (!process_event(&current)) {
            events[output_index++] = current;
        }
    }
    while (output_index < numevents && omni_generated_pop_matching(&runtime.generated, minType, maxType, &events[output_index])) {
        ++output_index;
    }
    return output_index;
}

int SDL_PushEvent(SDL_Event *event)
{
    ensure_initialized();
    return symbols.push_event == NULL ? -1 : symbols.push_event(event);
}

const Uint8 *SDL_GetKeyboardState(int *numkeys)
{
    const Uint8 *native_state;
    int native_count = 0;
    ensure_initialized();
    if (!omni_runtime_active(&runtime) || symbols.get_keyboard_state == NULL) {
        return symbols.get_keyboard_state == NULL ? keyboard_shadow : symbols.get_keyboard_state(numkeys);
    }
    native_state = symbols.get_keyboard_state(&native_count);
    if (native_count > SDL_NUM_SCANCODES && native_state != NULL) {
        native_count = SDL_NUM_SCANCODES;
    }
    memset(keyboard_shadow, 0, sizeof(keyboard_shadow));
    if (numkeys != NULL) {
        *numkeys = SDL_NUM_SCANCODES;
    }
    (void)native_state;
    return keyboard_shadow;
}

SDL_Keymod SDL_GetModState(void)
{
    ensure_initialized();
    if (omni_runtime_active(&runtime)) {
        return KMOD_NONE;
    }
    return symbols.get_mod_state == NULL ? KMOD_NONE : symbols.get_mod_state();
}

SDL_bool SDL_HasEvent(Uint32 type)
{
    SDL_Event event;
    ensure_initialized();
    if (omni_runtime_peek_generated(&runtime, &event) && event.type == type) {
        return 1;
    }
    if (omni_runtime_active(&runtime) && (type == SDL_KEYDOWN || type == SDL_KEYUP)) {
        return SDL_FALSE;
    }
    if (symbols.has_event != NULL && symbols.has_event(type) != 0) {
        return 1;
    }
    return 0;
}

SDL_bool SDL_HasEvents(Uint32 minType, Uint32 maxType)
{
    SDL_Event event;
    ensure_initialized();
    if (omni_generated_peek_matching(&runtime.generated, minType, maxType, &event)) {
        return 1;
    }
    if (omni_runtime_active(&runtime) && minType <= SDL_KEYUP && maxType >= SDL_KEYDOWN) {
        return SDL_FALSE;
    }
    return symbols.has_events != NULL && symbols.has_events(minType, maxType) != 0;
}

void SDL_FlushEvent(Uint32 type)
{
    ensure_initialized();
    if (symbols.flush_event != NULL) {
        symbols.flush_event(type);
    }
    if (runtime.generated.count != 0) {
        SDL_Event event;
        OmniGeneratedQueue retained;
        omni_generated_init(&retained);
        while (omni_generated_pop(&runtime.generated, &event)) {
            if (event.type != type) {
                (void)omni_generated_push(&retained, &event);
            }
        }
        runtime.generated = retained;
    }
}

void SDL_FlushEvents(Uint32 minType, Uint32 maxType)
{
    ensure_initialized();
    if (symbols.flush_events != NULL) {
        symbols.flush_events(minType, maxType);
    }
    if (runtime.generated.count != 0) {
        SDL_Event event;
        OmniGeneratedQueue retained;
        omni_generated_init(&retained);
        while (omni_generated_pop(&runtime.generated, &event)) {
            if (event.type < minType || event.type > maxType) {
                (void)omni_generated_push(&retained, &event);
            }
        }
        runtime.generated = retained;
    }
}

Uint8 SDL_EventState(Uint32 type, int state)
{
    ensure_initialized();
    return symbols.event_state == NULL ? SDL_ENABLE : symbols.event_state(type, state);
}

void SDL_GL_SwapWindow(SDL_Window *window)
{
    ensure_initialized();
    if (omni_runtime_active(&runtime)) {
        (void)omni_render_gl(window, &runtime);
    }
    if (symbols.gl_swap_window != NULL) {
        symbols.gl_swap_window(window);
    }
}

void SDL_RenderPresent(SDL_Renderer *renderer)
{
    ensure_initialized();
    if (omni_runtime_active(&runtime)) {
        (void)omni_render_sdlrenderer(renderer, &runtime);
    }
    if (symbols.render_present != NULL) {
        symbols.render_present(renderer);
    }
}
