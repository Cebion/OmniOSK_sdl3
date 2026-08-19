#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "runtime.h"

#include "diagnostics.h"
#include "imgui_bridge.h"
#include "render_gl.h"
#include "render_sdlrenderer.h"
#ifdef OMNI_HAVE_SDL12_BRIDGE
#include "sdl12_bridge.h"
#endif

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

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
#ifdef OMNI_HAVE_SDL12_BRIDGE
    {
        OmniSdl12Decoded decoded;
        SDL_Event synth;
        omni_sdl12_bridge_decode(event, &decoded);
        if (decoded.kind == OMNI_SDL12_EVENT_OTHER) {
            /* Not a keyboard/quit event OmniOSK needs to inspect - leave
             * the caller's buffer untouched and let it pass through. */
            return 0;
        }
        memset(&synth, 0, sizeof(synth));
        switch (decoded.kind) {
            case OMNI_SDL12_EVENT_QUIT:
                synth.type = SDL_QUIT;
                break;
            case OMNI_SDL12_EVENT_MOUSEMOTION:
                synth.type = SDL_MOUSEMOTION;
                break;
            case OMNI_SDL12_EVENT_MOUSEBUTTONDOWN:
                synth.type = SDL_MOUSEBUTTONDOWN;
                break;
            case OMNI_SDL12_EVENT_MOUSEBUTTONUP:
                synth.type = SDL_MOUSEBUTTONUP;
                break;
            default: {
                SDL_Keycode key = decoded.key_name[0] != '\0' ? SDL_GetKeyFromName(decoded.key_name) : SDLK_UNKNOWN;
                synth.type = (decoded.kind == OMNI_SDL12_EVENT_KEYDOWN) ? SDL_KEYDOWN : SDL_KEYUP;
                synth.key.type = synth.type;
                synth.key.state = (decoded.kind == OMNI_SDL12_EVENT_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
                synth.key.keysym.sym = key;
                synth.key.keysym.scancode = SDL_GetScancodeFromKey(key);
                break;
            }
        }
        return omni_runtime_process_event(&runtime, &synth, text_enabled);
    }
#else
    return omni_runtime_process_event(&runtime, event, text_enabled);
#endif
}

#ifdef OMNI_HAVE_SDL12_BRIDGE
/* OmniOSK's own generated (typed/submitted) key events don't reach
 * amuletsarmor's text field through the event queue at all: KEYBOARD.C's
 * KeyboardUpdate() reads input exclusively via SDL_GetKeyState() polling
 * and edge-detects transitions itself (confirmed by reading the source -
 * it never touches the event queue for gameplay or text entry), the same
 * reason the D-pad leak fix needed an SDL_GetKeyState hook rather than
 * event suppression alone. Delivering generated characters means
 * presenting them through that same polling interface instead: for one
 * SDL_GetKeyState() call, report the key(s) as pressed (triggering
 * KeyboardUpdate()'s 0->1 edge, which is what actually pushes a character
 * into its internal ASCII buffer); on the next call, report nothing (the
 * edge back to 0, and a clean gap before the next character) and advance
 * to the next queued combo. A "combo" is more than one simultaneous key
 * because omni_generated_ascii() wraps a shifted (uppercase) character as
 * [LSHIFT-DOWN, LETTER-DOWN, LETTER-UP, LSHIFT-UP] - both downs need to be
 * presented together for KeyboardUpdate()'s own shift-lookup to see
 * LSHIFT held at the same instant the letter's press-edge fires, or it'll
 * deliver lowercase instead. The matching UP pair is intentionally
 * discarded when grouping a combo - the state machine's own call-later
 * "gap" already synthesizes a clean release, no need to model SDL 1.2's
 * actual release timing.
 *
 * This is the *only* place runtime.generated gets drained for a bridge
 * build (see pop_generated_dispatch() below, which deliberately never
 * touches it): amuletsarmor's WindowsUpdateEvents() drains SDL_PollEvent
 * every frame *before* calling KeyboardUpdate() (confirmed in main.c), so
 * an event-queue-based drain would win that race and silently steal
 * characters before this combo mechanism ever saw them. Writing OmniOSK's
 * SDL2-shaped generated event (56 bytes) directly into a bridge caller's
 * real SDL 1.2-shaped buffer (24 bytes) is also a genuine stack-buffer
 * overflow - confirmed as a real "stack smashing detected" crash on real
 * hardware submitting a name in amuletsarmor before this was disabled. */
