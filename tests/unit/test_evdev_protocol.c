#include "input_evdev.h"

#include <linux/input-event-codes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); return 1; } } while (0)

int main(void)
{
    unsigned long bits[KEY_MAX / (sizeof(unsigned long) * 8U) + 1];
    struct input_event input;
    struct input_event output[2];
    OmniEvdevAction action;
    OmniEvdevOwnership ownership;
    OmniUinputWriter writer;
    OmniEvdevBackend backend;
    OmniConfig config;
    size_t written = 0;
    memset(bits, 0, sizeof(bits));
    bits[KEY_A / (sizeof(unsigned long) * 8U)] |= 1UL << (KEY_A % (sizeof(unsigned long) * 8U));
    bits[KEY_ENTER / (sizeof(unsigned long) * 8U)] |= 1UL << (KEY_ENTER % (sizeof(unsigned long) * 8U));
    bits[KEY_BACKSPACE / (sizeof(unsigned long) * 8U)] |= 1UL << (KEY_BACKSPACE % (sizeof(unsigned long) * 8U));
    bits[KEY_SPACE / (sizeof(unsigned long) * 8U)] |= 1UL << (KEY_SPACE % (sizeof(unsigned long) * 8U));
    CHECK(omni_evdev_is_candidate(bits, sizeof(bits) / sizeof(bits[0])) == 1);
    memset(&input, 0, sizeof(input));
    input.type = EV_KEY;
    input.code = KEY_A;
    input.value = 1;
    CHECK(omni_evdev_decode(&input, &action) == 1 && action.keycode == KEY_A && action.pressed == 1);
    input.type = EV_SYN;
    input.code = SYN_DROPPED;
    CHECK(omni_evdev_decode(&input, &action) == 1 && action.sync_dropped == 1);
    CHECK(omni_uinput_encode_key(output, 2, KEY_A, 1, &written) == 0);
    CHECK(written == 2 && output[0].type == EV_KEY && output[1].code == SYN_REPORT);
    omni_evdev_ownership_init(&ownership);
    CHECK(omni_evdev_fallback_allowed(&ownership) == 1);
    ownership.grab_committed = 1;
    CHECK(omni_evdev_fallback_allowed(&ownership) == 0);
    omni_uinput_init(&writer);
    omni_evdev_release(&ownership, &writer);
    CHECK(ownership.source_fd == -1);
    omni_config_defaults(&config);
    config.input_backend = OMNI_INPUT_EVDEV;
    strncpy(config.evdev_device, "/does/not/exist", sizeof(config.evdev_device) - 1);
    CHECK(omni_evdev_start(&backend, &config) != 0);
    return 0;
}
