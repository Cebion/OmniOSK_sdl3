#include "uinput_keyboard.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

void omni_uinput_init(OmniUinputWriter *writer)
{
    memset(writer, 0, sizeof(*writer));
    writer->fd = -1;
}

int omni_uinput_encode_key(struct input_event *events, size_t capacity,
                           int keycode, int pressed, size_t *written)
{
    if (events == NULL || written == NULL || capacity < 2 || keycode < 0 || keycode > KEY_MAX) {
        return -1;
    }
    memset(events, 0, sizeof(events[0]) * 2);
    events[0].type = EV_KEY;
    events[0].code = (unsigned short)keycode;
    events[0].value = pressed ? 1 : 0;
    events[1].type = EV_SYN;
    events[1].code = SYN_REPORT;
    *written = 2;
    return 0;
}

static int write_full(int fd, const void *data, size_t length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = write(fd, bytes + offset, length - offset);
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

int omni_uinput_create(OmniUinputWriter *writer)
{
    struct uinput_user_dev device;
    size_t key;
    omni_uinput_init(writer);
    writer->fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (writer->fd < 0) {
        return -1;
    }
    if (ioctl(writer->fd, UI_SET_EVBIT, EV_KEY) < 0 || ioctl(writer->fd, UI_SET_EVBIT, EV_SYN) < 0) {
        omni_uinput_close(writer);
        return -1;
    }
    for (key = 0; key <= KEY_MAX; ++key) {
        if (ioctl(writer->fd, UI_SET_KEYBIT, key) < 0) {
            omni_uinput_close(writer);
            return -1;
        }
    }
    memset(&device, 0, sizeof(device));
    strncpy(device.name, "omni-osk-virtual", sizeof(device.name) - 1);
    device.id.bustype = BUS_USB;
    device.id.vendor = 0x4f4d;
    device.id.product = 0x4f53;
    if (write_full(writer->fd, &device, sizeof(device)) != 0 || ioctl(writer->fd, UI_DEV_CREATE) < 0) {
        omni_uinput_close(writer);
        return -1;
    }
    writer->created = 1;
    return 0;
}

int omni_uinput_emit(OmniUinputWriter *writer, int keycode, int pressed)
{
    struct input_event events[2];
    size_t written;
    if (writer == NULL || writer->fd < 0 || keycode < 0 || keycode > KEY_MAX ||
        omni_uinput_encode_key(events, 2, keycode, pressed, &written) != 0 ||
        write_full(writer->fd, events, sizeof(events[0]) * written) != 0) {
        return -1;
    }
    writer->pressed[keycode] = pressed != 0;
    return 0;
}

static int ascii_linux_key(char character, int *keycode, int *shift)
{
    *shift = 0;
    if (character >= 'a' && character <= 'z') {
        *keycode = KEY_A + character - 'a';
        return 0;
    }
    if (character >= 'A' && character <= 'Z') {
        *keycode = KEY_A + character - 'A';
        *shift = 1;
        return 0;
    }
    if (character >= '0' && character <= '9') {
        *keycode = KEY_0 + character - '0';
        return 0;
    }
    if (character == ' ') { *keycode = KEY_SPACE; return 0; }
    if (character == '\\') { *keycode = KEY_BACKSLASH; return 0; }
    return -1;
}

int omni_uinput_emit_ascii(OmniUinputWriter *writer, char character)
{
    int keycode;
    int shift;
    if (ascii_linux_key(character, &keycode, &shift) != 0) {
        return -1;
    }
    if (shift && omni_uinput_emit(writer, KEY_LEFTSHIFT, 1) != 0) {
        return -1;
    }
    if (omni_uinput_emit(writer, keycode, 1) != 0 || omni_uinput_emit(writer, keycode, 0) != 0) {
        return -1;
    }
    if (shift && omni_uinput_emit(writer, KEY_LEFTSHIFT, 0) != 0) {
        return -1;
    }
    return 0;
}

int omni_uinput_release_all(OmniUinputWriter *writer)
{
    struct input_event events[2];
    size_t i;
    size_t written;
    if (writer == NULL) {
        return -1;
    }
    for (i = 0; i <= KEY_MAX; ++i) {
        if (writer->pressed[i] == 0) {
            continue;
        }
        if (omni_uinput_encode_key(events, 2, (int)i, 0, &written) != 0) {
            return -1;
        }
        if (writer->fd >= 0 && write_full(writer->fd, events, sizeof(events[0]) * written) != 0) {
            return -1;
        }
        writer->pressed[i] = 0;
    }
    return 0;
}

void omni_uinput_close(OmniUinputWriter *writer)
{
    if (writer == NULL) {
        return;
    }
    (void)omni_uinput_release_all(writer);
    if (writer->fd >= 0) {
        if (writer->created) {
            (void)ioctl(writer->fd, UI_DEV_DESTROY);
        }
        close(writer->fd);
    }
    writer->fd = -1;
    writer->created = 0;
}
