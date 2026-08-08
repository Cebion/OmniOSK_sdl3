#include "runtime.h"

#include "diagnostics.h"

#include <string.h>

#ifdef OMNI_HAVE_EVDEV
static void raw_emit_result(OmniRuntime *runtime, const OmniModelResult *result)
{
    size_t i;
    if (result->emitted_character) {
        (void)omni_uinput_emit_ascii(&runtime->evdev.writer, result->character);
    }
    if (result->backspace) {
        (void)omni_uinput_emit(&runtime->evdev.writer, KEY_BACKSPACE, 1);
        (void)omni_uinput_emit(&runtime->evdev.writer, KEY_BACKSPACE, 0);
    }
    if (result->submitted) {
        for (i = 0; i < runtime->model.buffer_length; ++i) {
            (void)omni_uinput_emit_ascii(&runtime->evdev.writer, runtime->model.buffer[i]);
        }
        if (runtime->config.emit_return) {
            (void)omni_uinput_emit(&runtime->evdev.writer, KEY_ENTER, 1);
            (void)omni_uinput_emit(&runtime->evdev.writer, KEY_ENTER, 0);
        }
    }
    if (runtime->config.mode == OMNI_MODE_INSTANT && (result->closed || result->canceled) && runtime->config.emit_return) {
        (void)omni_uinput_emit(&runtime->evdev.writer, KEY_ENTER, 1);
        (void)omni_uinput_emit(&runtime->evdev.writer, KEY_ENTER, 0);
    }
}

static void raw_activate(OmniRuntime *runtime)
{
    omni_osk_model_activate(&runtime->model);
    omni_evdev_set_active(&runtime->evdev, 1);
}

static void raw_deactivate(OmniRuntime *runtime, int clear_buffer)
{
    omni_osk_model_deactivate(&runtime->model, clear_buffer);
    omni_evdev_set_active(&runtime->evdev, 0);
}
#endif

void omni_runtime_tick(OmniRuntime *runtime)
{
#ifndef OMNI_HAVE_EVDEV
    (void)runtime;
    return;
#else
    OmniEvdevAction raw_action;
    while (runtime->evdev_enabled && omni_evdev_pop_action(&runtime->evdev, &raw_action)) {
        OmniModelResult result;
        if (raw_action.sync_dropped) {
            omni_diag_once(20, OMNI_LOG_WARN, "raw input reported SYN_DROPPED; ignoring until the next key transition");
            continue;
        }
        if (raw_action.keycode == runtime->evdev.toggle_key) {
            if (!raw_action.pressed) {
                continue;
            }
            if (!runtime->model.active && !omni_runtime_has_presentation(runtime)) {
                continue;
            }
            if (!runtime->model.active) {
                raw_activate(runtime);
                for (size_t count = 0; count < runtime->config.open_backspace_count; ++count) {
                    (void)omni_uinput_emit(&runtime->evdev.writer, KEY_BACKSPACE, 1);
                    (void)omni_uinput_emit(&runtime->evdev.writer, KEY_BACKSPACE, 0);
                }
            } else if (runtime->config.mode == OMNI_MODE_INSTANT) {
                raw_deactivate(runtime, 1);
                if (runtime->config.emit_return) {
                    (void)omni_uinput_emit(&runtime->evdev.writer, KEY_ENTER, 1);
                    (void)omni_uinput_emit(&runtime->evdev.writer, KEY_ENTER, 0);
                }
            } else {
                result = omni_osk_model_dismiss(&runtime->model, runtime->config.buffer_dismiss);
                raw_emit_result(runtime, &result);
                omni_evdev_set_active(&runtime->evdev, runtime->model.active);
            }
            continue;
        }
        if (!runtime->model.active || !raw_action.pressed) {
            continue;
        }
        if (raw_action.keycode == runtime->evdev.controls[0]) result = omni_osk_model_apply(&runtime->model, OMNI_ACTION_UP);
        else if (raw_action.keycode == runtime->evdev.controls[1]) result = omni_osk_model_apply(&runtime->model, OMNI_ACTION_DOWN);
        else if (raw_action.keycode == runtime->evdev.controls[2]) result = omni_osk_model_apply(&runtime->model, OMNI_ACTION_LEFT);
        else if (raw_action.keycode == runtime->evdev.controls[3]) result = omni_osk_model_apply(&runtime->model, OMNI_ACTION_RIGHT);
        else if (raw_action.keycode == runtime->evdev.controls[4]) result = omni_osk_model_apply(&runtime->model, OMNI_ACTION_CONFIRM);
        else if (raw_action.keycode == runtime->evdev.controls[5]) {
            result = omni_osk_model_apply(&runtime->model, OMNI_ACTION_BACKSPACE);
        } else if (raw_action.keycode == runtime->evdev.controls[6]) result = omni_osk_model_apply(&runtime->model, OMNI_ACTION_CHARSET);
        else continue;
        raw_emit_result(runtime, &result);
        if (!runtime->model.active) {
            omni_evdev_set_active(&runtime->evdev, 0);
        }
    }
#endif
}

