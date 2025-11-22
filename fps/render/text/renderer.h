#ifndef FPS_TEXT_RENDERER_H
#define FPS_TEXT_RENDERER_H

#include "../../core/core.h"
#include "../../core/map.h"

typedef struct {
    const FPS_Map *map;
} FPS_TextRenderer;

bool fps_text_renderer_init(FPS_TextRenderer *renderer, const FPS_Map *map);
void fps_text_renderer_draw(FPS_TextRenderer *renderer,
                            const FPS_EntityState *entities,
                            int count);

#endif // FPS_TEXT_RENDERER_H
