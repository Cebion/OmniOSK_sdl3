#include "generated_events.h"

#include <string.h>

static int queue_room(const OmniGeneratedQueue *queue, size_t needed)
{
    return needed <= OMNI_GENERATED_QUEUE_CAPACITY - queue->count;
}

void omni_generated_init(OmniGeneratedQueue *queue)
{
    memset(queue, 0, sizeof(*queue));
}

void omni_generated_clear(OmniGeneratedQueue *queue)
{
    queue->head = 0;
    queue->count = 0;
}

int omni_generated_empty(const OmniGeneratedQueue *queue)
{
    return queue->count == 0;
}

int omni_generated_peek(const OmniGeneratedQueue *queue, SDL_Event *event)
{
    if (queue->count == 0) {
        return 0;
    }
    *event = queue->events[queue->head];
    return 1;
}

int omni_generated_pop(OmniGeneratedQueue *queue, SDL_Event *event)
{
    if (!omni_generated_peek(queue, event)) {
        return 0;
    }
    queue->head = (queue->head + 1) % OMNI_GENERATED_QUEUE_CAPACITY;
    --queue->count;
    return 1;
}

int omni_generated_peek_matching(const OmniGeneratedQueue *queue, Uint32 min_type, Uint32 max_type, SDL_Event *event)
{
    size_t i;
    if (queue == NULL || event == NULL) {
        return 0;
    }
    for (i = 0; i < queue->count; ++i) {
        size_t index = (queue->head + i) % OMNI_GENERATED_QUEUE_CAPACITY;
        if (queue->events[index].type >= min_type && queue->events[index].type <= max_type) {
            *event = queue->events[index];
            return 1;
        }
    }
    return 0;
}

int omni_generated_pop_matching(OmniGeneratedQueue *queue, Uint32 min_type, Uint32 max_type, SDL_Event *event)
{
    OmniGeneratedQueue retained;
    SDL_Event current;
    int found = 0;
    omni_generated_init(&retained);
    while (omni_generated_pop(queue, &current)) {
        if (!found && current.type >= min_type && current.type <= max_type) {
            *event = current;
            found = 1;
        } else {
            (void)omni_generated_push(&retained, &current);
        }
    }
    *queue = retained;
    return found;
}

int omni_generated_push(OmniGeneratedQueue *queue, const SDL_Event *event)
{
    size_t tail;
    if (!queue_room(queue, 1)) {
        return 0;
    }
    tail = (queue->head + queue->count) % OMNI_GENERATED_QUEUE_CAPACITY;
    queue->events[tail] = *event;
    ++queue->count;
    return 1;
}

static int ascii_key(char character, SDL_Keycode *key, SDL_Keymod *mod)
{
    *mod = KMOD_NONE;
    if (character >= 'a' && character <= 'z') {
        *key = (SDL_Keycode)(SDLK_a + character - 'a');
        return 1;
    }
    if (character >= 'A' && character <= 'Z') {
        *key = (SDL_Keycode)(SDLK_a + character - 'A');
        *mod = KMOD_SHIFT;
        return 1;
    }
    if (character >= '0' && character <= '9') {
        *key = (SDL_Keycode)(SDLK_0 + character - '0');
        return 1;
    }
    switch (character) {
    case ' ': *key = SDLK_SPACE; return 1;
    case '!': *key = SDLK_1; *mod = KMOD_SHIFT; return 1;
    case '@': *key = SDLK_2; *mod = KMOD_SHIFT; return 1;
    case '#': *key = SDLK_3; *mod = KMOD_SHIFT; return 1;
    case '$': *key = SDLK_4; *mod = KMOD_SHIFT; return 1;
    case '%': *key = SDLK_5; *mod = KMOD_SHIFT; return 1;
    case '^': *key = SDLK_6; *mod = KMOD_SHIFT; return 1;
    case '&': *key = SDLK_7; *mod = KMOD_SHIFT; return 1;
    case '*': *key = SDLK_8; *mod = KMOD_SHIFT; return 1;
    case '(': *key = SDLK_9; *mod = KMOD_SHIFT; return 1;
    case ')': *key = SDLK_0; *mod = KMOD_SHIFT; return 1;
    case '-': *key = SDLK_MINUS; return 1;
    case '_': *key = SDLK_MINUS; *mod = KMOD_SHIFT; return 1;
    case '=': *key = SDLK_EQUALS; return 1;
    case '+': *key = SDLK_EQUALS; *mod = KMOD_SHIFT; return 1;
    case '[': *key = SDLK_LEFTBRACKET; return 1;
    case '{': *key = SDLK_LEFTBRACKET; *mod = KMOD_SHIFT; return 1;
    case ']': *key = SDLK_RIGHTBRACKET; return 1;
    case '}': *key = SDLK_RIGHTBRACKET; *mod = KMOD_SHIFT; return 1;
    case ';': *key = SDLK_SEMICOLON; return 1;
    case ':': *key = SDLK_SEMICOLON; *mod = KMOD_SHIFT; return 1;
    case '\'': *key = SDLK_QUOTE; return 1;
    case '"': *key = SDLK_QUOTE; *mod = KMOD_SHIFT; return 1;
    case ',': *key = SDLK_COMMA; return 1;
    case '<': *key = SDLK_COMMA; *mod = KMOD_SHIFT; return 1;
    case '.': *key = SDLK_PERIOD; return 1;
    case '>': *key = SDLK_PERIOD; *mod = KMOD_SHIFT; return 1;
    case '/': *key = SDLK_SLASH; return 1;
    case '?': *key = SDLK_SLASH; *mod = KMOD_SHIFT; return 1;
    case '|': *key = SDLK_BACKSLASH; *mod = KMOD_SHIFT; return 1;
    case '\\': *key = SDLK_BACKSLASH; return 1;
    case '`': *key = SDLK_BACKQUOTE; return 1;
    case '~': *key = SDLK_BACKQUOTE; *mod = KMOD_SHIFT; return 1;
    default: return 0;
    }
}

