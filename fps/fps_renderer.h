#ifndef FPS_RENDERER_H
#define FPS_RENDERER_H

#include <stdbool.h>
#include <stdint.h>
#include "fps_types.h"

struct FPS_Scene; // Forward declaration for scene asset type

typedef struct FPS_Renderer FPS_Renderer;

// Construct/destroy the renderer plugin (mock implementation provided).
FPS_Renderer* FPS_Renderer_Create(void);
void FPS_Renderer_Destroy(FPS_Renderer *r);

// Load the static scene (geometry/textures in a real impl; mock just records metadata).
bool FPS_Renderer_LoadScene(FPS_Renderer *r, const struct FPS_Scene *scene);

// Per-frame draw: consume truth from Core.
bool FPS_Renderer_Draw(FPS_Renderer *r,
                       const FPS_EntityState *entities, int count,
                       const void *uniforms, uint32_t uniform_size);

#endif // FPS_RENDERER_H
