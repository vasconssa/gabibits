#include <stdarg.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear/nuklear.h"
#undef NK_IMPLEMENTATION

#include "world/gui.h"


#include "volk/volk.h"

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER  128 * 1024

void update_gui_descriptors(Gui* gui);

Gui* create_gui(const sx_alloc* alloc, RendererContext* context) {
    Gui* gui = sx_malloc(alloc, sizeof(*gui));

    gui->alloc = alloc;
    gui->context = context;

    VkResult result;

    nk_buffer_init_default(&gui->data.cmds);
    nk_init_default(&gui->data.gui_context, 0);
    gui->data.fb_scale.x = 1;
    gui->data.fb_scale.y = 1;

    result = create_buffer(&gui->data.vertex_buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, MAX_VERTEX_BUFFER);
    sx_assert_rel(result == VK_SUCCESS && "Could not create vertex buffer!");

    result = create_buffer(&gui->data.index_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, MAX_INDEX_BUFFER);
    sx_assert_rel(result == VK_SUCCESS && "Could not create vertex buffer!");

    for (int32_t i = 0; i < MAX_NKTEXTURES; i++) {
        result = create_texture(&gui->data.textures[i], VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE, gui->alloc, "misc/empty.ktx");
    }
    // nk gui descriptor set
    {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.pNext = NULL;
        descriptor_set_allocate_info.descriptorPool = gui->context->nk_gui_descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &gui->context->nk_gui_descriptor_layout;

        result = vkAllocateDescriptorSets(gui->context->device.logical_device, &descriptor_set_allocate_info,
                &gui->data.descriptor_set);
        sx_assert_rel("Could not create nk gui descriptor set!");
    }
    /*update_gui_descriptors(gui);*/
    gui->data.push_constants_block.scale.x = 2.0 / gui->context->width;
    gui->data.push_constants_block.scale.y = 2.0 / gui->context->height;
    gui->data.push_constants_block.translate.x = -1.0;
    gui->data.push_constants_block.translate.y = -1.0;

    return gui;
}

uint8_t vertices[MAX_VERTEX_BUFFER];
uint8_t indices[MAX_INDEX_BUFFER];
void gui_update(Gui* gui) {
    struct nk_buffer vbuf, ibuf;
/* load draw vertices & elements directly into vertex + element buffer */
    {
        /* fill convert configuration */
        struct nk_convert_config config;
        static const struct nk_draw_vertex_layout_element vertex_layout[] = {
            {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(nk_vertex, position)},
            {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(nk_vertex, uv)},
            {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(nk_vertex, col)},
            {NK_VERTEX_LAYOUT_END}
        };
        sx_memset(&config, 0, sizeof(config));
        config.vertex_layout = vertex_layout;
        config.vertex_size = sizeof(nk_vertex);
        config.vertex_alignment = NK_ALIGNOF(nk_vertex);
        config.null = gui->data.null;
        config.circle_segment_count = 22;
        config.curve_segment_count = 22;
        config.arc_segment_count = 22;
        config.global_alpha = 1.0f;
        config.shape_AA = NK_ANTI_ALIASING_ON;
        config.line_AA = NK_ANTI_ALIASING_ON;

        /* setup buffers to load vertices and elements */
        nk_buffer_init_fixed(&vbuf, vertices, (size_t)MAX_VERTEX_BUFFER);
        nk_buffer_init_fixed(&ibuf, indices, (size_t)MAX_INDEX_BUFFER);
        nk_convert(&gui->data.gui_context, &gui->data.cmds, &vbuf, &ibuf, &config);
        copy_buffer(&gui->data.vertex_buffer, vertices, sizeof(vertices));
        copy_buffer(&gui->data.index_buffer, indices, sizeof(indices));
    }
}