static void fill_key_event(SDL_Event *event, Uint32 type, SDL_Keycode key, SDL_Keymod mod, Uint32 window_id)
{
    memset(event, 0, sizeof(*event));
    event->type = type;
    event->key.windowID = window_id;
    event->key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
    event->key.repeat = 0;
    event->key.keysym.sym = key;
    event->key.keysym.mod = mod;
    event->key.keysym.scancode = SDL_GetScancodeFromKey(key);
}

int omni_generated_ascii(OmniGeneratedQueue *queue, char character, int key_style, Uint32 window_id)
{
    SDL_Event down;
    SDL_Event up;
    SDL_Event shift_down;
    SDL_Event shift_up;
    SDL_Keycode key;
    SDL_Keymod mod;
    if (!ascii_key(character, &key, &mod)) {
        return 0;
    }
    if (!key_style) {
        char text[2] = {character, '\0'};
        return omni_generated_text(queue, text, window_id);
    }
    if (!queue_room(queue, mod == KMOD_SHIFT ? 4 : 2)) {
        return 0;
    }
    fill_key_event(&down, SDL_KEYDOWN, key, mod, window_id);
    fill_key_event(&up, SDL_KEYUP, key, mod, window_id);
    if (mod == KMOD_SHIFT) {
        fill_key_event(&shift_down, SDL_KEYDOWN, SDLK_LSHIFT, KMOD_SHIFT, window_id);
        fill_key_event(&shift_up, SDL_KEYUP, SDLK_LSHIFT, KMOD_NONE, window_id);
        return omni_generated_push(queue, &shift_down) && omni_generated_push(queue, &down) &&
               omni_generated_push(queue, &up) && omni_generated_push(queue, &shift_up);
    }
    return omni_generated_push(queue, &down) && omni_generated_push(queue, &up);
}

int omni_generated_text(OmniGeneratedQueue *queue, const char *text, Uint32 window_id)
{
    SDL_Event event;
    size_t length = strlen(text);
    if (length == 0 || length > 31 || !queue_room(queue, 1)) {
        return 0;
    }
    memset(&event, 0, sizeof(event));
    event.type = SDL_TEXTINPUT;
    event.text.windowID = window_id;
    strncpy(event.text.text, text, sizeof(event.text.text) - 1);
    return omni_generated_push(queue, &event);
}

int omni_generated_commit(OmniGeneratedQueue *queue, Uint32 window_id)
{
    SDL_Event down;
    SDL_Event up;
    if (!queue_room(queue, 2)) {
        return 0;
    }
    fill_key_event(&down, SDL_KEYDOWN, SDLK_RETURN, KMOD_NONE, window_id);
    fill_key_event(&up, SDL_KEYUP, SDLK_RETURN, KMOD_NONE, window_id);
    return omni_generated_push(queue, &down) && omni_generated_push(queue, &up);
}

int omni_generated_backspace(OmniGeneratedQueue *queue, Uint32 window_id)
{
    SDL_Event down;
    SDL_Event up;
    if (!queue_room(queue, 2)) {
        return 0;
    }
    fill_key_event(&down, SDL_KEYDOWN, SDLK_BACKSPACE, KMOD_NONE, window_id);
    fill_key_event(&up, SDL_KEYUP, SDLK_BACKSPACE, KMOD_NONE, window_id);
    return omni_generated_push(queue, &down) && omni_generated_push(queue, &up);
}
