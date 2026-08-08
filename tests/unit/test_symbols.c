#include "symbols.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition); return 1; } } while (0)

int main(void)
{
    OmniSymbols symbols;
    (void)SDL_Init(0);
    CHECK(omni_symbols_resolve(&symbols) == 0);
    CHECK(symbols.poll_event != NULL);
    CHECK(symbols.peep_events != NULL);
    CHECK(omni_symbol_address(&symbols, "SDL_PollEvent") != NULL);
    CHECK(omni_symbol_address(&symbols, "not-an-sdl-symbol") == NULL);
    SDL_Quit();
    return 0;
}