#define OMNI_COMBO_MAX_KEYS 4
#define OMNI_COMBO_QUEUE_CAPACITY 32

typedef struct {
    SDL_Keycode keys[OMNI_COMBO_MAX_KEYS];
    int count;
} OmniKeyCombo;

static OmniKeyCombo combo_queue[OMNI_COMBO_QUEUE_CAPACITY];
static size_t combo_head;
static size_t combo_count;
static int combo_presented;

static void refill_combo_queue(void)
{
    SDL_Event event;
    while (combo_count < OMNI_COMBO_QUEUE_CAPACITY && omni_runtime_pop_generated(&runtime, &event)) {
        SDL_Keycode down_keys[OMNI_COMBO_MAX_KEYS];
        int down_count = 0;
        size_t slot;
        int i;
        if (event.type != SDL_KEYDOWN) {
            /* A stray UP with no DOWN collected alongside it here - this
             * shouldn't happen given generated_events.c always pushes
             * N downs then N ups as one atomic push, but skip rather than
             * misinterpret if it ever does. */
            continue;
        }
        /* generated_events.c fills these with SDL2 keysym values (e.g.
         * SDLK_LSHIFT for the uppercase shift-wrap), but shadow[] below
         * emulates SDL 1.2's own SDL_GetKeyState() array, indexed by SDL
         * 1.2's numbering - identical to SDL2's for ASCII-range keys
         * (letters/digits/Return/Backspace), but not for SDLK_LSHIFT
         * (304 in real SDL 1.2, a large SDLK_SCANCODE_MASK-based value in
         * SDL2, out of shadow[]'s bounds - dropped silently by the bounds
         * check below without this, delivering lowercase instead of the
         * intended uppercase letter). */
        down_keys[down_count++] = omni_sdl12_equivalent(event.key.keysym.sym);
        while (down_count < OMNI_COMBO_MAX_KEYS &&
               omni_runtime_peek_generated(&runtime, &event) && event.type == SDL_KEYDOWN) {
            (void)omni_runtime_pop_generated(&runtime, &event);
            down_keys[down_count++] = omni_sdl12_equivalent(event.key.keysym.sym);
        }
        for (i = 0; i < down_count; ++i) {
            if (!omni_runtime_pop_generated(&runtime, &event)) {
                break;
            }
        }
        slot = (combo_head + combo_count) % OMNI_COMBO_QUEUE_CAPACITY;
        for (i = 0; i < down_count; ++i) {
            combo_queue[slot].keys[i] = down_keys[i];
        }
        combo_queue[slot].count = down_count;
        ++combo_count;
    }
}
#endif

