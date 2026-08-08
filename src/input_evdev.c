#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "input_evdev.h"

#include "diagnostics.h"

#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

void omni_evdev_ownership_init(OmniEvdevOwnership *ownership)
{
    memset(ownership, 0, sizeof(*ownership));
    ownership->source_fd = -1;
}

int omni_evdev_is_candidate(const unsigned long *key_bits, size_t words)
{
    size_t word_a = KEY_A / (sizeof(unsigned long) * 8U);
    size_t word_enter = KEY_ENTER / (sizeof(unsigned long) * 8U);
    size_t word_backspace = KEY_BACKSPACE / (sizeof(unsigned long) * 8U);
    size_t word_space = KEY_SPACE / (sizeof(unsigned long) * 8U);
    unsigned long mask_a = 1UL << (KEY_A % (sizeof(unsigned long) * 8U));
    unsigned long mask_enter = 1UL << (KEY_ENTER % (sizeof(unsigned long) * 8U));
    unsigned long mask_backspace = 1UL << (KEY_BACKSPACE % (sizeof(unsigned long) * 8U));
    unsigned long mask_space = 1UL << (KEY_SPACE % (sizeof(unsigned long) * 8U));
    return key_bits != NULL && word_a < words && word_enter < words &&
           word_backspace < words && word_space < words &&
           (key_bits[word_a] & mask_a) != 0 && (key_bits[word_enter] & mask_enter) != 0 &&
           (key_bits[word_backspace] & mask_backspace) != 0 && (key_bits[word_space] & mask_space) != 0;
}

int omni_evdev_decode(const struct input_event *event, OmniEvdevAction *action)
{
    if (event == NULL || action == NULL) {
        return -1;
    }
    memset(action, 0, sizeof(*action));
    if (event->type == EV_SYN && event->code == SYN_DROPPED) {
        action->sync_dropped = 1;
        return 1;
    }
    if (event->type != EV_KEY || event->value < 0 || event->value > 2) {
        return 0;
    }
    action->keycode = event->code;
    action->pressed = event->value != 0;
    return 1;
}

int omni_evdev_fallback_allowed(const OmniEvdevOwnership *ownership)
{
    return ownership != NULL && !ownership->grab_committed && !ownership->injection_started;
}

void omni_evdev_release(OmniEvdevOwnership *ownership, OmniUinputWriter *writer)
{
    if (ownership == NULL) {
        return;
    }
    if (writer != NULL) {
        omni_uinput_close(writer);
    }
    if (ownership->source_fd >= 0) {
        (void)ioctl(ownership->source_fd, EVIOCGRAB, 0);
        close(ownership->source_fd);
    }
    omni_evdev_ownership_init(ownership);
}

static int linux_key_for_sdl(SDL_Keycode key)
{
    switch (key) {
    case SDLK_F12: return KEY_F12;
    case SDLK_UP: return KEY_UP;
    case SDLK_DOWN: return KEY_DOWN;
    case SDLK_LEFT: return KEY_LEFT;
    case SDLK_RIGHT: return KEY_RIGHT;
    case SDLK_RETURN: return KEY_ENTER;
    case SDLK_BACKSPACE: return KEY_BACKSPACE;
    case SDLK_TAB: return KEY_TAB;
    default: return -1;
    }
}

static int is_control(const OmniEvdevBackend *backend, int keycode)
{
    size_t i;
    for (i = 0; i < sizeof(backend->controls) / sizeof(backend->controls[0]); ++i) {
        if (backend->controls[i] == keycode) {
            return 1;
        }
    }
    return keycode == backend->toggle_key;
}

static int write_event(OmniUinputWriter *writer, const struct input_event *event)
{
    const unsigned char *bytes = (const unsigned char *)event;
    size_t offset = 0;
    while (offset < sizeof(*event)) {
        ssize_t written = write(writer->fd, bytes + offset, sizeof(*event) - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return -1;
        }
        offset += (size_t)written;
    }
    return 0;
}

static void enqueue_action(OmniEvdevBackend *backend, const OmniEvdevAction *action, int observed_active)
{
    pthread_mutex_lock(&backend->action_mutex);
    if (observed_active && !__atomic_load_n(&backend->active, __ATOMIC_ACQUIRE)) {
        pthread_mutex_unlock(&backend->action_mutex);
        return;
    }
    if (backend->action_count < OMNI_RAW_ACTION_CAPACITY) {
        size_t tail = (backend->action_head + backend->action_count) % OMNI_RAW_ACTION_CAPACITY;
        backend->actions[tail] = *action;
        ++backend->action_count;
    } else {
        omni_diag_once(18, OMNI_LOG_ERROR, "raw input action queue is full; dropping one action");
    }
    pthread_mutex_unlock(&backend->action_mutex);
}

static void queue_action(OmniEvdevBackend *backend, const struct input_event *event, int observed_active)
{
    OmniEvdevAction action;
    if (event->type != EV_KEY || event->value < 0 || event->value > 1) {
        return;
    }
    action.keycode = event->code;
    action.pressed = event->value != 0;
    action.sync_dropped = 0;
    enqueue_action(backend, &action, observed_active);
}

