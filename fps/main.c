#include "fps_core.h"
#include "fps_mock_plugins.h"
#include "fps_renderer.h"
#include "fps_assets.h"

#include <stdio.h>

enum {
    TYPE_PLAYER = 1,
    TYPE_CRATE = 2,
};

int main(void) {
    FPS_CoreHandle core = FPS_Core_Init();
    if (!core) {
        fprintf(stderr, "Failed to create core\n");
        return 1;
    }

    // Register mock plugins
    FPS_PhysicsInterface physics = FPS_MockPhysicsInterface();
    FPS_GameplayInterface gameplay = FPS_MockGameplayInterface();
    FPS_Core_RegisterPhysics(core, &physics);
    FPS_Core_RegisterGameplay(core, &gameplay);

    // Spawn a player and a static crate
    FPS_EntityID player_id = FPS_Core_SpawnEntity(core, TYPE_PLAYER, (FPS_Vec3){0, 0, 0});
    (void)player_id;
    FPS_Core_SpawnEntity(core, TYPE_CRATE, (FPS_Vec3){2, 0, 2});

    // Load a mock scene and create renderer
    FPS_Scene scene;
    FPS_Scene_LoadFromFile("mock_scene.bin", &scene);
    FPS_Renderer *renderer = FPS_Renderer_Create();
    FPS_Renderer_LoadScene(renderer, &scene);

    // Simulate a couple frames
    FPS_InputFrame input = {0};
    input.move_axis_x = 1.0f; // strafe right
    input.move_axis_y = 0.5f; // forward
    input.action_primary = true;
    input.action_interact = true;

    for (int i = 0; i < 3; i++) {
        FPS_Core_Update(core, input, 0.016f);
        FPS_Renderer_Draw(renderer,
                          FPS_Core_GetEntities(core),
                          (int)FPS_Core_GetEntityCount(core),
                          NULL,
                          0);
        input.action_primary = false; // only fire first frame
        input.action_interact = false;
    }

    FPS_Renderer_Destroy(renderer);
    FPS_Core_Destroy(core);
    FPS_Scene_Unload(&scene);
    return 0;
}
