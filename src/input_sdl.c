#include "input_sdl.h"

#include "diagnostics.h"

#include <stdio.h>
#include <string.h>

void omni_sdl_input_init(OmniSdlInput *input)
{
    memset(input, 0, sizeof(*input));
}

/* sdl12-compat translates every keyboard event it hands to the game back
 * into real SDL 1.2's own keysym numbering (Keysym20to12() in
 * SDL12_compat.c) - that's the entire point of the shim. SDL 1.2's and
 * SDL2's keysym values are numerically identical for printable/ASCII-range
 * keys, but diverge for non-printable ones (SDL2 introduced the
 * SDLK_SCANCODE_MASK-based scheme for those). A game reached only through
 * sdl12-compat never produces SDL2-numbered non-printable keysyms, so
 * key_matches() must also check against the SDL 1.2 value for the
 * configured key. Values below are from SDL 1.2's own SDL_keysym.h,
 * unchanged in decades.
 */
SDL_Keycode omni_sdl12_equivalent(SDL_Keycode sdl2_key)
{
    switch (sdl2_key) {
        case SDLK_BACKSPACE: return 8;
        case SDLK_TAB:       return 9;
        case SDLK_RETURN:    return 13;
        case SDLK_ESCAPE:    return 27;
        case SDLK_UP:        return 273;
        case SDLK_DOWN:      return 274;
        case SDLK_RIGHT:     return 275;
        case SDLK_LEFT:      return 276;
        case SDLK_F1:        return 282;
        case SDLK_F2:        return 283;
        case SDLK_F3:        return 284;
        case SDLK_F4:        return 285;
        case SDLK_F5:        return 286;
        case SDLK_F6:        return 287;
        case SDLK_F7:        return 288;
        case SDLK_F8:        return 289;
        case SDLK_F9:        return 290;
        case SDLK_F10:       return 291;
        case SDLK_F11:       return 292;
        case SDLK_F12:       return 293;
        case SDLK_LSHIFT:    return 304;
        default:             return sdl2_key;
    }
}

static int key_matches(SDL_Keycode key, SDL_Keycode configured)
{
    return key == configured || key == omni_sdl12_equivalent(configured);
}

static int emit_buffer(const OmniConfig *config, OmniOskModel *model,
                       OmniGeneratedQueue *generated, Uint32 window_id, int text_enabled)
{
    size_t chunks = model->buffer_length == 0 ? 0 : (model->buffer_length + 30) / 31;
    size_t needed = config->event_mode == OMNI_EVENT_KEY || !text_enabled
                        ? model->buffer_length * 4 : chunks;
    if (config->emit_return) {
        needed += 2;
    }
    size_t offset = 0;
    size_t original_head = generated->head;
    size_t original_count = generated->count;
    if (generated->count + needed > OMNI_GENERATED_QUEUE_CAPACITY) {
        omni_diag_once(8, OMNI_LOG_ERROR, "generated event queue is full; submission was rejected atomically");
        return 0;
    }
    if (config->event_mode == OMNI_EVENT_KEY || (config->event_mode == OMNI_EVENT_AUTO && !text_enabled)) {
        size_t i;
        for (i = 0; i < model->buffer_length; ++i) {
            if (!omni_generated_ascii(generated, model->buffer[i], 1, window_id)) {
                generated->head = original_head;
                generated->count = original_count;
                return 0;
            }
        }
    } else {
        while (offset < model->buffer_length) {
            char chunk[32];
            size_t length = model->buffer_length - offset;
            if (length > 31) {
                length = 31;
            }
            memcpy(chunk, model->buffer + offset, length);
            chunk[length] = '\0';
            if (!omni_generated_text(generated, chunk, window_id)) {
                generated->head = original_head;
                generated->count = original_count;
                return 0;
            }
            offset += length;
        }
    }
    if (config->emit_return && !omni_generated_commit(generated, window_id)) {
        generated->head = original_head;
        generated->count = original_count;
        return 0;
    }
    return 1;
}

static void emit_model_result(const OmniConfig *config, OmniOskModel *model,
                              OmniGeneratedQueue *generated, const OmniModelResult *result,
                              Uint32 window_id, int text_enabled)
{
    if (result->emitted_character) {
        int use_text = config->event_mode != OMNI_EVENT_KEY && text_enabled;
        if (!omni_generated_ascii(generated, result->character, !use_text, window_id)) {
            omni_diag_once(9, OMNI_LOG_ERROR, "generated character could not be queued");
        }
    }
    if (result->backspace && !omni_generated_backspace(generated, window_id)) {
        omni_diag_once(11, OMNI_LOG_ERROR, "generated backspace could not be queued");
    }
    if (result->submitted) {
        if (!emit_buffer(config, model, generated, window_id, text_enabled)) {
            omni_diag_once(10, OMNI_LOG_ERROR, "buffered submission failed; overlay is closing without output");
        }
    }
    if (config->mode == OMNI_MODE_INSTANT && (result->closed || result->canceled) && config->emit_return &&
        !omni_generated_commit(generated, window_id)) {
        omni_diag_once(23, OMNI_LOG_ERROR, "close Return event could not be queued");
    }
}

