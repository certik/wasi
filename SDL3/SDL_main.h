#ifndef SDL_MAIN_H
#define SDL_MAIN_H

// Mock SDL_main.h - provides managed main support for tests

#ifdef SDL_MAIN_USE_CALLBACKS

#ifdef __cplusplus
extern "C" {
#endif

extern SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]);
extern SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event);
extern SDL_AppResult SDL_AppIterate(void *appstate);
extern void SDL_AppQuit(void *appstate, SDL_AppResult result);
extern bool SDL_PollEvent(SDL_Event *event);

static int SDL_main(int argc, char *argv[]) {
    void *appstate = NULL;
    SDL_AppResult result = SDL_AppInit(&appstate, argc, argv);

    if (result == SDL_APP_FAILURE) {
        if (appstate) {
            SDL_AppQuit(appstate, result);
        }
        return 1;
    }

    if (result == SDL_APP_SUCCESS) {
        SDL_AppQuit(appstate, result);
        return 0;
    }

    SDL_AppResult loop_result = result;
    while (loop_result == SDL_APP_CONTINUE) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            SDL_AppResult event_result = SDL_AppEvent(appstate, &event);
            if (event_result != SDL_APP_CONTINUE) {
                loop_result = event_result;
                break;
            }
        }

        if (loop_result != SDL_APP_CONTINUE) {
            break;
        }

        loop_result = SDL_AppIterate(appstate);
    }

    SDL_AppQuit(appstate, loop_result);
    return (loop_result == SDL_APP_SUCCESS) ? 0 : 1;
}

#define main SDL_main

#ifdef __cplusplus
}
#endif

#endif // SDL_MAIN_USE_CALLBACKS

#endif // SDL_MAIN_H
