#include "world/entity_manager.h"

EntityManager* create_entity_manager(const sx_alloc* alloc) {
    EntityManager* em = sx_malloc(alloc, sizeof(*em));
    em->alloc = alloc;
    em->handles = sx_handle_create_pool(em->alloc, 1000);
    return em;
}

void entity_manager_destroy(EntityManager* em) {
    sx_handle_destroy_pool(em->handles, em->alloc);
}

Entity entity_create(EntityManager* em) {
    if (sx_handle_full(em->handles)) {
        sx_handle_grow_pool(&em->handles, em->alloc);
    }
    Entity entity;
    entity.handle = sx_handle_new(em->handles);

    return entity;
}

bool entity_alive(EntityManager* em, Entity e) {
    return sx_handle_valid(em->handles, e.handle);
}

void entity_destroy(EntityManager* em, Entity e) {
    sx_handle_del(em->handles, e.handle);
}

uint64_t entity_index(Entity e) {
    return sx_handle_index(e.handle);
}