int omni_runtime_ensure(OmniRuntime *runtime)
{
    if (runtime->initialized) {
        return runtime->state != OMNI_RUNTIME_DISABLED;
    }
    runtime->state = OMNI_RUNTIME_PROBING;
    omni_config_load(&runtime->config);
    omni_generated_init(&runtime->generated);
    omni_sdl_input_init(&runtime->sdl_input);
    if (omni_osk_model_init(&runtime->model, &runtime->config) != 0) {
        runtime->state = OMNI_RUNTIME_DISABLED;
        runtime->initialized = 1;
        omni_diag(OMNI_LOG_ERROR, "OSK model could not initialize; continuing in pass-through mode");
        return 0;
    }
    if (runtime->config.input_backend != OMNI_INPUT_SDL) {
#ifdef OMNI_HAVE_EVDEV
        if (omni_evdev_start(&runtime->evdev, &runtime->config) == 0) {
            runtime->evdev_enabled = 1;
            omni_diag(OMNI_LOG_INFO, "evdev/uinput input backend committed before SDL ownership");
        } else if (!runtime->config.input_fallback) {
            runtime->state = OMNI_RUNTIME_DISABLED;
            runtime->initialized = 1;
            omni_diag(OMNI_LOG_ERROR, "requested raw input backend is unavailable; overlay disabled by OMNI_INPUT_FALLBACK=0");
            return 0;
        } else {
            omni_diag(OMNI_LOG_WARN, "requested raw input backend is unavailable before SDL ownership; falling back to SDL input");
        }
#else
        if (!runtime->config.input_fallback) {
            runtime->state = OMNI_RUNTIME_DISABLED;
            runtime->initialized = 1;
            omni_diag(OMNI_LOG_ERROR, "raw input support was disabled at build time; overlay disabled by OMNI_INPUT_FALLBACK=0");
            return 0;
        }
        omni_diag(OMNI_LOG_WARN, "raw input support was disabled at build time; falling back to SDL input");
#endif
    }
    runtime->state = OMNI_RUNTIME_READY;
    runtime->initialized = 1;
    omni_diag(OMNI_LOG_INFO, "SDL input backend ready; overlay starts inactive");
    return 1;
}

void omni_runtime_shutdown(OmniRuntime *runtime)
{
    if (!runtime->initialized) {
        return;
    }
    runtime->state = OMNI_RUNTIME_SHUTTING_DOWN;
    if (runtime->evdev_enabled) {
#ifdef OMNI_HAVE_EVDEV
        omni_evdev_stop(&runtime->evdev);
#endif
        runtime->evdev_enabled = 0;
    }
    omni_generated_clear(&runtime->generated);
    omni_osk_model_deactivate(&runtime->model, 1);
}

int omni_runtime_process_event(OmniRuntime *runtime, SDL_Event *event, int text_enabled)
{
    if (!omni_runtime_ensure(runtime) || runtime->state != OMNI_RUNTIME_READY) {
        return 0;
    }
    omni_runtime_tick(runtime);
    if (event->type == SDL_QUIT) {
        omni_runtime_shutdown(runtime);
        return 0;
    }
    if (!runtime->model.active && event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == runtime->config.toggle_key && !omni_runtime_has_presentation(runtime)) {
        SDL_Scancode scancode = event->key.keysym.scancode;
        if (scancode >= SDL_NUM_SCANCODES) {
            scancode = SDL_GetScancodeFromKey(event->key.keysym.sym);
        }
        runtime->sdl_input.suppressed[scancode] = 1;
        omni_diag_once(17, OMNI_LOG_INFO, "toggle received before a presentation target; keeping OSK inactive");
        return 1;
    }
    return omni_sdl_input_handle(&runtime->sdl_input, &runtime->config,
                                 &runtime->model, &runtime->generated,
                                 event, text_enabled);
}

int omni_runtime_pop_generated(OmniRuntime *runtime, SDL_Event *event)
{
    return runtime->state == OMNI_RUNTIME_READY && omni_generated_pop(&runtime->generated, event);
}

int omni_runtime_peek_generated(const OmniRuntime *runtime, SDL_Event *event)
{
    return runtime->state == OMNI_RUNTIME_READY && omni_generated_peek(&runtime->generated, event);
}

int omni_runtime_active(const OmniRuntime *runtime)
{
    return runtime->state == OMNI_RUNTIME_READY && runtime->model.active;
}

int omni_runtime_has_presentation(const OmniRuntime *runtime)
{
    return runtime->context != NULL || runtime->renderer != NULL;
}

void omni_runtime_set_window(OmniRuntime *runtime, SDL_Window *window)
{
    runtime->window = window;
}

void omni_runtime_set_renderer(OmniRuntime *runtime, SDL_Renderer *renderer)
{
    runtime->renderer = renderer;
}

void omni_runtime_set_context(OmniRuntime *runtime, SDL_GLContext context)
{
    runtime->context = context;
}

void omni_runtime_presentation_lost(OmniRuntime *runtime)
{
    omni_osk_model_deactivate(&runtime->model, 1);
    omni_generated_clear(&runtime->generated);
    omni_sdl_input_init(&runtime->sdl_input);
#ifdef OMNI_HAVE_EVDEV
    if (runtime->evdev_enabled) {
        omni_evdev_set_active(&runtime->evdev, 0);
        omni_evdev_clear_actions(&runtime->evdev);
    }
#endif
}

void omni_runtime_disable(OmniRuntime *runtime, const char *reason)
{
    if (runtime->state == OMNI_RUNTIME_DISABLED) {
        return;
    }
    omni_diag(OMNI_LOG_ERROR, "overlay disabled at %s; application remains in pass-through mode", reason);
    runtime->state = OMNI_RUNTIME_DISABLED;
    if (runtime->evdev_enabled) {
#ifdef OMNI_HAVE_EVDEV
        omni_evdev_stop(&runtime->evdev);
#endif
        runtime->evdev_enabled = 0;
    }
    omni_generated_clear(&runtime->generated);
    omni_osk_model_deactivate(&runtime->model, 1);
}
