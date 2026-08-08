#include "fixture.h"

#include <GLES2/gl2.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fixture_monitor_refresh(const FixtureMonitor *monitor)
{
    char title[256];
    snprintf(title, sizeof(title), "%.80s | audit app keydown=%u keyup=%u text=%u | last=%.48s | script=%.48s",
             monitor->base_title, monitor->key_down_count, monitor->key_up_count, monitor->text_count,
             monitor->last_event[0] == '\0' ? "none" : monitor->last_event,
             monitor->script[0] == '\0' ? "manual" : monitor->script);
    SDL_SetWindowTitle(monitor->window, title);
}

void fixture_monitor_init(FixtureMonitor *monitor, SDL_Window *window, const char *backend, const char *script)
{
    memset(monitor, 0, sizeof(*monitor));
    monitor->window = window;
    monitor->backend = backend;
    monitor->script = script == NULL ? "" : script;
    snprintf(monitor->base_title, sizeof(monitor->base_title), "%s", SDL_GetWindowTitle(window));
    snprintf(monitor->last_event, sizeof(monitor->last_event), "none");
    fixture_monitor_refresh(monitor);
    printf("APP_AUDIT_READY backend=%s script=%s\n", backend, monitor->script[0] == '\0' ? "manual" : monitor->script);
    fflush(stdout);
}

void fixture_monitor_event(FixtureMonitor *monitor, const SDL_Event *event)
{
    const char *key_name;
    if (event->type == SDL_KEYDOWN) {
        key_name = SDL_GetKeyName(event->key.keysym.sym);
        ++monitor->key_down_count;
        snprintf(monitor->last_event, sizeof(monitor->last_event), "KEYDOWN %s", key_name);
        printf("APP_RECEIVED backend=%s type=KEYDOWN key=%s repeat=%u counts=%u/%u/%u\n",
               monitor->backend, key_name, event->key.repeat, monitor->key_down_count,
               monitor->key_up_count, monitor->text_count);
    } else if (event->type == SDL_KEYUP) {
        key_name = SDL_GetKeyName(event->key.keysym.sym);
        ++monitor->key_up_count;
        snprintf(monitor->last_event, sizeof(monitor->last_event), "KEYUP %s", key_name);
        printf("APP_RECEIVED backend=%s type=KEYUP key=%s counts=%u/%u/%u\n",
               monitor->backend, key_name, monitor->key_down_count,
               monitor->key_up_count, monitor->text_count);
    } else if (event->type == SDL_TEXTINPUT) {
        ++monitor->text_count;
        snprintf(monitor->last_text, sizeof(monitor->last_text), "%.63s", event->text.text);
        snprintf(monitor->last_event, sizeof(monitor->last_event), "TEXTINPUT %.80s", event->text.text);
        printf("APP_RECEIVED backend=%s type=TEXTINPUT text=%s counts=%u/%u/%u\n",
               monitor->backend, event->text.text, monitor->key_down_count,
               monitor->key_up_count, monitor->text_count);
    } else {
        return;
    }
    fixture_monitor_refresh(monitor);
    fflush(stdout);
}

void fixture_monitor_summary(const FixtureMonitor *monitor)
{
    printf("APP_AUDIT_SUMMARY backend=%s keydown=%u keyup=%u text=%u last=%s last_text=%s\n",
           monitor->backend, monitor->key_down_count, monitor->key_up_count,
           monitor->text_count, monitor->last_event[0] == '\0' ? "none" : monitor->last_event,
           monitor->last_text[0] == '\0' ? "none" : monitor->last_text);
    fflush(stdout);
}

static void write_line(const char *path, const char *format, ...)
{
    va_list arguments;
    FILE *file;
    if (path == NULL) {
        return;
    }
    file = fopen(path, "a");
    if (file == NULL) {
        return;
    }
    va_start(arguments, format);
    vfprintf(file, format, arguments);
    va_end(arguments);
    fputc('\n', file);
    fclose(file);
}

static SDL_Keycode key_for_token(const char *token)
{
    if (strcmp(token, "toggle") == 0) return SDLK_F12;
    if (strcmp(token, "up") == 0) return SDLK_UP;
    if (strcmp(token, "down") == 0) return SDLK_DOWN;
    if (strcmp(token, "left") == 0) return SDLK_LEFT;
    if (strcmp(token, "right") == 0) return SDLK_RIGHT;
    if (strcmp(token, "confirm") == 0) return SDLK_RETURN;
    if (strcmp(token, "game") == 0) return SDLK_g;
    if (strcmp(token, "backspace") == 0) return SDLK_BACKSPACE;
    if (strcmp(token, "tab") == 0) return SDLK_TAB;
    return SDLK_UNKNOWN;
}

