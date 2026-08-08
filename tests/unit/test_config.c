#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); return 1; } } while (0)

int main(void)
{
    OmniConfig config;
    unsetenv("OMNI_INPUT_BACKEND");
    unsetenv("OMNI_MODE");
    unsetenv("OMNI_CHARSETS");
    unsetenv("OMNI_BUFFER_DISMISS");
    unsetenv("OMNI_STYLE");
    unsetenv("OMNI_CANCEL_COLOR");
    unsetenv("OMNI_SUBMIT_COLOR");
    unsetenv("OMNI_CLOSE_COLOR");
    unsetenv("OMNI_PANEL_COLOR");
    unsetenv("OMNI_KEY_COLOR");
    unsetenv("OMNI_FOCUS_COLOR");
    unsetenv("OMNI_BORDER_COLOR");
    unsetenv("OMNI_TEXT_COLOR");
    unsetenv("OMNI_KEY_GAP");
    unsetenv("OMNI_ROW_GAP");
    unsetenv("OMNI_WINDOW_ROUNDING");
    unsetenv("OMNI_KEY_ROUNDING");
    unsetenv("OMNI_WINDOW_PADDING");
    unsetenv("OMNI_FONT");
    unsetenv("OMNI_FONT_SIZE");
    unsetenv("OMNI_OPEN_BACKSPACE_COUNT");
    unsetenv("OMNI_EMIT_RETURN");
    omni_config_load(&config);
    CHECK(config.input_backend == OMNI_INPUT_SDL);
    CHECK(config.mode == OMNI_MODE_BUFFERED);
    CHECK(config.buffer_dismiss == OMNI_DISMISS_CANCEL);
    CHECK(config.toggle_key == SDLK_F12);
    CHECK(config.buffer_limit == 256);
    CHECK(config.charset_count == 4);
    CHECK(config.height > 0.54f && config.height < 0.56f);
    CHECK(config.style == OMNI_STYLE_DEFAULT);
    CHECK(config.key_gap > 4.9f && config.key_gap < 5.1f);
    CHECK(config.open_backspace_count == 0);
    CHECK(config.emit_return == 1);
    setenv("OMNI_INPUT_BACKEND", "evdev", 1);
    setenv("OMNI_MODE", "instant", 1);
    setenv("OMNI_BUFFER_LIMIT", "4096", 1);
    setenv("OMNI_CHARSETS", "lower+number;special", 1);
    setenv("OMNI_ANCHOR", "center", 1);
    setenv("OMNI_OPACITY", "0.5", 1);
    setenv("OMNI_STYLE", "high-contrast", 1);
    setenv("OMNI_CANCEL_COLOR", "112233", 1);
    setenv("OMNI_SUBMIT_COLOR", "#44556680", 1);
    setenv("OMNI_KEY_GAP", "3.5", 1);
    setenv("OMNI_HEIGHT", "0.73", 1);
    setenv("OMNI_KEY_ROUNDING", "12", 1);
    setenv("OMNI_ROW_GAP", "17", 1);
    omni_config_load(&config);
    CHECK(config.input_backend == OMNI_INPUT_EVDEV);
    CHECK(config.mode == OMNI_MODE_INSTANT);
    CHECK(config.buffer_limit == 4096);
    CHECK(config.charset_count == 2);
    CHECK(strcmp(config.charset_specs[0], "lower+number") == 0);
    CHECK(config.anchor == OMNI_ANCHOR_CENTER);
    CHECK(config.opacity > 0.49f && config.opacity < 0.51f);
    CHECK(config.style == OMNI_STYLE_HIGH_CONTRAST);
    CHECK(config.cancel_color.r > 0.06f && config.cancel_color.r < 0.07f);
    CHECK(config.cancel_color.g > 0.13f && config.cancel_color.g < 0.14f);
    CHECK(config.submit_color.a > 0.49f && config.submit_color.a < 0.51f);
    CHECK(config.key_gap > 3.49f && config.key_gap < 3.51f);
    CHECK(config.height > 0.72f && config.height < 0.74f);
    CHECK(config.key_rounding > 11.9f && config.key_rounding < 12.1f);
    CHECK(config.row_gap > 16.9f && config.row_gap < 17.1f);
    setenv("OMNI_BUFFER_LIMIT", "not-a-number", 1);
    setenv("OMNI_TOGGLE_KEY", "UP", 1);
    setenv("OMNI_WIDTH", "nan", 1);
    omni_config_load(&config);
    CHECK(config.buffer_limit == 256);
    CHECK(config.toggle_key == SDLK_F12);
    CHECK(config.width > 0.89f && config.width < 0.91f);
    unsetenv("OMNI_KEY_GAP");
    unsetenv("OMNI_HEIGHT");
    unsetenv("OMNI_KEY_ROUNDING");
    unsetenv("OMNI_ROW_GAP");
    setenv("OMNI_STYLE", "compact", 1);
    omni_config_load(&config);
    CHECK(config.style == OMNI_STYLE_COMPACT);
    CHECK(config.width > 0.81f && config.width < 0.83f);
    CHECK(config.height > 0.47f && config.height < 0.49f);
    CHECK(config.key_gap > 2.9f && config.key_gap < 3.1f);
    setenv("OMNI_FONT", "/tmp/test-font.ttf", 1);
    setenv("OMNI_FONT_SIZE", "21", 1);
    omni_config_load(&config);
    CHECK(strcmp(config.font_path, "/tmp/test-font.ttf") == 0);
    CHECK(config.font_size > 20.9f && config.font_size < 21.1f);
    setenv("OMNI_OPEN_BACKSPACE_COUNT", "2", 1);
    setenv("OMNI_EMIT_RETURN", "0", 1);
    omni_config_load(&config);
    CHECK(config.open_backspace_count == 2);
    CHECK(config.emit_return == 0);
    setenv("OMNI_OPEN_BACKSPACE_COUNT", "256", 1);
    omni_config_load(&config);
    CHECK(config.open_backspace_count == OMNI_MAX_OPEN_BACKSPACE_COUNT);
    unsetenv("OMNI_TOGGLE_KEY");
    setenv("OMNI_UP_KEY", "DOWN", 1);
    setenv("OMNI_DOWN_KEY", "DOWN", 1);
    omni_config_load(&config);
    CHECK(config.up_key == SDLK_UP);
    CHECK(config.down_key == SDLK_DOWN);
    setenv("OMNI_UP_KEY", "UP", 1);
    setenv("OMNI_DOWN_KEY", "LEFT", 1);
    setenv("OMNI_LEFT_KEY", "UP", 1);
    omni_config_load(&config);
    CHECK(config.up_key != config.down_key);
    CHECK(config.up_key != config.left_key);
    CHECK(config.down_key != config.left_key);
    return 0;
}
