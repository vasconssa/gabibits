#include "world/world.h"
#include "resource/gltf_importer.h"
#include <stdio.h>

World* create_world(const sx_alloc* alloc) {
    World* world = sx_malloc(alloc, sizeof(*world));
    world->alloc = alloc;
    world->entity_manager = create_entity_manager(alloc);
    world->scene_graph = create_scenegraph(alloc, world->entity_manager);
    world->renderer = create_renderer(alloc, world->entity_manager);


    return world;
}

void game_init(World* world) {
    MeshData* mesh_data;
    MaterialsData material_data;
    int num_meshes = scene_importer("misc/wingsuit.gltf", &mesh_data, &material_data);
    world->entities = sx_malloc(world->alloc, num_meshes * sizeof(Entity));
    init_pbr_materials(world->renderer, &material_data, "misc/");
    printf("num_meshes: %d\n", mesh_data[0].vertex_size);
    for (int i = 0; i < num_meshes; i++) {
        world->entities[i] = entity_create(world->entity_manager);
        create_mesh_instance(world->renderer, world->entities[i], &mesh_data[i]);
        sx_mat4 t_matrix = sx_mat4_translate(1796.520996, 1202.708618, 1897.478760);
        sx_mat4 r_matrix = sx_mat4_rotateXYZ(sx_torad(-90), sx_torad(180), sx_torad(45));
        sx_mat4 result = sx_mat4_mul(&t_matrix, &r_matrix);

        create_transform_instance(world->scene_graph, world->entities[i], &result);
        update_transform(world->renderer, world->entities[i], &result);
    }
    renderer_prepare(world->renderer);
}

void world_update(World* world) {
    renderer_render(world->renderer);
}
