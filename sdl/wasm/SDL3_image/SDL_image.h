#ifndef SDL_IMAGE_H
#define SDL_IMAGE_H

#include <SDL3/SDL.h>

// Load an image from a path into a software surface
SDL_Surface* IMG_Load(const char* file);

// Load an image from an SDL_IOStream into a software surface
// If closeio is true, the IOStream will be closed after loading
SDL_Surface* IMG_Load_IO(SDL_IOStream* src, bool closeio);

#endif // SDL_IMAGE_H
