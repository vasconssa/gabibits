#include "world/world.h"
#include "resource/gltf_importer.h"
#include "device/vk_device.h"
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
    world->gui = create_gui(alloc, world->renderer->context);
    world->renderer->gui = world->gui;


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
    
    struct nk_font_atlas* atlas;

    struct nk_color table[NK_COLOR_COUNT];
    table[NK_COLOR_TEXT] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_WINDOW] = nk_rgba(57, 67, 71, 215);
    table[NK_COLOR_HEADER] = nk_rgba(51, 51, 56, 220);
    table[NK_COLOR_BORDER] = nk_rgba(46, 46, 46, 255);
    table[NK_COLOR_BUTTON] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(63, 98, 126, 255);
    table[NK_COLOR_TOGGLE] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(45, 53, 56, 255);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SELECT] = nk_rgba(57, 67, 61, 255);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SLIDER] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(48, 83, 111, 245);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_PROPERTY] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_EDIT] = nk_rgba(50, 58, 61, 225);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_COMBO] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_CHART] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_CHART_COLOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR] = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(48, 83, 111, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(53, 88, 116, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(58, 93, 121, 255);
    table[NK_COLOR_TAB_HEADER] = nk_rgba(48, 83, 111, 255);
    nk_style_from_table(&world->gui->data.gui_context, table);
    /*nk_style_default(&world->gui->data.gui_context);*/
    nk_font_stash_begin(world->gui, &atlas);
    nk_font_stash_end(world->gui);
    renderer_prepare(world->renderer);

}

void world_update(World* world) {
    struct nk_context* ctx = &world->gui->data.gui_context;

    nk_input_begin(ctx);
    sx_vec3 m = mouse_axis(device()->input_manager, MA_CURSOR);
    nk_input_motion(ctx, (int)m.x, (int)m.y);
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_LEFT));
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_MIDDLE));
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)m.x, (int)m.y, mouse_button(device()->input_manager, MB_RIGHT));
    nk_input_end(ctx);

    if (nk_begin(&world->gui->data.gui_context, "Gabibits", nk_rect(50, 50, 200, 200),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_CLOSABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {
        enum {EASY, HARD};
        static int op = EASY;

        nk_layout_row_static(&world->gui->data.gui_context, 30, 80, 1);
        if (nk_button_label(&world->gui->data.gui_context, "button"))
            fprintf(stdout, "button pressed\n");
        nk_layout_row_dynamic(&world->gui->data.gui_context, 30, 2);
        if (nk_option_label(&world->gui->data.gui_context, "easy", op == EASY)) op = EASY;
        if (nk_option_label(&world->gui->data.gui_context, "hard", op == HARD)) op = HARD;
    }
    nk_end(&world->gui->data.gui_context);
    
    gui_update(world->gui);
    renderer_render(world->renderer);
    nk_clear(&world->gui->data.gui_context);
    /*nk_buffer_clear(&world->gui->data.cmds);*/
}

void world_destroy(World* world) {
    destroy_terrainsystem(world->terrain_system);
    destroy_gui(world->gui);
    destroy_renderer(world->renderer);
    scenegraph_destroy(world->scene_graph);
    entity_manager_destroy(world->entity_manager);
    sx_free(world->alloc, world->entities);
    sx_free(world->alloc, world);
}

