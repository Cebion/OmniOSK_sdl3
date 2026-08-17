#ifndef OMNI_GENERATED_EVENTS_H
#define OMNI_GENERATED_EVENTS_H

#include <SDL3/SDL.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OMNI_GENERATED_QUEUE_CAPACITY 256

typedef struct OmniGeneratedQueue {
    SDL_Event events[OMNI_GENERATED_QUEUE_CAPACITY];
    size_t head;
    size_t count;
} OmniGeneratedQueue;

void omni_generated_init(OmniGeneratedQueue *queue);
void omni_generated_clear(OmniGeneratedQueue *queue);
int omni_generated_empty(const OmniGeneratedQueue *queue);
int omni_generated_peek(const OmniGeneratedQueue *queue, SDL_Event *event);
int omni_generated_pop(OmniGeneratedQueue *queue, SDL_Event *event);
int omni_generated_peek_matching(const OmniGeneratedQueue *queue, Uint32 min_type, Uint32 max_type, SDL_Event *event);
int omni_generated_pop_matching(OmniGeneratedQueue *queue, Uint32 min_type, Uint32 max_type, SDL_Event *event);
int omni_generated_push(OmniGeneratedQueue *queue, const SDL_Event *event);
int omni_generated_ascii(OmniGeneratedQueue *queue, char character, int key_style, Uint32 window_id);
int omni_generated_text(OmniGeneratedQueue *queue, const char *text, Uint32 window_id);
int omni_generated_commit(OmniGeneratedQueue *queue, Uint32 window_id);
int omni_generated_backspace(OmniGeneratedQueue *queue, Uint32 window_id);

#ifdef __cplusplus
}
#endif

#endif
