#ifndef OMNI_INPUT_EVDEV_H
#define OMNI_INPUT_EVDEV_H

#include "uinput_keyboard.h"
#include "config.h"

#include <linux/input.h>
#include <pthread.h>
#include <stddef.h>

#define OMNI_RAW_ACTION_CAPACITY 128

typedef struct OmniEvdevOwnership {
    int source_fd;
    int grab_committed;
    int injection_started;
} OmniEvdevOwnership;

typedef struct OmniEvdevAction {
    int keycode;
    int pressed;
    int sync_dropped;
} OmniEvdevAction;

typedef struct OmniEvdevBackend {
    OmniEvdevOwnership ownership;
    OmniUinputWriter writer;
    pthread_t worker;
    pthread_mutex_t action_mutex;
    OmniEvdevAction actions[OMNI_RAW_ACTION_CAPACITY];
    size_t action_head;
    size_t action_count;
    volatile int stop;
    volatile int active;
    int running;
    int toggle_key;
    int controls[7];
} OmniEvdevBackend;

void omni_evdev_ownership_init(OmniEvdevOwnership *ownership);
int omni_evdev_is_candidate(const unsigned long *key_bits, size_t words);
int omni_evdev_decode(const struct input_event *event, OmniEvdevAction *action);
int omni_evdev_fallback_allowed(const OmniEvdevOwnership *ownership);
void omni_evdev_release(OmniEvdevOwnership *ownership, OmniUinputWriter *writer);
int omni_evdev_start(OmniEvdevBackend *backend, const OmniConfig *config);
void omni_evdev_stop(OmniEvdevBackend *backend);
void omni_evdev_set_active(OmniEvdevBackend *backend, int active);
void omni_evdev_clear_actions(OmniEvdevBackend *backend);
int omni_evdev_pop_action(OmniEvdevBackend *backend, OmniEvdevAction *action);

#endif