static void *evdev_worker(void *data)
{
    OmniEvdevBackend *backend = (OmniEvdevBackend *)data;
    struct input_event event;
    while (__atomic_load_n(&backend->stop, __ATOMIC_ACQUIRE) == 0) {
        ssize_t result = read(backend->ownership.source_fd, &event, sizeof(event));
        if (result == (ssize_t)sizeof(event)) {
            int active = __atomic_load_n(&backend->active, __ATOMIC_ACQUIRE) != 0;
            if ((active && is_control(backend, event.code)) ||
                (!active && event.type == EV_KEY && event.code == backend->toggle_key) ||
                (active && event.type == EV_SYN && event.code == SYN_DROPPED)) {
                if (event.type == EV_SYN && event.code == SYN_DROPPED) {
                    OmniEvdevAction action = {0, 0, 1};
                    enqueue_action(backend, &action, active);
                } else {
                    queue_action(backend, &event, active);
                }
            } else if (!active && backend->writer.fd >= 0) {
                if (write_event(&backend->writer, &event) != 0) {
                    omni_diag_once(19, OMNI_LOG_ERROR, "raw virtual keyboard write failed");
                    break;
                }
            }
            continue;
        }
        if (result < 0 && (errno == EINTR || errno == EAGAIN)) {
            continue;
        }
        break;
    }
    return NULL;
}

int omni_evdev_start(OmniEvdevBackend *backend, const OmniConfig *config)
{
    unsigned long key_bits[(KEY_MAX / (sizeof(unsigned long) * 8U)) + 1];
    int fd;
    size_t i;
    memset(backend, 0, sizeof(*backend));
    omni_evdev_ownership_init(&backend->ownership);
    omni_uinput_init(&backend->writer);
    pthread_mutex_init(&backend->action_mutex, NULL);
    backend->stop = 0;
    backend->active = 0;
    if (strcmp(config->evdev_device, "auto") == 0) {
        pthread_mutex_destroy(&backend->action_mutex);
        return -1;
    }
    fd = open(config->evdev_device, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        pthread_mutex_destroy(&backend->action_mutex);
        return -1;
    }
    memset(key_bits, 0, sizeof(key_bits));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0 ||
        !omni_evdev_is_candidate(key_bits, sizeof(key_bits) / sizeof(key_bits[0])) ||
        omni_uinput_create(&backend->writer) != 0 || ioctl(fd, EVIOCGRAB, 1) < 0) {
        close(fd);
        omni_uinput_close(&backend->writer);
        pthread_mutex_destroy(&backend->action_mutex);
        return -1;
    }
    backend->ownership.source_fd = fd;
    backend->ownership.grab_committed = 1;
    backend->ownership.injection_started = 1;
    backend->toggle_key = linux_key_for_sdl(config->toggle_key);
    backend->controls[0] = linux_key_for_sdl(config->up_key);
    backend->controls[1] = linux_key_for_sdl(config->down_key);
    backend->controls[2] = linux_key_for_sdl(config->left_key);
    backend->controls[3] = linux_key_for_sdl(config->right_key);
    backend->controls[4] = linux_key_for_sdl(config->confirm_key);
    backend->controls[5] = linux_key_for_sdl(config->backspace_key);
    backend->controls[6] = linux_key_for_sdl(config->charset_key);
    for (i = 0; i < sizeof(backend->controls) / sizeof(backend->controls[0]); ++i) {
        if (backend->controls[i] < 0) {
            backend->controls[i] = KEY_RESERVED;
        }
    }
    if (pthread_create(&backend->worker, NULL, evdev_worker, backend) != 0) {
        omni_evdev_release(&backend->ownership, &backend->writer);
        pthread_mutex_destroy(&backend->action_mutex);
        return -1;
    }
    backend->running = 1;
    return 0;
}

void omni_evdev_stop(OmniEvdevBackend *backend)
{
    if (backend == NULL) {
        return;
    }
    __atomic_store_n(&backend->stop, 1, __ATOMIC_RELEASE);
    if (backend->running) {
        (void)pthread_join(backend->worker, NULL);
    }
    omni_evdev_release(&backend->ownership, &backend->writer);
    pthread_mutex_destroy(&backend->action_mutex);
    backend->running = 0;
}

void omni_evdev_set_active(OmniEvdevBackend *backend, int active)
{
    if (backend != NULL) {
        __atomic_store_n(&backend->active, active != 0, __ATOMIC_RELEASE);
    }
}

void omni_evdev_clear_actions(OmniEvdevBackend *backend)
{
    if (backend == NULL) {
        return;
    }
    pthread_mutex_lock(&backend->action_mutex);
    backend->action_head = 0;
    backend->action_count = 0;
    pthread_mutex_unlock(&backend->action_mutex);
}

int omni_evdev_pop_action(OmniEvdevBackend *backend, OmniEvdevAction *action)
{
    if (backend == NULL || action == NULL) {
        return 0;
    }
    pthread_mutex_lock(&backend->action_mutex);
    if (backend->action_count == 0) {
        pthread_mutex_unlock(&backend->action_mutex);
        return 0;
    }
    *action = backend->actions[backend->action_head];
    backend->action_head = (backend->action_head + 1) % OMNI_RAW_ACTION_CAPACITY;
    --backend->action_count;
    pthread_mutex_unlock(&backend->action_mutex);
    return 1;
}
