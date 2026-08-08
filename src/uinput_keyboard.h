#ifndef OMNI_UINPUT_KEYBOARD_H
#define OMNI_UINPUT_KEYBOARD_H

#include <linux/input.h>
#include <stddef.h>

typedef struct OmniUinputWriter {
    int fd;
    int created;
    int pressed[KEY_MAX + 1];
} OmniUinputWriter;

void omni_uinput_init(OmniUinputWriter *writer);
int omni_uinput_encode_key(struct input_event *events, size_t capacity,
                           int keycode, int pressed, size_t *written);
int omni_uinput_create(OmniUinputWriter *writer);
int omni_uinput_emit(OmniUinputWriter *writer, int keycode, int pressed);
int omni_uinput_emit_ascii(OmniUinputWriter *writer, char character);
int omni_uinput_release_all(OmniUinputWriter *writer);
void omni_uinput_close(OmniUinputWriter *writer);

#endif
