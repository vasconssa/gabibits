#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sx/math.h"
#include "sx/allocator.h"
#include "sx/hash.h"
#include "world/types.h"
#include "world/entity_manager.h"

/// Instance data.
typedef struct TransformInstanceData {
    unsigned size;              ///< Number of used entries in arrays
    unsigned capacity;          ///< Number of allocated entries in arrays
    void *buffer;               ///< Raw buffer for data.

    Entity *entity;             ///< The entity owning this instance.
    sx_mat4 *local;           ///< Local transform with respect to parent.
    sx_mat4 *world;           ///< World transform.
    TransformInstance *parent;           ///< The parent instance of this instance.
    TransformInstance *first_child;      ///< The first child of this instance.
    TransformInstance *next_sibling;     ///< The next sibling of this instance.
    TransformInstance *prev_sibling;     ///< The previous sibling of this instance.
} TransformInstanceData;

typedef struct SceneGraph {
    const sx_alloc* alloc;
    EntityManager* entity_manager;
    TransformInstanceData data;
    sx_hashtbl* table;
} SceneGraph;

SceneGraph* create_scenegraph(const sx_alloc* alloc, EntityManager* em);
void scenegraph_allocate(SceneGraph* sm, uint32_t size);
void scenegraph_grow(SceneGraph* sm);
void scenegraph_destroy(SceneGraph* sm);
TransformInstance create_transform_instance(SceneGraph* sm, Entity e, sx_mat4* m);
void destroy_transform_instance(SceneGraph* sm, Entity e, TransformInstance i);
bool scenegraph_lookup(SceneGraph* sm, Entity e);
void transform(SceneGraph* sm, sx_mat4* parent, TransformInstance i);
sx_mat4 get_local_matrix(SceneGraph* sm, Entity e);
void set_local_position(SceneGraph* sm, Entity e, sx_vec3 position);
sx_vec3 get_local_position(SceneGraph* sm, Entity e);
void set_local_rotation(SceneGraph* sm, Entity e, sx_vec3 rotation);
sx_vec3 get_local_rotation(SceneGraph* sm, Entity e);

