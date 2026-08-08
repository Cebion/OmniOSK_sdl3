#include "../fixture.h"

#include <SDL.h>

#include <stdio.h>

static int state_restored(SDL_Renderer *renderer, int width, int height)
{
    SDL_Rect viewport;
    SDL_Rect clip;
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha;
    SDL_BlendMode blend;
    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderGetClipRect(renderer, &clip);
    SDL_GetRenderDrawColor(renderer, &red, &green, &blue, &alpha);
    SDL_GetRenderDrawBlendMode(renderer, &blend);
    return viewport.w == width - 6 && viewport.h == height - 8 && clip.x == 7 && clip.y == 9 &&
           clip.w == width - 14 && clip.h == height - 16 && red == 12 && green == 34 && blue == 56 &&
           alpha == 255 && blend == SDL_BLENDMODE_ADD;
}

int main(int argc, char **argv)
{
    FixtureOptions options;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Event event;
    Uint32 start;
    int width = 0;
    int height = 0;
    int resized = 0;
    int quit = 0;
    int present_count = 0;
    int x = 0;
    int direction = 1;
    FixtureMonitor monitor;
    if (fixture_parse_options(argc, argv, &options) != 0) {
        fprintf(stderr, "usage: %s [--trace PATH] [--events SCRIPT] [--duration-ms N] [--screenshot PATH]\n", argv[0]);
        return 2;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "renderer: SDL_Init failed: %s\n", SDL_GetError());
        return 77;
    }
    window = SDL_CreateWindow(fixture_window_title("Omni OSK SDL Renderer sample"), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              960, 540, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        fprintf(stderr, "renderer: window unavailable: %s\n", SDL_GetError());
        SDL_Quit();
        return 77;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL) {
        fprintf(stderr, "renderer: renderer unavailable: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 77;
    }
    fixture_monitor_init(&monitor, window, "renderer", options.events);
    fixture_push_script(options.events);
    start = SDL_GetTicks();
    while (options.duration_ms == 0 || SDL_GetTicks() - start < options.duration_ms) {
        SDL_Rect viewport;
        SDL_Rect clip;
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
        SDL_GetRendererOutputSize(renderer, &width, &height);
        x += direction * 3;
        if (x <= 0 || x >= width - 100) {
            direction = -direction;
        }
        viewport = (SDL_Rect){3, 5, width - 6, height - 8};
        clip = (SDL_Rect){7, 9, width - 14, height - 16};
        SDL_RenderSetViewport(renderer, &viewport);
        SDL_RenderSetClipRect(renderer, &clip);
        SDL_SetRenderDrawColor(renderer, 12, 34, 56, 255);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 60, 145, 220, 255);
        SDL_Rect rectangle = {x, height / 2 - 50, 100, 100};
        SDL_RenderFillRect(renderer, &rectangle);
        SDL_RenderSetViewport(renderer, &viewport);
        SDL_RenderSetClipRect(renderer, &clip);
        SDL_SetRenderDrawColor(renderer, 12, 34, 56, 255);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_RenderPresent(renderer);
        fixture_log_sentinel(options.trace, SDL_GetRenderTarget(renderer) != NULL ? 1U : 0U,
                             (unsigned int)(viewport.w > 0 && viewport.h > 0));
        fixture_log_frame(options.trace, ++present_count, width, height);
        fixture_log_state(options.trace, "renderer", state_restored(renderer, width, height));
        SDL_Delay(8);
    }
    if (options.screenshot != NULL && (width == 0 || height == 0 || fixture_write_renderer_screenshot(options.screenshot, renderer, width, height) != 0)) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    fixture_monitor_summary(&monitor);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
