#pragma once
#include <stdint.h>
#include "sx/allocator.h"
#include "world/types.h"

typedef struct EntityManager {
    sx_handle_pool* handles;
    const sx_alloc* alloc;
} EntityManager;

EntityManager* create_entity_manager(const sx_alloc* alloc);
void entity_manager_destroy(EntityManager* em);
Entity entity_create(EntityManager* em);
bool entity_alive(EntityManager* em, Entity e);
uint64_t entity_index(Entity e);
void entity_destroy(EntityManager* em, Entity e);

