#ifndef OMNI_CONFIG_H
#define OMNI_CONFIG_H

#include <SDL.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OMNI_MAX_PAGES 16
#define OMNI_MAX_PAGE_SPEC 128
#define OMNI_DEFAULT_BUFFER_LIMIT 256
#define OMNI_MAX_BUFFER_LIMIT 4096
#define OMNI_MAX_OPEN_BACKSPACE_COUNT 256

typedef enum OmniInputBackend {
    OMNI_INPUT_SDL,
    OMNI_INPUT_EVDEV,
    OMNI_INPUT_AUTO
} OmniInputBackend;

typedef enum OmniMode {
    OMNI_MODE_BUFFERED,
    OMNI_MODE_INSTANT
} OmniMode;

typedef enum OmniEventMode {
    OMNI_EVENT_AUTO,
    OMNI_EVENT_TEXT,
    OMNI_EVENT_KEY
} OmniEventMode;

typedef enum OmniDismissMode {
    OMNI_DISMISS_STAY,
    OMNI_DISMISS_SUBMIT,
    OMNI_DISMISS_CANCEL
} OmniDismissMode;

typedef enum OmniAnchor {
    OMNI_ANCHOR_TOP_LEFT,
    OMNI_ANCHOR_TOP_CENTER,
    OMNI_ANCHOR_TOP_RIGHT,
    OMNI_ANCHOR_CENTER_LEFT,
    OMNI_ANCHOR_CENTER,
    OMNI_ANCHOR_CENTER_RIGHT,
    OMNI_ANCHOR_BOTTOM_LEFT,
    OMNI_ANCHOR_BOTTOM_CENTER,
    OMNI_ANCHOR_BOTTOM_RIGHT
} OmniAnchor;

typedef enum OmniLogLevel {
    OMNI_LOG_ERROR,
    OMNI_LOG_WARN,
    OMNI_LOG_INFO,
    OMNI_LOG_DEBUG
} OmniLogLevel;

typedef enum OmniStyle {
    OMNI_STYLE_DEFAULT,
    OMNI_STYLE_COMPACT,
    OMNI_STYLE_HIGH_CONTRAST
} OmniStyle;

typedef struct OmniColor {
    float r;
    float g;
    float b;
    float a;
} OmniColor;

typedef struct OmniConfig {
    OmniInputBackend input_backend;
    int input_fallback;
    char evdev_device[256];
    SDL_Keycode toggle_key;
    SDL_Keycode up_key;
    SDL_Keycode down_key;
    SDL_Keycode left_key;
    SDL_Keycode right_key;
    SDL_Keycode confirm_key;
    SDL_Keycode backspace_key;
    SDL_Keycode charset_key;
    OmniMode mode;
    OmniEventMode event_mode;
    OmniDismissMode buffer_dismiss;
    size_t buffer_limit;
    size_t open_backspace_count;
    int emit_return;
    char charset_specs[OMNI_MAX_PAGES][OMNI_MAX_PAGE_SPEC];
    size_t charset_count;
    OmniAnchor anchor;
    float width;
    float height;
    float x;
    float y;
    float opacity;
    OmniStyle style;
    OmniColor panel_color;
    OmniColor key_color;
    OmniColor focus_color;
    OmniColor border_color;
    OmniColor text_color;
    OmniColor cancel_color;
    OmniColor submit_color;
    OmniColor close_color;
    float window_rounding;
    float key_rounding;
    float key_gap;
    float row_gap;
    float window_padding;
    char font_path[512];
    float font_size;
    OmniLogLevel log_level;
} OmniConfig;

void omni_config_defaults(OmniConfig *config);
int omni_config_load(OmniConfig *config);
const char *omni_config_key_name(SDL_Keycode key);

#ifdef __cplusplus
}
#endif

#endif
