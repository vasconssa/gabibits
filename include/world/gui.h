#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sx/math.h"
#include "sx/allocator.h"
#include "sx/hash.h"
#include "world/types.h"
#include "world/entity_manager.h"
#include "world/renderer.h"

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear/nuklear.h"

#define MAX_NKTEXTURES 8

typedef struct nk_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
} nk_vertex;

typedef struct GuiPushConstantBlock {
    sx_vec2 scale;
    sx_vec2 translate;
} GuiPushConstantBlock;

typedef struct GuiInstanceData {
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    struct nk_context gui_context;
    struct nk_font_atlas atlas;
    struct nk_vec2 fb_scale;
    
    GuiPushConstantBlock push_constants_block;

    Buffer vertex_buffer;
    Buffer index_buffer;

    Texture textures[MAX_NKTEXTURES];
    VkDescriptorSet descriptor_set;
} GuiInstanceData;

typedef struct Gui {
    const sx_alloc* alloc;
    GuiInstanceData data;
    RendererContext* context;
} Gui;


Gui* create_gui(const sx_alloc* alloc, RendererContext* context);
void gui_update(Gui* gui);
void gui_draw(Gui* gui, uint32_t command_buffer_index);
void destroy_gui(Gui* gui);

NK_API void nk_font_stash_begin(Gui* gui, struct nk_font_atlas** atlas);
NK_API void nk_font_stash_end(Gui* gui);