static int pop_generated_dispatch(SDL_Event *target)
{
#ifdef OMNI_HAVE_SDL12_BRIDGE
    /* Never drain runtime.generated from here for a bridge target -
     * delivery happens exclusively through SDL_GetKeyState()'s
     * combo-injection mechanism (refill_combo_queue() above), which is
     * what amuletsarmor's own text entry actually reads. See that
     * function's own comment for why draining here too would race it and
     * also reintroduce a real stack-buffer-overflow crash. */
    (void)target;
    return 0;
#else
    return omni_runtime_pop_generated(&runtime, target);
#endif
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

#ifdef OMNI_HAVE_SDL12_BRIDGE
/* Normally SDL_RenderPresent()/SDL_GL_SwapWindow() are hooked below to draw
 * the overlay just before forwarding to the real present call. That never
 * fires for a target reached via sdl12-compat: it presents its own
 * internal renderer through the same invisible-to-interception path as
 * everything else discussed in sdl12_bridge.c/discover_presentation().
 *
 * The fix is to hook whichever real SDL 1.2 entry point the game actually
 * uses to push its frame to the screen - SDL_UpdateRect/SDL_UpdateRects/
 * SDL_Flip, all SDL 1.2-only names with no SDL2 equivalent, so they're
 * safe to intercept by name with zero collision risk. The hooks below skip
 * calling through to the real implementation entirely while the overlay
 * is active (see the comment right above them) rather than drawing after
 * it - drawing after still left two independent things (sdl12-compat's
 * own present of the game's content, and ours) racing to be what's shown
 * each real frame, which produced real, confirmed-on-hardware flicker no
 * amount of present-timing/present-count tuning fully eliminated (vsync
 * toggling, forcing a software renderer, presenting several times in a
 * row - see git history/docs/omniosk-integration.md for the full trail).
 * Skipping the real call outright removes the race instead of trying to
 * win it: only one thing ever presents each real frame while the overlay
 * is open.
 *
 * A first attempt instead drew from inside the SDL_PollEvent/SDL_WaitEvent
 * hooks on a fixed ~60fps timer, since those are guaranteed to run every
 * frame regardless of what the game itself calls. That raced sdl12-compat's
 * own present cycle (uncoordinated timing between the two) and produced
 * visible flicker on real hardware against a game that draws continuously
 * (confirmed on amuletsarmor, which calls SDL_UpdateRect() every frame).
 * Kept as `present_overlay_fallback()` below for a game that never calls
 * any of those three functions at all (e.g. checkkeys, which sets a video
 * mode but never actually draws) - only used when the real hook has never
 * fired, so it never competes with it. */
static int real_present_hook_seen;

static void draw_and_present_overlay(void)
{
    if (!omni_runtime_active(&runtime) || runtime.renderer == NULL) {
        return;
    }
    (void)omni_render_sdlrenderer(runtime.renderer, &runtime);
    if (symbols.render_present != NULL) {
        symbols.render_present(runtime.renderer);
    }
}

static void present_overlay_fallback(void)
{
    static Uint32 last_present_ticks;
    Uint32 now;
    if (real_present_hook_seen) {
        return;
    }
    now = SDL_GetTicks();
    if (now - last_present_ticks < 16) {
        return;
    }
    last_present_ticks = now;
    draw_and_present_overlay();
}

typedef void (*OmniSdl12UpdateRectFn)(void *, Sint32, Sint32, Uint32, Uint32);
typedef void (*OmniSdl12UpdateRectsFn)(void *, int, void *);
typedef int (*OmniSdl12FlipFn)(void *);

/* Every present-timing/present-count experiment tried above still had to
 * coexist with sdl12-compat's OWN present of the game's own content on
 * the same real frame - two independent things fighting to be what's on
 * screen next, unavoidably producing some kind of visible alternation
 * (a clean strobe with vsync on, a faster dithering-like flicker with it
 * off, no meaningful difference from presenting several times in a row -
 * all confirmed on real hardware, see draw_and_present_overlay()'s
 * OMNI_OVERLAY_PRESENT_REPEATS comment). The actual fix: stop the game's
 * own content from ever reaching the screen at all while the overlay is
 * open, so there's only ever one thing presenting each real frame -
 * ours. Skip calling through to the real SDL_UpdateRect/SDL_UpdateRects/
 * SDL_Flip entirely while omni_runtime_active() - sdl12-compat's own
 * implementation is a pure "blit the game's surface to the screen and
 * present" call with no other side effects to worry about losing, so
 * skipping it just means the screen stops changing except for what we
 * draw, resuming normally the moment the overlay closes. */
void SDL_UpdateRect(void *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h)
{
    static OmniSdl12UpdateRectFn real_fn;
    ensure_initialized();
    if (real_fn == NULL) {
        real_fn = (OmniSdl12UpdateRectFn)dlsym(RTLD_NEXT, "SDL_UpdateRect");
    }
    real_present_hook_seen = 1;
    if (omni_runtime_active(&runtime)) {
        draw_and_present_overlay();
        return;
    }
    if (real_fn != NULL) {
        real_fn(screen, x, y, w, h);
    }
}

void SDL_UpdateRects(void *screen, int numrects, void *rects)
{
    static OmniSdl12UpdateRectsFn real_fn;
    ensure_initialized();
    if (real_fn == NULL) {
        real_fn = (OmniSdl12UpdateRectsFn)dlsym(RTLD_NEXT, "SDL_UpdateRects");
    }
    real_present_hook_seen = 1;
    if (omni_runtime_active(&runtime)) {
        draw_and_present_overlay();
        return;
    }
    if (real_fn != NULL) {
        real_fn(screen, numrects, rects);
    }
}

int SDL_Flip(void *screen)
{
    static OmniSdl12FlipFn real_fn;
    ensure_initialized();
    if (real_fn == NULL) {
        real_fn = (OmniSdl12FlipFn)dlsym(RTLD_NEXT, "SDL_Flip");
    }
    real_present_hook_seen = 1;
    if (omni_runtime_active(&runtime)) {
        draw_and_present_overlay();
        return 0;
    }
    return real_fn != NULL ? real_fn(screen) : -1;
}

/* AmuletsArmor (and old SDL 1.2-era games generally) don't read keyboard
 * input from the event queue at all for their main gameplay loop - they
 * poll SDL_GetKeyState() every frame instead (KEYBOARD.C's KeyboardUpdate()
 * calls SDL_GetKeyState(NULL) directly). SDL_PollEvent/SDL_WaitEvent
 * suppression (the whole mechanism the rest of this file relies on for
 * "the overlay is the exclusive input target while open") only filters
 * what gets *returned* from the event queue - it does nothing to the
 * separate, always-live key-state array SDL_GetKeyState() reads, which
 * sdl12-compat keeps updated internally as a side effect of its own event
 * processing, independent of whether the app ever pulls the matching event
 * off the queue. A hook already exists for this exact problem under the
 * SDL2 name (SDL_GetKeyboardState() below, returning an all-zeroed shadow
 * array while the overlay is active) - but SDL 1.2 has an entirely
 * different function *name* for the same concept (SDL_GetKeyState, no SDL2
 * equivalent, indexed by SDLK_* keysym value rather than SDL_SCANCODE_*),
 * so the existing hook was never being reached for a game calling the SDL
 * 1.2 name. Same fix shape as SDL_UpdateRect/SDL_Flip above: safe to
 * intercept by name since SDL2 doesn't define this symbol at all. */
typedef Uint8 *(*OmniSdl12GetKeyStateFn)(int *);

Uint8 *SDL_GetKeyState(int *numkeys)
{
    static OmniSdl12GetKeyStateFn real_fn;
    static Uint8 shadow[512];
    /* A key still physically held at the exact instant the overlay
     * closes (most commonly the very confirm-key press that triggered
     * the close, e.g. Submit) was never seen as "pressed" by the game
     * while we were blanking everything - the game's own edge-detection
     * (KEYBOARD.C's KeyboardUpdate(), comparing against its own
     * previous-state table) has no record of it ever going down. The
     * instant we resume passing real state through, that still-held key
     * looks like a brand new 0->1 edge to the game, even though the
     * player never released and re-pressed anything - confirmed on real
     * hardware as text submitted through the overlay also reaching the
     * game as an Enter keypress the moment control returns to it. Masks
     * exactly the keys that were held at close time as 0 until they're
     * genuinely released (real_state transitions to 0), mirroring what
     * pass_release[] already does for the SDL_PollEvent/SDL_WaitEvent
     * suppression path below, just for this separate state-poll path and
     * in the opposite direction (open vs. close). */
    static Uint8 suppress_until_released[512];
    static int was_active;
    Uint8 *real_state;
    int real_count = 0;
    int active_now;
    int i;
    ensure_initialized();
    if (real_fn == NULL) {
        real_fn = (OmniSdl12GetKeyStateFn)dlsym(RTLD_NEXT, "SDL_GetKeyState");
    }
    if (real_fn == NULL) {
        if (numkeys != NULL) {
            *numkeys = 0;
        }
        return NULL;
    }
    real_state = real_fn(&real_count);
    refill_combo_queue();
    if (real_count > (int)sizeof(shadow)) {
        real_count = (int)sizeof(shadow);
    }
    active_now = omni_runtime_active(&runtime);
    if (was_active && !active_now) {
        for (i = 0; i < real_count; ++i) {
            suppress_until_released[i] = (real_state[i] != 0) ? 1 : 0;
        }
    }
    was_active = active_now;
    if (!active_now && combo_count == 0) {
        int need_mask = 0;
        for (i = 0; i < real_count; ++i) {
            if (suppress_until_released[i]) {
                need_mask = 1;
                break;
            }
        }
        if (!need_mask) {
            if (numkeys != NULL) {
                *numkeys = real_count;
            }
            return real_state;
        }
        memcpy(shadow, real_state, (size_t)real_count);
        for (i = 0; i < real_count; ++i) {
            if (suppress_until_released[i]) {
                if (real_state[i] == 0) {
                    suppress_until_released[i] = 0;
                } else {
                    shadow[i] = 0;
                }
            }
        }
        if (numkeys != NULL) {
            *numkeys = real_count;
        }
        return shadow;
    }
    memset(shadow, 0, sizeof(shadow));
    if (combo_count > 0) {
        if (!combo_presented) {
            const OmniKeyCombo *combo = &combo_queue[combo_head];
            for (i = 0; i < combo->count; ++i) {
                SDL_Keycode key = combo->keys[i];
                if (key >= 0 && (size_t)key < sizeof(shadow)) {
                    shadow[key] = 1;
                }
            }
            combo_presented = 1;
        } else {
            combo_head = (combo_head + 1) % OMNI_COMBO_QUEUE_CAPACITY;
            --combo_count;
            combo_presented = 0;
        }
    }
    if (numkeys != NULL) {
        *numkeys = real_count;
    }
    return shadow;
}
#endif

int SDL_PollEvent(SDL_Event *event)
{
    SDL_Event scratch;
    SDL_Event *target = event == NULL ? &scratch : event;
    ensure_initialized();
    omni_runtime_tick(&runtime);
#ifdef OMNI_HAVE_SDL12_BRIDGE
    present_overlay_fallback();
#endif
    for (;;) {
        if (poll_native(target) != 0) {
            if (!process_event(target)) {
                return 1;
            }
            continue;
        }
        if (pop_generated_dispatch(target)) {
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
#ifdef OMNI_HAVE_SDL12_BRIDGE
    present_overlay_fallback();
#endif
    if (pop_generated_dispatch(target)) {
        return 1;
    }
    if (symbols.wait_event == NULL) {
        return 0;
    }
    while (symbols.wait_event(target) != 0) {
        if (!process_event(target)) {
            return 1;
        }
        if (pop_generated_dispatch(target)) {
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
#ifdef OMNI_HAVE_SDL12_BRIDGE
    present_overlay_fallback();
#endif
    if (pop_generated_dispatch(target)) {
        return 1;
    }
    if (symbols.wait_event_timeout == NULL) {
        return 0;
    }
    while (symbols.wait_event_timeout(target, timeout) != 0) {
        if (!process_event(target)) {
            return 1;
        }
        if (pop_generated_dispatch(target)) {
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

/* SDL_GetMouseState/SDL_GetRelativeMouseState exist under this exact name
 * in both SDL2 and real SDL 1.2 (unlike SDL_GetKeyState/SDL_UpdateRect/
 * SDL_Flip above, which are SDL 1.2-only names) - normal LD_PRELOAD global
 * symbol interposition already reaches this one regardless of which SDL
 * the caller thinks it's linked against, so no OMNI_HAVE_SDL12_BRIDGE
 * gating is needed here, same as the SDL_GetKeyboardState/SDL_GetModState
 * hooks just above. Needed for the same reason SDL_GetKeyState was:
 * amuletsarmor's mouse-driven movement (MOUSEMOD.C) reads
 * SDL_GetRelativeMouseState() every frame directly, never through the
 * event queue at all, so suppressing SDL_MOUSEMOTION events alone (see
 * input_sdl.c) left the in-game cursor still moving while the overlay was
 * open - confirmed on real hardware. Real SDL2 and real SDL 1.2 return
 * different widths (Uint32 vs Uint8) for the same call; returning a plain
 * 0 is safe either way. Still calls through to drain the real underlying
 * accumulator even while suppressing the *result*, so relative deltas
 * accumulated while the overlay was open don't all land in a single
 * lurch once it closes. */
typedef Uint32 (*OmniGetRelativeMouseStateFn)(int *, int *);
typedef Uint32 (*OmniGetMouseStateFn)(int *, int *);

Uint32 SDL_GetRelativeMouseState(int *x, int *y)
{
    static OmniGetRelativeMouseStateFn real_fn;
    Uint32 result;
    int real_x = 0;
    int real_y = 0;
    ensure_initialized();
    if (real_fn == NULL) {
        real_fn = (OmniGetRelativeMouseStateFn)dlsym(RTLD_NEXT, "SDL_GetRelativeMouseState");
    }
    result = real_fn == NULL ? 0 : real_fn(&real_x, &real_y);
    if (omni_runtime_active(&runtime)) {
        if (x != NULL) {
            *x = 0;
        }
        if (y != NULL) {
            *y = 0;
        }
        return 0;
    }
    if (x != NULL) {
        *x = real_x;
    }
    if (y != NULL) {
        *y = real_y;
    }
    return result;
}

Uint32 SDL_GetMouseState(int *x, int *y)
{
    static OmniGetMouseStateFn real_fn;
    ensure_initialized();
    if (real_fn == NULL) {
        real_fn = (OmniGetMouseStateFn)dlsym(RTLD_NEXT, "SDL_GetMouseState");
    }
    if (omni_runtime_active(&runtime)) {
        /* Unlike SDL_GetRelativeMouseState, this is a live poll with no
         * accumulator to drain - nothing to call through for while
         * suppressing, just zero it directly. */
        if (x != NULL) {
            *x = 0;
        }
        if (y != NULL) {
            *y = 0;
        }
        return 0;
    }
    return real_fn == NULL ? 0 : real_fn(x, y);
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
