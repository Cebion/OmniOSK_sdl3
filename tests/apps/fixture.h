#ifndef SDL_OSK_FIXTURE_H
#define SDL_OSK_FIXTURE_H

#include <SDL.h>

#include <stddef.h>

typedef struct FixtureOptions {
    const char *trace;
    const char *events;
    const char *screenshot;
    unsigned int duration_ms;
} FixtureOptions;

typedef struct FixtureMonitor {
    SDL_Window *window;
    const char *backend;
    const char *script;
    char base_title[128];
    unsigned int key_down_count;
    unsigned int key_up_count;
    unsigned int text_count;
    char last_event[96];
    char last_text[64];
} FixtureMonitor;

int fixture_parse_options(int argc, char **argv, FixtureOptions *options);
const char *fixture_window_title(const char *base_title);
void fixture_push_script(const char *script);
void fixture_monitor_init(FixtureMonitor *monitor, SDL_Window *window, const char *backend, const char *script);
void fixture_monitor_event(FixtureMonitor *monitor, const SDL_Event *event);
void fixture_monitor_summary(const FixtureMonitor *monitor);
void fixture_log_event(const char *path, const SDL_Event *event);
void fixture_log_frame(const char *path, int present_count, int width, int height);
void fixture_log_sentinel(const char *path, unsigned int first, unsigned int second);
void fixture_log_state(const char *path, const char *backend, int state_ok);
int fixture_write_gl_screenshot(const char *path, int width, int height);
int fixture_write_renderer_screenshot(const char *path, SDL_Renderer *renderer, int width, int height);

#endif
