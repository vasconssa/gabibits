#include "world/world.h"
#include "resource/gltf_importer.h"
#include "sx/string.h"
#include <stdio.h>

World* create_world(const sx_alloc* alloc) {
    World* world = sx_malloc(alloc, sizeof(*world));
    world->alloc = alloc;
    world->entity_manager = create_entity_manager(alloc);
    world->scene_graph = create_scenegraph(alloc, world->entity_manager);
    world->renderer = create_renderer(alloc, world->entity_manager);
    world->terrain_system = create_terrainsystem(alloc, world->entity_manager, world->renderer->context);
    world->renderer->terrain_system = world->terrain_system;


    return world;
}

void game_init(World* world) {
    MeshData* mesh_data;
    MaterialsData material_data;
    int num_meshes = scene_importer("misc/wingsuit.gltf", &mesh_data, &material_data);
    world->entities = sx_malloc(world->alloc, (num_meshes + 1) * sizeof(Entity));
    init_pbr_materials(world->renderer, &material_data, "misc/");
    sx_free(sx_alloc_malloc(), material_data.base_color_texture_names);
    sx_free(sx_alloc_malloc(), material_data.metallic_roughness_texture_names);
    sx_free(sx_alloc_malloc(), material_data.normal_texture_names);
    sx_free(sx_alloc_malloc(), material_data.occlusion_texture_names);
    sx_free(sx_alloc_malloc(), material_data.emissive_texture_names);
    sx_free(sx_alloc_malloc(), material_data.base_color_factors);
    sx_free(sx_alloc_malloc(), material_data.emissive_factors);
    sx_free(sx_alloc_malloc(), material_data.metallic_factors);
    sx_free(sx_alloc_malloc(), material_data.roughness_factors);

    for (int i = 1; i < num_meshes + 1; i++) {
        world->entities[i] = entity_create(world->entity_manager);
        create_mesh_instance(world->renderer, world->entities[i], &mesh_data[i - 1]);
        sx_mat4 t_matrix = sx_mat4_translate(1796.520996, 1202.708618, 1897.478760);
        sx_mat4 r_matrix = sx_mat4_rotateXYZ(sx_torad(-90), sx_torad(180), sx_torad(-45));
        sx_mat4 result = sx_mat4_mul(&t_matrix, &r_matrix);

        create_transform_instance(world->scene_graph, world->entities[i], &result);
        update_transform(world->renderer, world->entities[i], &result);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].index_data);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].vertex_data);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].vertex_offsets);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].index_offsets);
        sx_free(sx_alloc_malloc(), mesh_data[i-1].material_indices);
    }
    sx_free(sx_alloc_malloc(), mesh_data);
    TerrainTileInfo terrain_info;
    terrain_info.displacement_factor = 1300;
    terrain_info.tessellation_factor = 1.5;
    terrain_info.tessellated_edge_size = 1280;
    sx_strcpy(terrain_info.terrain_heightmap_texture, 200, "misc/terrain/terrain_demolevel_heightmap2.ktx");
    sx_strcpy(terrain_info.terrain_normal_texture, 200, "misc/terrain/terrain_demolevel_normal2.ktx");
    sx_strcpy(terrain_info.terrain_layers_texture, 200, "misc/terrain/terrain_demolevel_colormap2.ktx");
    world->entities[0] = entity_create(world->entity_manager);
    create_terrain_instance(world->terrain_system, world->entities[0], terrain_info);
    
    renderer_prepare(world->renderer);
}

void world_update(World* world) {
    renderer_render(world->renderer);
}

void world_destroy(World* world) {
    destroy_terrainsystem(world->terrain_system);
    destroy_renderer(world->renderer);
    scenegraph_destroy(world->scene_graph);
    entity_manager_destroy(world->entity_manager);
    sx_free(world->alloc, world->entities);
    sx_free(world->alloc, world);
}

