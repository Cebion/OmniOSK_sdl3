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
static bool keyboard_shadow[SDL_SCANCODE_COUNT];

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
    int text_enabled = symbols.is_text_input_active == NULL || symbols.is_text_input_active(runtime.window);
    return omni_runtime_process_event(&runtime, event, text_enabled);
}

bool SDL_Init(SDL_InitFlags flags)
{
    bool result;
    ensure_initialized();
    if (symbols.init == NULL) {
        return false;
    }
    result = symbols.init(flags);
    if (result && (flags & (SDL_INIT_VIDEO | SDL_INIT_EVENTS)) != 0) {
        (void)omni_runtime_ensure(&runtime);
    }
    return result;
}

bool SDL_InitSubSystem(SDL_InitFlags flags)
{
    bool result;
    ensure_initialized();
    if (symbols.init_subsystem == NULL) {
        return false;
    }
    result = symbols.init_subsystem(flags);
    if (result && (flags & (SDL_INIT_VIDEO | SDL_INIT_EVENTS)) != 0) {
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

SDL_Window *SDL_CreateWindow(const char *title, int w, int h, SDL_WindowFlags flags)
{
    SDL_Window *window;
    ensure_initialized();
    if (symbols.create_window == NULL) {
        return NULL;
    }
    window = symbols.create_window(title, w, h, flags);
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

bool SDL_GL_MakeCurrent(SDL_Window *window, SDL_GLContext context)
{
    bool result;
    ensure_initialized();
    if (symbols.gl_make_current == NULL) {
        return false;
    }
    result = symbols.gl_make_current(window, context);
    if (result) {
        if (runtime.context == context) {
            omni_runtime_set_window(&runtime, window);
        } else if (runtime.context == NULL && runtime.renderer == NULL) {
            omni_runtime_set_context(&runtime, context);
            omni_runtime_set_window(&runtime, window);
        }
    }
    return result;
}

bool SDL_GL_DestroyContext(SDL_GLContext context)
{
    bool result = true;
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
    if (symbols.gl_destroy_context != NULL) {
        result = symbols.gl_destroy_context(context);
    }
    return result;
}

SDL_Renderer *SDL_CreateRenderer(SDL_Window *window, const char *name)
{
    SDL_Renderer *renderer;
    ensure_initialized();
    if (symbols.create_renderer == NULL) {
        return NULL;
    }
    renderer = symbols.create_renderer(window, name);
    if (renderer != NULL && runtime.renderer == NULL && runtime.context == NULL) {
        omni_runtime_set_renderer(&runtime, renderer);
        omni_runtime_set_window(&runtime, window);
    }
    return renderer;
}

bool SDL_CreateWindowAndRenderer(const char *title, int width, int height, SDL_WindowFlags window_flags,
                                  SDL_Window **window, SDL_Renderer **renderer)
{
    bool result;
    ensure_initialized();
    if (symbols.create_window_and_renderer == NULL) {
        return false;
    }
    result = symbols.create_window_and_renderer(title, width, height, window_flags, window, renderer);
    if (result && runtime.renderer == NULL && runtime.context == NULL) {
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

static bool poll_native(SDL_Event *event)
{
    if (symbols.poll_event == NULL) {
        return false;
    }
    return symbols.poll_event(event);
}

bool SDL_PollEvent(SDL_Event *event)
{
    SDL_Event scratch;
    SDL_Event *target = event == NULL ? &scratch : event;
    ensure_initialized();
    omni_runtime_tick(&runtime);
    for (;;) {
        if (poll_native(target)) {
            if (!process_event(target)) {
                return true;
            }
            continue;
        }
        if (omni_runtime_pop_generated(&runtime, target)) {
            return true;
        }
        return false;
    }
}

bool SDL_WaitEvent(SDL_Event *event)
{
    SDL_Event scratch;
    SDL_Event *target = event == NULL ? &scratch : event;
    ensure_initialized();
    omni_runtime_tick(&runtime);
    if (omni_runtime_pop_generated(&runtime, target)) {
        return true;
    }
    if (symbols.wait_event == NULL) {
        return false;
    }
    while (symbols.wait_event(target)) {
        if (!process_event(target)) {
            return true;
        }
        if (omni_runtime_pop_generated(&runtime, target)) {
            return true;
        }
    }
    return false;
}

bool SDL_WaitEventTimeout(SDL_Event *event, Sint32 timeoutMS)
{
    SDL_Event scratch;
    SDL_Event *target = event == NULL ? &scratch : event;
    ensure_initialized();
    omni_runtime_tick(&runtime);
    if (omni_runtime_pop_generated(&runtime, target)) {
        return true;
    }
    if (symbols.wait_event_timeout == NULL) {
        return false;
    }
    while (symbols.wait_event_timeout(target, timeoutMS)) {
        if (!process_event(target)) {
            return true;
        }
        if (omni_runtime_pop_generated(&runtime, target)) {
            return true;
        }
        timeoutMS = 0;
    }
    return false;
}

int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_EventAction action,
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
                         (current.type == SDL_EVENT_KEY_DOWN || current.type == SDL_EVENT_KEY_UP);
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

bool SDL_PushEvent(SDL_Event *event)
{
    ensure_initialized();
    return symbols.push_event == NULL ? false : symbols.push_event(event);
}

const bool *SDL_GetKeyboardState(int *numkeys)
{
    const bool *native_state;
    int native_count = 0;
    ensure_initialized();
    if (!omni_runtime_active(&runtime) || symbols.get_keyboard_state == NULL) {
        return symbols.get_keyboard_state == NULL ? keyboard_shadow : symbols.get_keyboard_state(numkeys);
    }
    native_state = symbols.get_keyboard_state(&native_count);
    if (native_count > SDL_SCANCODE_COUNT && native_state != NULL) {
        native_count = SDL_SCANCODE_COUNT;
    }
    memset(keyboard_shadow, 0, sizeof(keyboard_shadow));
    if (numkeys != NULL) {
        *numkeys = SDL_SCANCODE_COUNT;
    }
    (void)native_state;
    return keyboard_shadow;
}

SDL_Keymod SDL_GetModState(void)
{
    ensure_initialized();
    if (omni_runtime_active(&runtime)) {
        return SDL_KMOD_NONE;
    }
    return symbols.get_mod_state == NULL ? SDL_KMOD_NONE : symbols.get_mod_state();
}

bool SDL_HasEvent(Uint32 type)
{
    SDL_Event event;
    ensure_initialized();
    if (omni_runtime_peek_generated(&runtime, &event) && event.type == type) {
        return true;
    }
    if (omni_runtime_active(&runtime) && (type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP)) {
        return false;
    }
    if (symbols.has_event != NULL && symbols.has_event(type)) {
        return true;
    }
    return false;
}

bool SDL_HasEvents(Uint32 minType, Uint32 maxType)
{
    SDL_Event event;
    ensure_initialized();
    if (omni_generated_peek_matching(&runtime.generated, minType, maxType, &event)) {
        return true;
    }
    if (omni_runtime_active(&runtime) && minType <= SDL_EVENT_KEY_UP && maxType >= SDL_EVENT_KEY_DOWN) {
        return false;
    }
    return symbols.has_events != NULL && symbols.has_events(minType, maxType);
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

void SDL_SetEventEnabled(Uint32 type, bool enabled)
{
    ensure_initialized();
    if (symbols.set_event_enabled != NULL) {
        symbols.set_event_enabled(type, enabled);
    }
}

bool SDL_EventEnabled(Uint32 type)
{
    ensure_initialized();
    return symbols.event_enabled == NULL ? true : symbols.event_enabled(type);
}

bool SDL_GL_SwapWindow(SDL_Window *window)
{
    bool result = true;
    ensure_initialized();
    if (omni_runtime_active(&runtime)) {
        (void)omni_render_gl(window, &runtime);
    }
    if (symbols.gl_swap_window != NULL) {
        result = symbols.gl_swap_window(window);
    }
    return result;
}

bool SDL_RenderPresent(SDL_Renderer *renderer)
{
    bool result = true;
    ensure_initialized();
    if (omni_runtime_active(&runtime)) {
        (void)omni_render_sdlrenderer(renderer, &runtime);
    }
    if (symbols.render_present != NULL) {
        result = symbols.render_present(renderer);
    }
    return result;
}