static OmniModelAction action_for_key(const OmniConfig *config, SDL_Keycode key)
{
    if (key_matches(key, config->up_key)) return OMNI_ACTION_UP;
    if (key_matches(key, config->down_key)) return OMNI_ACTION_DOWN;
    if (key_matches(key, config->left_key)) return OMNI_ACTION_LEFT;
    if (key_matches(key, config->right_key)) return OMNI_ACTION_RIGHT;
    if (key_matches(key, config->confirm_key)) return OMNI_ACTION_CONFIRM;
    if (key_matches(key, config->backspace_key)) return OMNI_ACTION_BACKSPACE;
    return OMNI_ACTION_CHARSET;
}

static int is_direction_key(const OmniConfig *config, SDL_Keycode key)
{
    return key_matches(key, config->up_key) || key_matches(key, config->down_key) ||
           key_matches(key, config->left_key) || key_matches(key, config->right_key);
}

int omni_sdl_input_handle(OmniSdlInput *input, const OmniConfig *config,
                          OmniOskModel *model, OmniGeneratedQueue *generated,
                          SDL_Event *event, int text_input_enabled)
{
    SDL_Keycode key;
    SDL_Scancode scancode;
    OmniModelResult model_result;
    if (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP) {
        if (model->active &&
            (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN ||
             event->type == SDL_MOUSEBUTTONUP)) {
            /* While the overlay is open, it should be the exclusive input
             * target - a game whose primary controls route through mouse
             * events (e.g. a gptokeyb left-stick-as-mouse-movement mapping)
             * would otherwise keep receiving them underneath the overlay,
             * moving the player around while the user thinks they're only
             * navigating the keyboard. */
            return 1;
        }
        return model->active && (event->type == SDL_TEXTINPUT || event->type == SDL_TEXTEDITING);
    }
    key = event->key.keysym.sym;
    scancode = event->key.keysym.scancode;
    if (scancode >= SDL_NUM_SCANCODES) {
        scancode = SDL_GetScancodeFromKey(key);
    }
    if (key_matches(key, config->toggle_key)) {
        if (event->type == SDL_KEYDOWN) {
            if (input->toggle_down || event->key.repeat != 0) {
                return 1;
            }
            input->toggle_down = 1;
            if (!model->active) {
                memcpy(input->pass_release, input->held, sizeof(input->held));
                input->pass_release[scancode] = 0;
                omni_osk_model_activate(model);
                for (size_t count = 0; count < config->open_backspace_count; ++count) {
                    if (!omni_generated_backspace(generated, event->key.windowID)) {
                        omni_diag_once(24, OMNI_LOG_ERROR, "open backspace event could not be queued");
                        break;
                    }
                }
            } else if (config->mode == OMNI_MODE_INSTANT) {
                model_result = omni_osk_model_dismiss(model, OMNI_DISMISS_CANCEL);
                emit_model_result(config, model, generated, &model_result, event->key.windowID, text_input_enabled);
            } else {
                model_result = omni_osk_model_dismiss(model, config->buffer_dismiss);
                emit_model_result(config, model, generated, &model_result, event->key.windowID, text_input_enabled);
            }
        } else {
            input->toggle_down = 0;
            input->held[scancode] = 0;
        }
        return 1;
    }
    if (!model->active) {
        if (event->type == SDL_KEYDOWN) {
            input->held[scancode] = 1;
        } else if (input->suppressed[scancode] != 0) {
            input->suppressed[scancode] = 0;
            input->held[scancode] = 0;
            return 1;
        } else {
            input->held[scancode] = 0;
        }
        return 0;
    }
    if (event->type == SDL_KEYUP) {
        input->held[scancode] = 0;
        if (input->pass_release[scancode] != 0) {
            input->pass_release[scancode] = 0;
            return 0;
        }
        input->suppressed[scancode] = 0;
        return 1;
    }
    if (input->held[scancode] != 0 && !(event->key.repeat != 0 && is_direction_key(config, key))) {
        return 1;
    }
    input->held[scancode] = 1;
    if (key_matches(key, config->up_key) || key_matches(key, config->down_key) ||
        key_matches(key, config->left_key) || key_matches(key, config->right_key) ||
        key_matches(key, config->confirm_key) || key_matches(key, config->backspace_key)) {
        if (event->key.repeat == 0 || is_direction_key(config, key)) {
            model_result = omni_osk_model_apply(model, action_for_key(config, key));
            emit_model_result(config, model, generated, &model_result, event->key.windowID, text_input_enabled);
        }
        input->suppressed[scancode] = 1;
        return 1;
    }
    if (key_matches(key, config->charset_key)) {
        if (event->key.repeat == 0) {
            (void)omni_osk_model_apply(model, OMNI_ACTION_CHARSET);
        }
        input->suppressed[scancode] = 1;
        return 1;
    }
    input->suppressed[scancode] = 1;
    return 1;
}