void gui_draw(Gui* gui, uint32_t command_buffer_index) {

    vkCmdBindPipeline(gui->context->graphic_queue_cmdbuffer[command_buffer_index], 
            VK_PIPELINE_BIND_POINT_GRAPHICS, gui->context->nk_gui_pipeline);
    vkCmdBindDescriptorSets(gui->context->graphic_queue_cmdbuffer[command_buffer_index], 
            VK_PIPELINE_BIND_POINT_GRAPHICS, gui->context->nk_gui_pipeline_layout,
            1, 1, &gui->data.descriptor_set, 0, NULL);
    gui->data.push_constants_block.scale.x = 2.0 / gui->context->width;
    gui->data.push_constants_block.scale.y = 2.0 / gui->context->height;
    gui->data.push_constants_block.translate.x = -1.0;
    gui->data.push_constants_block.translate.y = -1.0;
    vkCmdPushConstants(gui->context->graphic_queue_cmdbuffer[command_buffer_index], gui->context->nk_gui_pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
            0, sizeof(GuiPushConstantBlock), &gui->data.push_constants_block);
    {
        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;

            /* iterate over and execute each draw command */
        VkDeviceSize doffset = 0;
        vkCmdBindVertexBuffers(gui->context->graphic_queue_cmdbuffer[command_buffer_index], 
                0, 1, &gui->data.vertex_buffer.buffer, &doffset);
        vkCmdBindIndexBuffer(gui->context->graphic_queue_cmdbuffer[command_buffer_index], 
                gui->data.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);

        
        uint32_t index_offset = 0;
        nk_draw_foreach(cmd, &gui->data.gui_context, &gui->data.cmds) {
            if (!cmd->elem_count) continue;

            VkRect2D scissor = {
                .extent = {
                    .width = cmd->clip_rect.w * gui->data.fb_scale.x,
                    .height = cmd->clip_rect.h * gui->data.fb_scale.y,
                },
                .offset = {
                    .x = sx_max(cmd->clip_rect.x * gui->data.fb_scale.x, (float)0.0),
                    .y = sx_max(cmd->clip_rect.y * gui->data.fb_scale.y, (float)0.0),
                }
            };
            vkCmdSetScissor(gui->context->graphic_queue_cmdbuffer[command_buffer_index], 0, 1, &scissor);
            vkCmdDrawIndexed(gui->context->graphic_queue_cmdbuffer[command_buffer_index], cmd->elem_count, 1, index_offset, 0, 0);
            index_offset += cmd->elem_count;
        }
        /*nk_clear(&gui->data.gui_context);*/
    }

}


void update_gui_descriptors(Gui* gui) {
    VkDescriptorImageInfo image_info[MAX_NKTEXTURES];
    for (int32_t i = 0; i < MAX_NKTEXTURES; i++) {
        image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[i].imageView = gui->data.textures[i].image_buffer.image_view;
        image_info[i].sampler = gui->data.textures[i].sampler;
    }

    VkWriteDescriptorSet descriptor_writes[1];
    descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[0].pNext = NULL;
    descriptor_writes[0].dstSet = gui->data.descriptor_set;
    descriptor_writes[0].dstBinding = 0;
    descriptor_writes[0].dstArrayElement = 0;
    descriptor_writes[0].descriptorCount = MAX_NKTEXTURES;
    descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_writes[0].pImageInfo = image_info;
    descriptor_writes[0].pBufferInfo = NULL;

    vkUpdateDescriptorSets(gui->context->device.logical_device, 1, descriptor_writes, 0, NULL);
}

NK_API void nk_font_stash_begin(Gui* gui, struct nk_font_atlas** atlas) {
    nk_font_atlas_init_default(&gui->data.atlas);
    nk_font_atlas_begin(&gui->data.atlas);
    *atlas = &gui->data.atlas;
}

NK_API void nk_font_stash_end(Gui* gui) {
    VkResult result;
    const void* image;
    int w, h;
    image = nk_font_atlas_bake(&gui->data.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    printf("w: %d, h:%d\n", w, h);
    result = create_texture_from_data(&gui->data.textures[0], VK_SAMPLER_ADDRESS_MODE_REPEAT, gui->alloc, (uint8_t*)image, w, h);
    update_gui_descriptors(gui);
    nk_font_atlas_end(&gui->data.atlas, nk_handle_id(0), &gui->data.null);
    if (gui->data.atlas.default_font) {
        nk_style_set_font(&gui->data.gui_context, &gui->data.atlas.default_font->handle);
    }
}


void destroy_gui(Gui* gui) {
    vkDeviceWaitIdle(gui->context->device.logical_device);
    clear_buffer(&gui->context->device, &gui->data.vertex_buffer);
    clear_buffer(&gui->context->device, &gui->data.index_buffer);
    for (int32_t i = 0; i < MAX_NKTEXTURES; i++) {
        clear_texture(&gui->context->device, &gui->data.textures[i]);
    }

    sx_free(gui->alloc, gui);
}
