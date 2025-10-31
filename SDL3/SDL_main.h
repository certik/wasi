#ifndef SDL_MAIN_H
#define SDL_MAIN_H

// Mock SDL_main.h - provides managed main support for tests

#ifdef SDL_MAIN_USE_CALLBACKS

#ifdef __cplusplus
extern "C" {
#endif

int SDL_main(int argc, char *argv[]);
#define main SDL_main

#ifdef __cplusplus
}
#endif

#endif // SDL_MAIN_USE_CALLBACKS

#endif // SDL_MAIN_H
