#ifndef __wasm__
#include <SDL3/SDL.h>

void SDL_DestroyGPUBuffer(SDL_GPUDevice* device, SDL_GPUBuffer* buffer) {
    if (device && buffer) {
        SDL_ReleaseGPUBuffer(device, buffer);
    }
}

void SDL_BindGPUVertexBuffer(SDL_GPURenderPass* render_pass, Uint32 slot, SDL_GPUBuffer* buffer, size_t offset, Uint32 stride) {
    (void)stride; // Stride is not handled in this minimal wrapper.
    if (!render_pass || !buffer) {
        return;
    }
    SDL_GPUBufferBinding binding;
    binding.buffer = buffer;
    binding.offset = (Uint32)offset;
    SDL_BindGPUVertexBuffers(render_pass, slot, &binding, 1);
}

int SDL_main(int argc, char *argv[]) {
    SDL_SetMainReady();
    void *state = NULL;
    SDL_AppResult init_result = SDL_AppInit(&state, argc, argv);
    if (init_result == SDL_APP_FAILURE) {
        if (state) {
            SDL_AppQuit(state, init_result);
        }
        return 1;
    }
    if (init_result == SDL_APP_SUCCESS) {
        SDL_AppQuit(state, init_result);
        return 0;
    }

    SDL_AppResult loop_result = init_result;
    while (loop_result == SDL_APP_CONTINUE) {
        loop_result = SDL_AppIterate(state);
    }

    SDL_AppQuit(state, loop_result);
    return (loop_result == SDL_APP_SUCCESS) ? 0 : 1;
}

#ifdef main
#undef main
#endif
int main(int argc, char *argv[]) {
    return SDL_main(argc, argv);
}
#endif
