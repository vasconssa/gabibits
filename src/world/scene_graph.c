#include <stdalign.h>
#include "world/scene_graph.h"
#include "sx/os.h"
#include "sx/allocator.h"
#include "stdio.h"

SceneGraph* create_scenegraph(const sx_alloc* alloc, EntityManager* em) {
    SceneGraph* sg = sx_malloc(alloc, sizeof(*sg));
    sg->alloc = alloc;
    sg->entity_manager = em;
    sg->table = sx_hashtbl_create(alloc, 1000);
    sg->data.buffer = NULL;
    sg->data.capacity = 0;
    sg->data.size = 0;

    return sg;
}
/*
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
*/
void scenegraph_allocate(SceneGraph* sm, uint32_t size) {
    sx_assert_rel(size > sm->data.size);

    TransformInstanceData new_data;
    uint32_t bytes = size * (  sizeof(Entity) + alignof(Entity)
                             + 2 * sizeof(sx_mat4) + alignof(sx_mat4)
                             + 4 * sizeof(TransformInstance) + alignof(TransformInstance) );
    new_data.buffer = sx_malloc(sm->alloc, bytes);
    new_data.size = sm->data.size;
    new_data.capacity = size;

    new_data.entity       =  (Entity*           )sx_align_ptr((void*)(new_data.buffer             ), 0, alignof(Entity));
    new_data.local        =  (sx_mat4*          )sx_align_ptr((void*)(new_data.entity +       size), 0, alignof(sx_mat4));
    new_data.world        =  (sx_mat4*          )sx_align_ptr((void*)(new_data.local  +       size), 0, alignof(sx_mat4));
    new_data.parent       =  (TransformInstance*)sx_align_ptr((void*)(new_data.world  +       size), 0, alignof(TransformInstance));
    new_data.first_child  =  (TransformInstance*)sx_align_ptr((void*)(new_data.parent +       size), 0, alignof(TransformInstance));
    new_data.next_sibling =  (TransformInstance*)sx_align_ptr((void*)(new_data.first_child +  size), 0, alignof(TransformInstance));
    new_data.prev_sibling =  (TransformInstance*)sx_align_ptr((void*)(new_data.next_sibling + size), 0, alignof(TransformInstance));

    if (sm->data.buffer) {
        sx_memcpy(new_data.entity, sm->data.entity, sm->data.size * sizeof(Entity));
        sx_memcpy(new_data.local, sm->data.local, sm->data.size * sizeof(sx_mat4));
        sx_memcpy(new_data.world, sm->data.world, sm->data.size * sizeof(sx_mat4));
        sx_memcpy(new_data.parent, sm->data.parent, sm->data.size * sizeof(TransformInstance));
        sx_memcpy(new_data.first_child, sm->data.first_child, sm->data.size * sizeof(TransformInstance));
        sx_memcpy(new_data.next_sibling, sm->data.next_sibling, sm->data.size * sizeof(TransformInstance));
        sx_memcpy(new_data.prev_sibling, sm->data.prev_sibling, sm->data.size * sizeof(TransformInstance));

        sx_free(sm->alloc, sm->data.buffer);
    }
    sm->data = new_data;
}

void scenegraph_grow(SceneGraph* sm) {
    scenegraph_allocate(sm, sm->data.capacity * 2 + 1);
}

TransformInstance create_transform_instance(SceneGraph* sm, Entity e, sx_mat4* m) {
    if (sm->data.capacity == sm->data.size) {
        scenegraph_grow(sm);
    }

    uint32_t last = sm->data.size;
    sm->data.size++;

    sm->data.entity[last] = e;
    sx_memcpy(&sm->data.world[last], m, sizeof(*m));
    sx_memcpy(&sm->data.local[last], m, sizeof(*m));
    sm->data.parent[last].i = UINT32_MAX;
    sm->data.first_child[last].i = UINT32_MAX;
    sm->data.next_sibling[last].i = UINT32_MAX;
    sm->data.prev_sibling[last].i = UINT32_MAX;

    sx_hashtbl_add(sm->table, sx_hash_u32(e.handle), last);
    printf("transforminstance: %u\n\n", last);

    TransformInstance inst;
    inst.i = last;
    return inst;
}

void destroy_transform_instance(SceneGraph* sm, Entity e, TransformInstance i) {
}

bool scenegraph_lookup(SceneGraph* sm, Entity e) {
    return sx_hashtbl_find(sm->table, sx_hash_u32(e.handle)) != -1;
}

void transform(SceneGraph* sm, sx_mat4* parent, TransformInstance i) {
}

sx_mat4 get_local_matrix(SceneGraph* sm, Entity e) {
    TransformInstance i;
    i.i = sx_hashtbl_find_get(sm->table, sx_hash_u32(e.handle), -1);
    return sm->data.local[i.i];
}

void set_local_position(SceneGraph* sm, Entity e, sx_vec3 position) {
    TransformInstance i;
    i.i = sx_hashtbl_find_get(sm->table, sx_hash_u32(e.handle), -1);
    sm->data.local[i.i].col4 = sx_vec4f(position.x, position.y, position.z, 1.0);
}

sx_vec3 get_local_position(SceneGraph* sm, Entity e) {
    TransformInstance i;
    i.i = sx_hashtbl_find_get(sm->table, sx_hash_u32(e.handle), -1);
    sx_vec4 p =  sm->data.local[i.i].col4;
    return sx_vec3f(p.x, p.y, p.z);
}

void set_local_rotation(SceneGraph* sm, Entity e, sx_vec3 rotation) {
    TransformInstance i;
    i.i = sx_hashtbl_find_get(sm->table, sx_hash_u32(e.handle), -1);
    sx_vec4 p =  sm->data.local[i.i].col4;
    sm->data.local[i.i] = sx_mat4_rotateZYX(rotation.x, rotation.y, rotation.z);
    sm->data.local[i.i].col4 = sx_vec4f(p.x, p.y, p.z, 1.0);
}

sx_vec3 get_local_rotation(SceneGraph* sm, Entity e) {
    TransformInstance i;
    i.i = sx_hashtbl_find_get(sm->table, sx_hash_u32(e.handle), -1);
    sx_quat q = sx_mat4_quat(&sm->data.local[i.i]);
    return sx_quat_toeuler(q);
}

void scenegraph_destroy(SceneGraph* sm) {
    sx_hashtbl_destroy(sm->table, sm->alloc);
    sx_free(sm->alloc, sm->data.buffer);
    sx_free(sm->alloc, sm);
}