static void push_key(SDL_Keycode key)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.keysym.sym = key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    SDL_PushEvent(&event);
    event.type = SDL_KEYUP;
    event.key.state = SDL_RELEASED;
    SDL_PushEvent(&event);
}

int fixture_parse_options(int argc, char **argv, FixtureOptions *options)
{
    int index;
    options->trace = NULL;
    options->events = "";
    options->screenshot = NULL;
    options->duration_ms = 0;
    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--trace") == 0 && index + 1 < argc) {
            options->trace = argv[++index];
        } else if (strcmp(argv[index], "--events") == 0 && index + 1 < argc) {
            options->events = argv[++index];
        } else if (strcmp(argv[index], "--screenshot") == 0 && index + 1 < argc) {
            options->screenshot = argv[++index];
        } else if (strcmp(argv[index], "--duration-ms") == 0 && index + 1 < argc) {
            options->duration_ms = (unsigned int)strtoul(argv[++index], NULL, 10);
        } else {
            return -1;
        }
    }
    return 0;
}

const char *fixture_window_title(const char *base_title)
{
    static char title[256];
    const char *suffix = getenv("OMNI_WINDOW_TITLE_SUFFIX");
    if (suffix == NULL || suffix[0] == '\0') {
        return base_title;
    }
    snprintf(title, sizeof(title), "%s %s", base_title, suffix);
    return title;
}

void fixture_push_script(const char *script)
{
    char copy[512];
    char *token;
    char *cursor;
    strncpy(copy, script == NULL ? "" : script, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';
    cursor = copy;
    token = strtok(cursor, ",");
    while (token != NULL) {
        SDL_Keycode key = key_for_token(token);
        if (strcmp(token, "submit") == 0 || strcmp(token, "cancel") == 0) {
            push_key(SDLK_DOWN);
            push_key(SDLK_DOWN);
            if (strcmp(token, "cancel") == 0) {
                push_key(SDLK_RETURN);
            } else {
                push_key(SDLK_RIGHT);
                push_key(SDLK_RIGHT);
                push_key(SDLK_RETURN);
            }
        } else if (key != SDLK_UNKNOWN) {
            push_key(key);
        }
        token = strtok(NULL, ",");
    }
}

void fixture_log_event(const char *path, const SDL_Event *event)
{
    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        write_line(path, "event type=%u key=%s state=%u repeat=%u", event->type,
                   SDL_GetKeyName(event->key.keysym.sym), event->key.state, event->key.repeat);
    } else if (event->type == SDL_TEXTINPUT) {
        write_line(path, "event type=%u text=%s", event->type, event->text.text);
    } else {
        write_line(path, "event type=%u", event->type);
    }
}

void fixture_log_frame(const char *path, int present_count, int width, int height)
{
    write_line(path, "frame present=%d size=%dx%d", present_count, width, height);
}

void fixture_log_sentinel(const char *path, unsigned int first, unsigned int second)
{
    write_line(path, "sentinel first=%u second=%u", first, second);
}

void fixture_log_state(const char *path, const char *backend, int state_ok)
{
    write_line(path, "state backend=%s restored=%d", backend, state_ok);
}

static int write_ppm(const char *path, const unsigned char *pixels, int width, int height, int bottom_up)
{
    FILE *file = fopen(path, "wb");
    int row;
    if (file == NULL) {
        return -1;
    }
    fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (row = height - 1; row >= 0; --row) {
        int source_row = bottom_up ? row : height - row - 1;
        fwrite(pixels + (size_t)source_row * (size_t)width * 3U, 1, (size_t)width * 3U, file);
    }
    fclose(file);
    return 0;
}

int fixture_write_gl_screenshot(const char *path, int width, int height)
{
    unsigned char *pixels = (unsigned char *)malloc((size_t)width * (size_t)height * 3U);
    int result;
    if (pixels == NULL) {
        return -1;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    result = write_ppm(path, pixels, width, height, 1);
    free(pixels);
    return result;
}

int fixture_write_renderer_screenshot(const char *path, SDL_Renderer *renderer, int width, int height)
{
    unsigned char *pixels = (unsigned char *)malloc((size_t)width * (size_t)height * 3U);
    int result;
    if (pixels == NULL) {
        return -1;
    }
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGB24, pixels, width * 3) != 0) {
        free(pixels);
        return -1;
    }
    result = write_ppm(path, pixels, width, height, 0);
    free(pixels);
    return result;
}
