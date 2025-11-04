// stb_image implementation wrapper
// Download stb_image.h from: https://github.com/nothings/stb/blob/master/stb_image.h
// For now, we'll use SDL_Surface instead which SDL3 provides

#ifndef STB_IMAGE_IMPL_H
#define STB_IMAGE_IMPL_H

#include <SDL3/SDL.h>

// Simple wrapper to load image using SDL_Surface
typedef struct {
    unsigned char *data;
    int width;
    int height;
    int channels;
} ImageData;

static ImageData load_image(const char *filename) {
    ImageData result = {0};
    
    SDL_Surface *surface = SDL_LoadBMP(filename);
    if (!surface) {
        // Try using SDL_image if available, otherwise we need to convert JPG to BMP
        // For now, log error
        SDL_Log("Failed to load image %s: %s", filename, SDL_GetError());
        return result;
    }
    
    // Convert to RGBA format
    SDL_Surface *rgba_surface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    
    if (!rgba_surface) {
        SDL_Log("Failed to convert surface to RGBA: %s", SDL_GetError());
        return result;
    }
    
    result.width = rgba_surface->w;
    result.height = rgba_surface->h;
    result.channels = 4;
    
    // Allocate and copy pixel data
    size_t data_size = result.width * result.height * result.channels;
    result.data = (unsigned char *)SDL_malloc(data_size);
    if (result.data) {
        SDL_memcpy(result.data, rgba_surface->pixels, data_size);
    }
    
    SDL_DestroySurface(rgba_surface);
    return result;
}

static void free_image(ImageData *img) {
    if (img && img->data) {
        SDL_free(img->data);
        img->data = NULL;
    }
}

#endif // STB_IMAGE_IMPL_H
