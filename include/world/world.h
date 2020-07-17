#pragma once
#include <stdint.h>
#include "sx/allocator.h"
#include "world/types.h"
#include "world/entity_manager.h"
#include "world/renderer.h"
#include "world/scene_graph.h"
#include "world/terrain_system.h"
#include "camera.h"

typedef struct World {
    const sx_alloc* alloc;
    EntityManager* entity_manager;
    Renderer* renderer;
    SceneGraph* scene_graph;
    TerrainSystem* terrain_system;
    Entity* entities;
    FpsCamera* cameras;
} World;

World* create_world(const sx_alloc* alloc);
void game_init(World* world);
void world_update(World* world);
void world_destroy(World* world);
