#include "fps_renderer.h"
#include "fps_assets.h"

#include <stdio.h>
#include <stdlib.h>

struct FPS_Renderer {
    const FPS_Scene *scene;
};

FPS_Renderer* FPS_Renderer_Create(void) {
    FPS_Renderer *r = (FPS_Renderer*)calloc(1, sizeof(FPS_Renderer));
    return r;
}

void FPS_Renderer_Destroy(FPS_Renderer *r) {
    free(r);
}

bool FPS_Renderer_LoadScene(FPS_Renderer *r, const FPS_Scene *scene) {
    if (!r) return false;
    r->scene = scene;
    return true;
}

bool FPS_Renderer_Draw(FPS_Renderer *r,
                       const FPS_EntityState *entities, int count,
                       const void *uniforms, uint32_t uniform_size) {
    (void)uniforms;
    (void)uniform_size;
    if (!r) return false;
    const char *scene_name = (r->scene && r->scene->meta.name) ? r->scene->meta.name : "(no scene)";
    printf("[Renderer] Drawing scene '%s' with %d entities\n", scene_name, count);
    for (int i = 0; i < count; i++) {
        const FPS_EntityState *e = &entities[i];
        printf("  Entity %u type %u at (%.2f, %.2f, %.2f)\n",
               e->id, e->type, e->position.x, e->position.y, e->position.z);
    }
    return true;
}
