#include "input_sdl.h"

#include "diagnostics.h"

#include <string.h>

void omni_sdl_input_init(OmniSdlInput *input)
{
    memset(input, 0, sizeof(*input));
}

static int key_matches(SDL_Keycode key, SDL_Keycode configured)
{
    return key == configured;
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
    if (event->type != SDL_EVENT_KEY_DOWN && event->type != SDL_EVENT_KEY_UP) {
        return model->active && (event->type == SDL_EVENT_TEXT_INPUT || event->type == SDL_EVENT_TEXT_EDITING);
    }
    key = event->key.key;
    scancode = event->key.scancode;
    if (scancode >= SDL_SCANCODE_COUNT) {
        scancode = SDL_GetScancodeFromKey(key, NULL);
    }
    if (key_matches(key, config->toggle_key)) {
        if (event->type == SDL_EVENT_KEY_DOWN) {
            if (input->toggle_down || event->key.repeat) {
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
        if (event->type == SDL_EVENT_KEY_DOWN) {
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
    if (event->type == SDL_EVENT_KEY_UP) {
        input->held[scancode] = 0;
        if (input->pass_release[scancode] != 0) {
            input->pass_release[scancode] = 0;
            return 0;
        }
        input->suppressed[scancode] = 0;
        return 1;
    }
    if (input->held[scancode] != 0 && !(event->key.repeat && is_direction_key(config, key))) {
        return 1;
    }
    input->held[scancode] = 1;
    if (key_matches(key, config->up_key) || key_matches(key, config->down_key) ||
        key_matches(key, config->left_key) || key_matches(key, config->right_key) ||
        key_matches(key, config->confirm_key) || key_matches(key, config->backspace_key)) {
        if (!event->key.repeat || is_direction_key(config, key)) {
            model_result = omni_osk_model_apply(model, action_for_key(config, key));
            emit_model_result(config, model, generated, &model_result, event->key.windowID, text_input_enabled);
        }
        input->suppressed[scancode] = 1;
        return 1;
    }
    if (key_matches(key, config->charset_key)) {
        if (!event->key.repeat) {
            (void)omni_osk_model_apply(model, OMNI_ACTION_CHARSET);
        }
        input->suppressed[scancode] = 1;
        return 1;
    }
    input->suppressed[scancode] = 1;
    return 1;
}
