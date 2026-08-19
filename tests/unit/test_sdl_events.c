#include "input_sdl.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); return 1; } } while (0)

static SDL_Event key_event(Uint32 type, SDL_Keycode key)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.keysym.sym = key;
    event.key.windowID = 7;
    return event;
}

int main(void)
{
    OmniConfig config;
    OmniOskModel model;
    OmniSdlInput input;
    OmniGeneratedQueue generated;
    SDL_Event event;
    omni_config_defaults(&config);
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_sdl_input_init(&input);
    omni_generated_init(&generated);
    event = key_event(SDL_KEYDOWN, SDLK_a);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 0);
    event = key_event(SDL_KEYDOWN, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(model.active == 1);
    event = key_event(SDL_KEYDOWN, SDLK_a);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    event = key_event(SDL_KEYDOWN, SDLK_RETURN);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(generated.count == 0);
    event = key_event(SDL_KEYUP, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    event = key_event(SDL_KEYDOWN, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(model.active == 0 && model.buffer_length == 0);
    event = key_event(SDL_KEYUP, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    config.mode = OMNI_MODE_INSTANT;
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_sdl_input_init(&input);
    event = key_event(SDL_KEYDOWN, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    event = key_event(SDL_KEYDOWN, SDLK_RETURN);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(omni_generated_pop(&generated, &event) == 1);
    CHECK(event.type == SDL_TEXTINPUT);
    CHECK(strcmp(event.text.text, "a") == 0);
    omni_generated_clear(&generated);
    event = key_event(SDL_KEYUP, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    event = key_event(SDL_KEYDOWN, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(omni_generated_pop(&generated, &event) == 1 && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN);
    CHECK(omni_generated_pop(&generated, &event) == 1 && event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_RETURN);
    CHECK(omni_generated_empty(&generated));
    config.emit_return = 0;
    omni_osk_model_activate(&model);
    omni_sdl_input_init(&input);
    event = key_event(SDL_KEYDOWN, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    event = key_event(SDL_KEYUP, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    event = key_event(SDL_KEYDOWN, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(omni_generated_empty(&generated));
    omni_generated_clear(&generated);
    {
        size_t index;
        for (index = 0; index < model.item_count; ++index) {
            if (model.items[index].kind == OMNI_ITEM_BACKSPACE) {
                model.focus = index;
                break;
            }
        }
        CHECK(index < model.item_count);
        omni_sdl_input_init(&input);
        event = key_event(SDL_KEYDOWN, SDLK_RETURN);
        CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
        CHECK(omni_generated_pop(&generated, &event) == 1 && event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE);
        CHECK(omni_generated_pop(&generated, &event) == 1 && event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_BACKSPACE);
    }
    config.buffer_dismiss = OMNI_DISMISS_STAY;
    omni_osk_model_activate(&model);
    omni_sdl_input_init(&input);
    event = key_event(SDL_KEYDOWN, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(model.active == 0);
    omni_generated_clear(&generated);
    config.event_mode = OMNI_EVENT_TEXT;
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_osk_model_activate(&model);
    omni_sdl_input_init(&input);
    event = key_event(SDL_KEYDOWN, SDLK_RETURN);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 0) == 1);
    CHECK(omni_generated_pop(&generated, &event) == 1);
    CHECK(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_a);
    omni_generated_clear(&generated);
    omni_osk_model_activate(&model);
    omni_sdl_input_init(&input);
    event = key_event(SDL_KEYDOWN, SDLK_RIGHT);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    {
        size_t first_focus = model.focus;
        event = key_event(SDL_KEYDOWN, SDLK_RIGHT);
        event.key.repeat = 1;
        CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
        CHECK(model.focus != first_focus);
    }
    CHECK(omni_generated_ascii(&generated, 'A', 1, 7) == 1);
    CHECK(omni_generated_pop(&generated, &event) == 1 && event.key.keysym.sym == SDLK_LSHIFT);
    CHECK(omni_generated_pop(&generated, &event) == 1 && event.key.keysym.sym == SDLK_a);
    CHECK(omni_generated_pop(&generated, &event) == 1 && event.type == SDL_KEYUP);
    CHECK(omni_generated_pop(&generated, &event) == 1 && event.key.keysym.sym == SDLK_LSHIFT);
    omni_generated_clear(&generated);
    CHECK(omni_generated_ascii(&generated, '\\', 1, 7) == 1);
    CHECK(omni_generated_pop(&generated, &event) == 1 && event.key.keysym.sym == SDLK_BACKSLASH);
    omni_config_defaults(&config);
    config.open_backspace_count = 2;
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_sdl_input_init(&input);
    omni_generated_clear(&generated);
    event = key_event(SDL_KEYDOWN, SDLK_F12);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(generated.count == 4);
    while (omni_generated_pop(&generated, &event)) {
        CHECK(event.key.keysym.sym == SDLK_BACKSPACE);
    }
    config.emit_return = 0;
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_osk_model_activate(&model);
    omni_sdl_input_init(&input);
    event = key_event(SDL_KEYDOWN, SDLK_RETURN);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    {
        size_t index;
        for (index = 0; index < model.item_count; ++index) {
            if (model.items[index].kind == OMNI_ITEM_SUBMIT) {
                model.focus = index;
                break;
            }
        }
        CHECK(index < model.item_count);
    }
    omni_sdl_input_init(&input);
    event = key_event(SDL_KEYDOWN, SDLK_RETURN);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(omni_generated_pop(&generated, &event) == 1 && event.type == SDL_TEXTINPUT && strcmp(event.text.text, "a") == 0);
    CHECK(omni_generated_empty(&generated));

    /* sdl12-compat presents keyboard events to the game using SDL 1.2's own
     * keysym numbering (e.g. F12 is 293, not SDL2's SDLK_F12/1073741893) -
     * key_matches() must recognize the configured toggle key either way. */
    CHECK(omni_sdl12_equivalent(SDLK_F12) == 293);
    omni_config_defaults(&config);
    CHECK(omni_osk_model_init(&model, &config) == 0);
    omni_sdl_input_init(&input);
    omni_generated_clear(&generated);
    event = key_event(SDL_KEYDOWN, (SDL_Keycode)293);
    CHECK(omni_sdl_input_handle(&input, &config, &model, &generated, &event, 1) == 1);
    CHECK(model.active == 1);

    return 0;
}
