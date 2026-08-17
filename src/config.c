#include "config.h"

#include "diagnostics.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct KeyName {
    const char *name;
    SDL_Keycode key;
} KeyName;

static const KeyName key_names[] = {
    {"F12", SDLK_F12}, {"UP", SDLK_UP}, {"DOWN", SDLK_DOWN},
    {"LEFT", SDLK_LEFT}, {"RIGHT", SDLK_RIGHT}, {"RETURN", SDLK_RETURN},
    {"ENTER", SDLK_RETURN}, {"BACKSPACE", SDLK_BACKSPACE}, {"TAB", SDLK_TAB},
    {"ESCAPE", SDLK_ESCAPE}, {"SPACE", SDLK_SPACE}, {"A", SDLK_A},
    {"B", SDLK_B}, {"C", SDLK_C}, {"D", SDLK_D}, {"E", SDLK_E},
    {"F", SDLK_F}, {"G", SDLK_G}, {"H", SDLK_H}, {"I", SDLK_I},
    {"J", SDLK_J}, {"K", SDLK_K}, {"L", SDLK_L}, {"M", SDLK_M},
    {"N", SDLK_N}, {"O", SDLK_O}, {"P", SDLK_P}, {"Q", SDLK_Q},
    {"R", SDLK_R}, {"S", SDLK_S}, {"T", SDLK_T}, {"U", SDLK_U},
    {"V", SDLK_V}, {"W", SDLK_W}, {"X", SDLK_X}, {"Y", SDLK_Y},
    {"Z", SDLK_Z}
};

static const char *env_value(const char *name)
{
    const char *value = getenv(name);
    return value == NULL ? "" : value;
}

static int equals(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static int parse_bool(const char *value, int fallback, const char *name)
{
    if (value[0] == '\0') {
        return fallback;
    }
    if (strcmp(value, "0") == 0 || equals(value, "false")) {
        return 0;
    }
    if (strcmp(value, "1") == 0 || equals(value, "true")) {
        return 1;
    }
    omni_diag(OMNI_LOG_WARN, "%s has unknown value '%s'; using default", name, value);
    return fallback;
}

static float parse_float(const char *value, float fallback, float minimum, float maximum, const char *name)
{
    char *end = NULL;
    float result;
    errno = 0;
    if (value[0] == '\0') {
        return fallback;
    }
    result = strtof(value, &end);
    if (errno != 0 || end == value || *end != '\0' || !isfinite(result) || result < minimum || result > maximum) {
        omni_diag(OMNI_LOG_WARN, "%s has invalid value '%s'; using default", name, value);
        return fallback;
    }
    return result;
}

static int hex_digit(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static OmniColor parse_color(const char *value, OmniColor fallback, const char *name)
{
    const char *digits = value;
    size_t length;
    OmniColor result = fallback;
    size_t index;
    if (value[0] == '\0') {
        return fallback;
    }
    if (digits[0] == '#') {
        ++digits;
    }
    length = strlen(digits);
    if (length != 6 && length != 8) {
        omni_diag(OMNI_LOG_WARN, "%s must be RRGGBB or RRGGBBAA; using default", name);
        return fallback;
    }
    for (index = 0; index < length; ++index) {
        if (hex_digit(digits[index]) < 0) {
            omni_diag(OMNI_LOG_WARN, "%s has invalid color '%s'; using default", name, value);
            return fallback;
        }
    }
    result.r = (float)(hex_digit(digits[0]) * 16 + hex_digit(digits[1])) / 255.0f;
    result.g = (float)(hex_digit(digits[2]) * 16 + hex_digit(digits[3])) / 255.0f;
    result.b = (float)(hex_digit(digits[4]) * 16 + hex_digit(digits[5])) / 255.0f;
    if (length == 8) {
        result.a = (float)(hex_digit(digits[6]) * 16 + hex_digit(digits[7])) / 255.0f;
    }
    return result;
}

static void apply_style_preset(OmniConfig *config)
{
    if (config->style == OMNI_STYLE_COMPACT) {
        config->width = 0.82f;
        config->height = 0.48f;
        config->window_padding = 9.0f;
        config->key_gap = 3.0f;
        config->row_gap = 3.0f;
        config->window_rounding = 7.0f;
        config->key_rounding = 3.0f;
    } else if (config->style == OMNI_STYLE_HIGH_CONTRAST) {
        config->panel_color = (OmniColor){0.02f, 0.025f, 0.04f, 0.98f};
        config->key_color = (OmniColor){0.16f, 0.30f, 0.52f, 1.0f};
        config->focus_color = (OmniColor){0.24f, 0.50f, 0.85f, 1.0f};
        config->border_color = (OmniColor){0.70f, 0.85f, 1.0f, 1.0f};
        config->text_color = (OmniColor){1.0f, 1.0f, 1.0f, 1.0f};
        config->cancel_color = (OmniColor){0.48f, 0.16f, 0.18f, 1.0f};
        config->submit_color = (OmniColor){0.10f, 0.45f, 0.28f, 1.0f};
        config->close_color = (OmniColor){0.24f, 0.30f, 0.36f, 1.0f};
        config->window_rounding = 8.0f;
        config->key_rounding = 4.0f;
    }
}

static size_t parse_size(const char *value, size_t fallback, const char *name)
{
    char *end = NULL;
    unsigned long parsed;
    errno = 0;
    if (value[0] == '\0') {
        return fallback;
    }
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0 || parsed > OMNI_MAX_BUFFER_LIMIT) {
        omni_diag(OMNI_LOG_WARN, "%s must be between 1 and %d; using default", name, OMNI_MAX_BUFFER_LIMIT);
        return fallback;
    }
    return (size_t)parsed;
}

static size_t parse_count(const char *value, size_t fallback, size_t maximum, const char *name)
{
    char *end = NULL;
    unsigned long parsed;
    errno = 0;
    if (value[0] == '\0') {
        return fallback;
    }
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > maximum) {
        omni_diag(OMNI_LOG_WARN, "%s must be between 0 and %zu; using default", name, maximum);
        return fallback;
    }
    return (size_t)parsed;
}

