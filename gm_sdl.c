#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char* args[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Hello SDL 3", 640, 480, 0);
    if (window == NULL) {
        SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_PumpEvents();  // Force window to display on macOS

    SDL_Delay(3000);  // Wait 3 seconds

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
