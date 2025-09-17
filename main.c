#include <stdio.h>
#include <SDL3/SDL.h>

int main(void) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {                
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("Hello SDL3", 800, 600, SDL_WINDOW_RESIZABLE);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Delay(1000);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