static SDL_Keycode parse_key(const char *value, SDL_Keycode fallback, const char *name)
{
    size_t i;
    SDL_Keycode parsed;
    if (value[0] == '\0') {
        return fallback;
    }
    parsed = SDL_GetKeyFromName(value);
    if (parsed != SDLK_UNKNOWN) {
        return parsed;
    }
    for (i = 0; i < sizeof(key_names) / sizeof(key_names[0]); ++i) {
        if (equals(value, key_names[i].name)) {
            return key_names[i].key;
        }
    }
    omni_diag(OMNI_LOG_WARN, "%s has unknown key '%s'; using default", name, value);
    return fallback;
}

static void parse_choice(const char *value, const char *name, const char *const *choices,
                         size_t count, size_t fallback, size_t *result)
{
    size_t i;
    if (value[0] == '\0') {
        *result = fallback;
        return;
    }
    for (i = 0; i < count; ++i) {
        if (equals(value, choices[i])) {
            *result = i;
            return;
        }
    }
    omni_diag(OMNI_LOG_WARN, "%s has unknown value '%s'; using default", name, value);
    *result = fallback;
}

static void parse_charsets(OmniConfig *config, const char *value)
{
    char copy[OMNI_MAX_PAGES * OMNI_MAX_PAGE_SPEC];
    char *cursor;
    char *token;
    size_t count = 0;
    if (value[0] == '\0') {
        return;
    }
    strncpy(copy, value, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    cursor = copy;
    while ((token = strsep(&cursor, ";")) != NULL && count < OMNI_MAX_PAGES) {
        if (token[0] == '\0' || strlen(token) >= OMNI_MAX_PAGE_SPEC) {
            omni_diag(OMNI_LOG_WARN, "OMNI_CHARSETS contains an empty or oversized page; ignoring it");
            continue;
        }
        strncpy(config->charset_specs[count], token, OMNI_MAX_PAGE_SPEC - 1);
        config->charset_specs[count][OMNI_MAX_PAGE_SPEC - 1] = '\0';
        ++count;
    }
    if (count == 0) {
        omni_diag(OMNI_LOG_WARN, "OMNI_CHARSETS has no usable pages; using defaults");
        return;
    }
    config->charset_count = count;
}

void omni_config_defaults(OmniConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->input_backend = OMNI_INPUT_SDL;
    config->input_fallback = 1;
    strncpy(config->evdev_device, "auto", sizeof(config->evdev_device) - 1);
    config->toggle_key = SDLK_F12;
    config->up_key = SDLK_UP;
    config->down_key = SDLK_DOWN;
    config->left_key = SDLK_LEFT;
    config->right_key = SDLK_RIGHT;
    config->confirm_key = SDLK_RETURN;
    config->backspace_key = SDLK_BACKSPACE;
    config->charset_key = SDLK_TAB;
    config->mode = OMNI_MODE_BUFFERED;
    config->event_mode = OMNI_EVENT_AUTO;
    config->buffer_dismiss = OMNI_DISMISS_CANCEL;
    config->buffer_limit = OMNI_DEFAULT_BUFFER_LIMIT;
    config->open_backspace_count = 0;
    config->emit_return = 1;
    strcpy(config->charset_specs[0], "lower");
    strcpy(config->charset_specs[1], "upper");
    strcpy(config->charset_specs[2], "number");
    strcpy(config->charset_specs[3], "special");
    config->charset_count = 4;
    config->anchor = OMNI_ANCHOR_BOTTOM_CENTER;
    config->width = 0.90f;
    config->height = 0.55f;
    config->opacity = 0.92f;
    config->style = OMNI_STYLE_DEFAULT;
    config->panel_color = (OmniColor){0.12f, 0.15f, 0.18f, 0.98f};
    config->key_color = (OmniColor){0.22f, 0.27f, 0.31f, 1.0f};
    config->focus_color = (OmniColor){0.33f, 0.48f, 0.62f, 1.0f};
    config->border_color = (OmniColor){0.42f, 0.50f, 0.55f, 1.0f};
    config->text_color = (OmniColor){0.93f, 0.96f, 0.97f, 1.0f};
    config->cancel_color = (OmniColor){0.37f, 0.28f, 0.20f, 1.0f};
    config->submit_color = (OmniColor){0.18f, 0.38f, 0.28f, 1.0f};
    config->close_color = (OmniColor){0.24f, 0.29f, 0.33f, 1.0f};
    config->window_rounding = 10.0f;
    config->key_rounding = 4.0f;
    config->key_gap = 5.0f;
    config->row_gap = 5.0f;
    config->window_padding = 14.0f;
    config->font_path[0] = '\0';
    config->font_size = 0.0f;
    config->log_level = OMNI_LOG_WARN;
}

int omni_config_load(OmniConfig *config)
{
    static const char *const backends[] = {"sdl", "evdev", "auto"};
    static const char *const modes[] = {"buffered", "instant"};
    static const char *const events[] = {"auto", "text", "key"};
    static const char *const dismiss[] = {"stay", "submit", "cancel"};
    static const char *const anchors[] = {"top-left", "top-center", "top-right", "center-left", "center", "center-right", "bottom-left", "bottom-center", "bottom-right"};
    static const char *const styles[] = {"default", "compact", "high-contrast"};
    static const char *const logs[] = {"error", "warn", "info", "debug"};
    size_t choice;
    omni_config_defaults(config);
    parse_choice(env_value("OMNI_INPUT_BACKEND"), "OMNI_INPUT_BACKEND", backends, 3, 0, &choice);
    config->input_backend = (OmniInputBackend)choice;
    config->input_fallback = parse_bool(env_value("OMNI_INPUT_FALLBACK"), 1, "OMNI_INPUT_FALLBACK");
    if (env_value("OMNI_EVDEV_DEVICE")[0] != '\0') {
        strncpy(config->evdev_device, env_value("OMNI_EVDEV_DEVICE"), sizeof(config->evdev_device) - 1);
        config->evdev_device[sizeof(config->evdev_device) - 1] = '\0';
    }
    config->toggle_key = parse_key(env_value("OMNI_TOGGLE_KEY"), config->toggle_key, "OMNI_TOGGLE_KEY");
    config->up_key = parse_key(env_value("OMNI_UP_KEY"), config->up_key, "OMNI_UP_KEY");
    config->down_key = parse_key(env_value("OMNI_DOWN_KEY"), config->down_key, "OMNI_DOWN_KEY");
    config->left_key = parse_key(env_value("OMNI_LEFT_KEY"), config->left_key, "OMNI_LEFT_KEY");
    config->right_key = parse_key(env_value("OMNI_RIGHT_KEY"), config->right_key, "OMNI_RIGHT_KEY");
    config->confirm_key = parse_key(env_value("OMNI_CONFIRM_KEY"), config->confirm_key, "OMNI_CONFIRM_KEY");
    config->backspace_key = parse_key(env_value("OMNI_BACKSPACE_KEY"), config->backspace_key, "OMNI_BACKSPACE_KEY");
    config->charset_key = parse_key(env_value("OMNI_CHARSET_KEY"), config->charset_key, "OMNI_CHARSET_KEY");
    parse_choice(env_value("OMNI_MODE"), "OMNI_MODE", modes, 2, 0, &choice);
    config->mode = (OmniMode)choice;
    parse_choice(env_value("OMNI_EVENT_MODE"), "OMNI_EVENT_MODE", events, 3, 0, &choice);
    config->event_mode = (OmniEventMode)choice;
    parse_choice(env_value("OMNI_BUFFER_DISMISS"), "OMNI_BUFFER_DISMISS", dismiss, 3, OMNI_DISMISS_CANCEL, &choice);
    config->buffer_dismiss = (OmniDismissMode)choice;
    config->buffer_limit = parse_size(env_value("OMNI_BUFFER_LIMIT"), config->buffer_limit, "OMNI_BUFFER_LIMIT");
    config->open_backspace_count = parse_count(env_value("OMNI_OPEN_BACKSPACE_COUNT"), config->open_backspace_count,
                                               OMNI_MAX_OPEN_BACKSPACE_COUNT, "OMNI_OPEN_BACKSPACE_COUNT");
    config->emit_return = parse_bool(env_value("OMNI_EMIT_RETURN"), config->emit_return, "OMNI_EMIT_RETURN");
    parse_charsets(config, env_value("OMNI_CHARSETS"));
    parse_choice(env_value("OMNI_STYLE"), "OMNI_STYLE", styles, 3, 0, &choice);
    config->style = (OmniStyle)choice;
    apply_style_preset(config);
    parse_choice(env_value("OMNI_ANCHOR"), "OMNI_ANCHOR", anchors, 9, OMNI_ANCHOR_BOTTOM_CENTER, &choice);
    config->anchor = (OmniAnchor)choice;
    config->width = parse_float(env_value("OMNI_WIDTH"), config->width, 0.05f, 1.0f, "OMNI_WIDTH");
    config->height = parse_float(env_value("OMNI_HEIGHT"), config->height, 0.05f, 1.0f, "OMNI_HEIGHT");
    config->x = parse_float(env_value("OMNI_X"), 0.0f, -1.0f, 1.0f, "OMNI_X");
    config->y = parse_float(env_value("OMNI_Y"), 0.0f, -1.0f, 1.0f, "OMNI_Y");
    config->opacity = parse_float(env_value("OMNI_OPACITY"), config->opacity, 0.0f, 1.0f, "OMNI_OPACITY");
    config->panel_color = parse_color(env_value("OMNI_PANEL_COLOR"), config->panel_color, "OMNI_PANEL_COLOR");
    config->key_color = parse_color(env_value("OMNI_KEY_COLOR"), config->key_color, "OMNI_KEY_COLOR");
    config->focus_color = parse_color(env_value("OMNI_FOCUS_COLOR"), config->focus_color, "OMNI_FOCUS_COLOR");
    config->border_color = parse_color(env_value("OMNI_BORDER_COLOR"), config->border_color, "OMNI_BORDER_COLOR");
    config->text_color = parse_color(env_value("OMNI_TEXT_COLOR"), config->text_color, "OMNI_TEXT_COLOR");
    config->cancel_color = parse_color(env_value("OMNI_CANCEL_COLOR"), config->cancel_color, "OMNI_CANCEL_COLOR");
    config->submit_color = parse_color(env_value("OMNI_SUBMIT_COLOR"), config->submit_color, "OMNI_SUBMIT_COLOR");
    config->close_color = parse_color(env_value("OMNI_CLOSE_COLOR"), config->close_color, "OMNI_CLOSE_COLOR");
    config->window_rounding = parse_float(env_value("OMNI_WINDOW_ROUNDING"), config->window_rounding, 0.0f, 32.0f, "OMNI_WINDOW_ROUNDING");
    config->key_rounding = parse_float(env_value("OMNI_KEY_ROUNDING"), config->key_rounding, 0.0f, 24.0f, "OMNI_KEY_ROUNDING");
    config->key_gap = parse_float(env_value("OMNI_KEY_GAP"), config->key_gap, 0.0f, 32.0f, "OMNI_KEY_GAP");
    config->row_gap = parse_float(env_value("OMNI_ROW_GAP"), config->row_gap, 0.0f, 32.0f, "OMNI_ROW_GAP");
    config->window_padding = parse_float(env_value("OMNI_WINDOW_PADDING"), config->window_padding, 0.0f, 64.0f, "OMNI_WINDOW_PADDING");
    if (env_value("OMNI_FONT")[0] != '\0') {
        strncpy(config->font_path, env_value("OMNI_FONT"), sizeof(config->font_path) - 1);
        config->font_path[sizeof(config->font_path) - 1] = '\0';
    }
    config->font_size = parse_float(env_value("OMNI_FONT_SIZE"), config->font_size, 8.0f, 64.0f, "OMNI_FONT_SIZE");
    parse_choice(env_value("OMNI_LOG"), "OMNI_LOG", logs, 4, OMNI_LOG_WARN, &choice);
    config->log_level = (OmniLogLevel)choice;
    omni_diag_set_level(config->log_level);

    {
        struct Control {
            SDL_Keycode *key;
            SDL_Keycode fallback;
            const char *name;
        } controls[] = {
            {&config->toggle_key, SDLK_F12, "OMNI_TOGGLE_KEY"},
            {&config->up_key, SDLK_UP, "OMNI_UP_KEY"},
            {&config->down_key, SDLK_DOWN, "OMNI_DOWN_KEY"},
            {&config->left_key, SDLK_LEFT, "OMNI_LEFT_KEY"},
            {&config->right_key, SDLK_RIGHT, "OMNI_RIGHT_KEY"},
            {&config->confirm_key, SDLK_RETURN, "OMNI_CONFIRM_KEY"},
            {&config->backspace_key, SDLK_BACKSPACE, "OMNI_BACKSPACE_KEY"},
            {&config->charset_key, SDLK_TAB, "OMNI_CHARSET_KEY"}
        };
        size_t i;
        size_t j;
        size_t k;
        for (i = 1; i < sizeof(controls) / sizeof(controls[0]); ++i) {
            if (*controls[0].key == *controls[i].key) {
                omni_diag(OMNI_LOG_WARN, "%s duplicates %s; using F12 for the reserved toggle", controls[0].name, controls[i].name);
                *controls[0].key = SDLK_F12;
                break;
            }
        }
        for (i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
            for (j = 0; j < i; ++j) {
                if (i == 0) {
                    continue;
                }
                if (*controls[i].key == *controls[j].key) {
                    SDL_Keycode candidate = controls[i].fallback;
                    int available = 1;
                    for (k = 0; k < i; ++k) {
                        if (*controls[k].key == candidate) {
                            available = 0;
                            break;
                        }
                    }
                    if (available) {
                        *controls[i].key = candidate;
                    } else {
                        candidate = controls[j].fallback;
                        available = 1;
                        for (k = 0; k <= i; ++k) {
                            if (k != j && *controls[k].key == candidate) {
                                available = 0;
                                break;
                            }
                        }
                        if (available) {
                            *controls[j].key = candidate;
                        } else {
                            *controls[i].key = SDLK_UNKNOWN;
                        }
                    }
                    omni_diag(OMNI_LOG_WARN, "%s duplicates %s; using its default", controls[i].name, controls[j].name);
                    break;
                }
            }
        }
    }
    return 0;
}

const char *omni_config_key_name(SDL_Keycode key)
{
    size_t i;
    for (i = 0; i < sizeof(key_names) / sizeof(key_names[0]); ++i) {
        if (key_names[i].key == key) {
            return key_names[i].name;
        }
    }
    return SDL_GetKeyName(key);
}
