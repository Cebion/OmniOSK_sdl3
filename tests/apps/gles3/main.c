#include "../fixture.h"

#include <SDL.h>
#include <GLES2/gl2.h>

#include <math.h>
#include <stdio.h>

static int state_restored(int width, int height)
{
    GLint viewport[4];
    GLint scissor[4];
    GLint program;
    GLboolean scissor_enabled;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
    return viewport[0] == 3 && viewport[1] == 5 && viewport[2] == width - 6 && viewport[3] == height - 8 &&
           scissor[0] == 7 && scissor[1] == 9 && scissor[2] == width - 14 && scissor[3] == height - 16 &&
           program == 0 && scissor_enabled == GL_TRUE;
}

int main(int argc, char **argv)
{
    FixtureOptions options;
    SDL_Window *window;
    SDL_GLContext context;
    SDL_Event event;
    Uint32 start;
    int width = 0;
    int height = 0;
    int resized = 0;
    int quit = 0;
    int present_count = 0;
    float phase = 0.0f;
    int x = 0;
    int direction = 1;
    FixtureMonitor monitor;
    if (fixture_parse_options(argc, argv, &options) != 0) {
        fprintf(stderr, "usage: %s [--trace PATH] [--events SCRIPT] [--duration-ms N] [--screenshot PATH]\n", argv[0]);
        return 2;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "gles3: SDL_Init failed: %s\n", SDL_GetError());
        return 77;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    window = SDL_CreateWindow(fixture_window_title("Omni OSK GLES sample"), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              960, 540, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        fprintf(stderr, "gles3: OpenGL window unavailable: %s\n", SDL_GetError());
        SDL_Quit();
        return 77;
    }
    context = SDL_GL_CreateContext(window);
    if (context == NULL) {
        fprintf(stderr, "gles3: OpenGL context unavailable: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 77;
    }
    SDL_GL_SetSwapInterval(1);
    fixture_monitor_init(&monitor, window, "gl", options.events);
    fixture_push_script(options.events);
    start = SDL_GetTicks();
    while (options.duration_ms == 0 || SDL_GetTicks() - start < options.duration_ms) {
        while (SDL_PollEvent(&event) != 0) {
            fixture_log_event(options.trace, &event);
            fixture_monitor_event(&monitor, &event);
            if (event.type == SDL_QUIT) {
                quit = 1;
            }
        }
        if (quit) {
            break;
        }
        if (!resized && options.duration_ms > 0 && SDL_GetTicks() - start > options.duration_ms / 2) {
            SDL_SetWindowSize(window, 800, 450);
            resized = 1;
        }
        SDL_GL_GetDrawableSize(window, &width, &height);
        x += direction * 3;
        if (x <= 0 || x >= width - width / 5) {
            direction = -direction;
        }
        phase += 0.01f;
        glViewport(3, 5, width - 6, height - 8);
        glScissor(7, 9, width - 14, height - 16);
        glEnable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glUseProgram(0);
        glClearColor(0.04f + 0.02f * sinf(phase), 0.08f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(x, height / 2 - 50, width / 5, 100);
        glClearColor(0.18f, 0.52f, 0.86f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(width - x - width / 5, height / 2 + 70, width / 5, 48);
        glClearColor(0.92f, 0.52f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(3, 5, width - 6, height - 8);
        glScissor(7, 9, width - 14, height - 16);
        glEnable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glUseProgram(0);
        SDL_GL_SwapWindow(window);
        fixture_log_sentinel(options.trace, 0, 0);
        fixture_log_frame(options.trace, ++present_count, width, height);
        fixture_log_state(options.trace, "gl", state_restored(width, height));
        SDL_Delay(8);
    }
    if (options.screenshot != NULL && (width == 0 || height == 0 || fixture_write_gl_screenshot(options.screenshot, width, height) != 0)) {
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    fixture_monitor_summary(&monitor);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
