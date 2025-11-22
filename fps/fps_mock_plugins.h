#ifndef FPS_MOCK_PLUGINS_H
#define FPS_MOCK_PLUGINS_H

#include "fps_plugin_physics.h"
#include "fps_plugin_logic.h"
#include "fps_renderer.h"

// Helpers that return simple mock interfaces for testing.
FPS_PhysicsInterface FPS_MockPhysicsInterface(void);
FPS_GameplayInterface FPS_MockGameplayInterface(void);

#endif // FPS_MOCK_PLUGINS_H
