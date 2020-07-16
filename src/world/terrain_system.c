#include "world/terrain_system.h"
#include "camera.h"
#include "device/vk_device.h"

#define VK_IMPORT_FUNC(_func) extern PFN_##_func _func
#define VK_IMPORT_INSTANCE_FUNC VK_IMPORT_FUNC
#define VK_IMPORT_DEVICE_FUNC   VK_IMPORT_FUNC
VK_IMPORT
VK_IMPORT_INSTANCE
VK_IMPORT_DEVICE
#undef VK_IMPORT_DEVICE_FUNC
#undef VK_IMPORT_INSTANCE_FUNC
#undef VK_IMPORT_FUNC

TerrainSystem* create_terrainsystem(const sx_alloc* alloc, EntityManager* em, RendererContext* context) {
    TerrainSystem* ts = sx_malloc(alloc, sizeof(*ts));
    ts->context = context;
    ts->alloc = alloc;
    ts->entity_manager = em;
    ts->table = sx_hashtbl_create(alloc, 1000);
    ts->data.capacity = 0;
    ts->data.size = 0;

    // terrain pool
    {
        VkDescriptorPoolSize pool_sizes[2];
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[0].descriptorCount = 1;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_sizes[1].descriptorCount = 3;

        VkDescriptorPoolCreateInfo descriptor_pool_create_info;
        descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptor_pool_create_info.pNext = NULL;
        descriptor_pool_create_info.flags = 0;
        descriptor_pool_create_info.maxSets = 1;
        descriptor_pool_create_info.poolSizeCount = 2;
        descriptor_pool_create_info.pPoolSizes = pool_sizes;
        VkResult result;
        result = vkCreateDescriptorPool(ts->context->device.logical_device, &descriptor_pool_create_info, NULL,
                &ts->terrain_descriptor_pool);
        sx_assert_rel(result == VK_SUCCESS && "Could not create terrain descriptor pool!");
    }
    // Terrain layout
    {
        VkDescriptorSetLayoutBinding layout_bindings[4];
        layout_bindings[0].binding = 0;
        layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layout_bindings[0].descriptorCount = 1;
        layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT 
                                        | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        layout_bindings[0].pImmutableSamplers = NULL;
        //height map
        layout_bindings[1].binding = 1;
        layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_bindings[1].descriptorCount = 1;
        layout_bindings[1].stageFlags =   VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT 
                                        | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        layout_bindings[1].pImmutableSamplers = NULL;
        //texture array
        layout_bindings[2].binding = 2;
        layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_bindings[2].descriptorCount = 1;
        layout_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layout_bindings[2].pImmutableSamplers = NULL;

        layout_bindings[3].binding = 3;
        layout_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_bindings[3].descriptorCount = 1;
        layout_bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        layout_bindings[3].pImmutableSamplers = NULL;

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
        descriptor_set_layout_create_info.sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_create_info.pNext = NULL;
        descriptor_set_layout_create_info.flags = 0;
        descriptor_set_layout_create_info.bindingCount = 4;
        descriptor_set_layout_create_info.pBindings = layout_bindings;

        VkResult result;
        result = vkCreateDescriptorSetLayout(ts->context->device.logical_device,
                &descriptor_set_layout_create_info, NULL, &ts->terrain_descriptor_layout);
        sx_assert_rel(result == VK_SUCCESS && "Could not create terrain descriptor layout!");
    }
    /* Terrain pipeline {{{*/
    {
        VkVertexInputBindingDescription vertex_binding_descriptions[1] = {
            {
                .binding = 0,
                .stride  = 8 * sizeof(float),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };
        //Position, Normal, uv
		VkVertexInputAttributeDescription vertex_attribute_descriptions[3] = {
            {
                .location = 0,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0
            },
            {
                .location = 1,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 3*sizeof(float)
            },
            {
                .location = 2,
                .binding = vertex_binding_descriptions[0].binding,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 6*sizeof(float)
            }
        };

		VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = vertex_binding_descriptions,
            .vertexAttributeDescriptionCount = 3,
            .pVertexAttributeDescriptions = vertex_attribute_descriptions
        };

		VkPipelineViewportStateCreateInfo viewport_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = NULL,
            .scissorCount = 1,
            .pScissors = NULL
        };

		VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic_state_create_info = { 
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamic_states
        };

		VkPipelineRasterizationStateCreateInfo rasterization_state_create_info  = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0,
            .depthBiasClamp = 0.0,
            .depthBiasSlopeFactor = 0.0,
            .lineWidth = 1.0
        };

		VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
            .depthBoundsTestEnable = VK_FALSE,
            .minDepthBounds = 0.0,
            .maxDepthBounds = 1.0
        };

		VkPipelineColorBlendAttachmentState color_blend_attachment_state[5] = {
            {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
            {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
            {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
            {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
            {
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            },
        };

		VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .attachmentCount = 5,
            .pAttachments = color_blend_attachment_state
        };


        VkPipelineTessellationStateCreateInfo tessellation_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .patchControlPoints = 4
        };

        VkPipelineInputAssemblyStateCreateInfo terrain_assembly_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        VkGraphicsPipelineCreateInfo pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .pVertexInputState = &vertex_input_state_create_info,
            .pInputAssemblyState = &terrain_assembly_state_create_info,
            .pTessellationState = &tessellation_create_info,
            .pViewportState = &viewport_state_create_info,
            .pRasterizationState = &rasterization_state_create_info,
            .pMultisampleState = &multisample_state_create_info,
            .pDepthStencilState = &depth_stencil_state_create_info,
            .pColorBlendState = &color_blend_state_create_info,
            .pDynamicState = &dynamic_state_create_info,
            .renderPass = ts->context->render_pass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        VkDescriptorSetLayout descriptor_set_layouts[2] = {
            ts->context->global_descriptor_layout, 
            ts->terrain_descriptor_layout
        };

        VkPipelineLayoutCreateInfo layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .setLayoutCount = 2,
            .pSetLayouts = descriptor_set_layouts,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = NULL
        };

        VkResult result;
        result = vkCreatePipelineLayout(ts->context->device.logical_device, &layout_create_info, NULL,
                &ts->terrain_pipeline_layout);
        sx_assert_rel(result == VK_SUCCESS && "Could not create terrain pipeline layout!");

        pipeline_create_info.layout = ts->terrain_pipeline_layout;

        VkPipelineShaderStageCreateInfo shader_stages[4];
        shader_stages[0] = load_shader(ts->context->device.logical_device, "shaders/terrain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shader_stages[1] = load_shader(ts->context->device.logical_device, "shaders/terrain.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
        shader_stages[2] = load_shader(ts->context->device.logical_device, "shaders/terrain.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
        shader_stages[3] = load_shader(ts->context->device.logical_device, "shaders/terrain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

        pipeline_create_info.stageCount = 4;
        pipeline_create_info.pStages = shader_stages;

        result = vkCreateGraphicsPipelines(ts->context->device.logical_device, VK_NULL_HANDLE, 1,
                &pipeline_create_info, NULL, &ts->terrain_pipeline);
        sx_assert_rel(result == VK_SUCCESS && "Could not create terrain pipeline!");
        vkDestroyShaderModule(ts->context->device.logical_device, shader_stages[0].module, NULL);
        vkDestroyShaderModule(ts->context->device.logical_device, shader_stages[1].module, NULL);
        vkDestroyShaderModule(ts->context->device.logical_device, shader_stages[2].module, NULL);
        vkDestroyShaderModule(ts->context->device.logical_device, shader_stages[3].module, NULL);
    }
    /*}}}*/

    return ts;
}

void terrainsystem_allocate(TerrainSystem* ts, uint32_t size) {
    sx_assert_rel(size > ts->data.size);

    TerrainInstanceData new_data;
    uint32_t bytes = size * (  sizeof(Entity) + alignof(Entity)
                             + sizeof(Buffer) + alignof(Buffer)
                             + sizeof(Buffer) + alignof(Buffer)
                             + sizeof(Buffer) + alignof(Buffer)
                             + sizeof(Texture) + alignof(Texture)
                             + sizeof(Texture) + alignof(Texture)
                             + sizeof(Texture) + alignof(Texture)
                             + sizeof(VkDescriptorSet) + alignof(VkDescriptorSet) );
    new_data.buffer = sx_malloc(ts->alloc, bytes);
    new_data.size = ts->data.size;
    new_data.capacity = size;

    new_data.entity = (Entity*)sx_align_ptr((void*)(new_data.buffer), 0, alignof(Entity));
    new_data.vertex_buffer = (Buffer*)sx_align_ptr((void*)(new_data.entity + size), 0, alignof(Buffer));
    new_data.index_buffer = (Buffer*)sx_align_ptr((void*)(new_data.vertex_buffer + size), 0, alignof(Buffer));
    new_data.terrain_ubo  = (Buffer*)sx_align_ptr((void*)(new_data.index_buffer + size), 0, alignof(Buffer));
    new_data.terrain_layers = (Texture*)sx_align_ptr((void*)(new_data.terrain_ubo + size), 0, alignof(Texture));
    new_data.terrain_heightmap = (Texture*)sx_align_ptr((void*)(new_data.terrain_layers + size), 0, alignof(Texture));
    new_data.terrain_normal = (Texture*)sx_align_ptr((void*)(new_data.terrain_heightmap + size), 0, alignof(Texture));
    new_data.descriptor_set = (VkDescriptorSet*)sx_align_ptr((void*)(new_data.terrain_normal + size), 0, alignof(VkDescriptorSet));

    sx_memcpy(new_data.entity, ts->data.entity, ts->data.size * sizeof(Entity));
    sx_memcpy(new_data.vertex_buffer, ts->data.vertex_buffer, ts->data.size * sizeof(Buffer));
    sx_memcpy(new_data.index_buffer, ts->data.index_buffer, ts->data.size * sizeof(Buffer));
    sx_memcpy(new_data.terrain_ubo, ts->data.terrain_ubo, ts->data.size * sizeof(Buffer));
    sx_memcpy(new_data.terrain_layers, ts->data.terrain_layers, ts->data.size * sizeof(Texture));
    sx_memcpy(new_data.terrain_heightmap, ts->data.terrain_heightmap, ts->data.size * sizeof(Texture));
    sx_memcpy(new_data.terrain_normal, ts->data.terrain_normal, ts->data.size * sizeof(Texture));
    sx_memcpy(new_data.descriptor_set, ts->data.descriptor_set, ts->data.size * sizeof(VkDescriptorSet));
    /*sx_memcpy(new_data.index_offsets, ts->data.index_offsets, ts->data.size * sizeof(uint32_t));*/

    sx_free(ts->alloc, ts->data.buffer);
    ts->data = new_data;
}

void terrainsystem_grow(TerrainSystem* ts) {
    terrainsystem_allocate(ts, ts->data.capacity * 2 + 1);
}

enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
TerrainInstance create_terrain_instance(TerrainSystem* ts, Entity e, TerrainTileInfo info) {
    if (ts->data.capacity == ts->data.size) {
        terrainsystem_grow(ts);
    }

    uint32_t last = ts->data.size;
    ts->data.size++;

    ts->data.entity[last] = e;

    typedef struct Vertex {
        sx_vec3 pos;
        sx_vec3 normal;
        sx_vec2 uv;
    } Vertex;

    float mul = 1.0 * 64.0;

    const uint32_t patch_size =  64;
    const uint32_t uv_scale = 1.0;
    const float wx = mul * 1.0;
    const float wy = mul * 1.0;
    const uint32_t w = patch_size - 1;
    const uint32_t index_count = w * w * 4;
    Vertex *vertex_data_terrain = sx_malloc(ts->alloc, patch_size * patch_size * sizeof(*vertex_data_terrain));
    uint32_t *index_data_terrain = sx_malloc(ts->alloc, index_count * sizeof(*index_data_terrain));

    for (uint32_t x = 0; x < patch_size; x++) {
        for (uint32_t y = 0; y < patch_size; y++) {
            uint32_t index = x + y * patch_size;
            vertex_data_terrain[index].pos.x = x * wx + wx / 2.0 - (float)patch_size * wx / 2.0;
            vertex_data_terrain[index].pos.y = 0.0;
            vertex_data_terrain[index].pos.z = y * wy + wy / 2.0 - (float)patch_size * wy / 2.0;

            vertex_data_terrain[index].normal.x = 0.0;
            vertex_data_terrain[index].normal.y = 1.0;
            vertex_data_terrain[index].normal.z = 0.0;

            vertex_data_terrain[index].uv.x = (float)x / patch_size * uv_scale;
            vertex_data_terrain[index].uv.y = 1.0 - (float)y / patch_size * uv_scale;
        }
    }

    for (uint32_t x = 0; x < w; x++) {
        for (uint32_t y = 0; y < w; y++) {
            uint32_t index = (x + y * w) * 4;
            index_data_terrain[index] = x + y * patch_size;
            index_data_terrain[index + 1] = index_data_terrain[index] + 1;
            index_data_terrain[index + 2] = index_data_terrain[index + 1] + patch_size;
            index_data_terrain[index + 3] = index_data_terrain[index] + patch_size;
        }
    }

    VkResult result;
    result = create_buffer(&ts->data.vertex_buffer[last], 
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, patch_size * patch_size * sizeof(Vertex));

    sx_assert_rel(result == VK_SUCCESS && "Could not create terrain vertex buffer!");
    result = copy_buffer_staged(&ts->data.vertex_buffer[last], vertex_data_terrain, patch_size * patch_size * sizeof(Vertex));
    sx_assert_rel(result == VK_SUCCESS && "Could not copy terrain vertex buffer!");

    result = create_buffer(&ts->data.index_buffer[last],
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_count * sizeof(uint32_t));
    sx_assert_rel(result == VK_SUCCESS && "Could not create terrain index buffer!");
    result = copy_buffer_staged(&ts->data.index_buffer[last], index_data_terrain, index_count * sizeof(uint32_t));
    sx_assert_rel(result == VK_SUCCESS && "Could not copy terrain index buffer!");

    create_texture(&ts->data.terrain_heightmap[last], VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, ts->alloc, info.terrain_heightmap_texture);
    create_texture(&ts->data.terrain_normal[last], VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, ts->alloc, info.terrain_normal_texture);
    create_texture(&ts->data.terrain_layers[last], VK_SAMPLER_ADDRESS_MODE_REPEAT, ts->alloc, info.terrain_layers_texture);
    result = create_buffer(&ts->data.terrain_ubo[last],
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(TerrainUBO));
    sx_assert_rel(result == VK_SUCCESS && "Could not create terrain uniform buffer!");
    // Terrain descriptor set
    {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.pNext = NULL;
        descriptor_set_allocate_info.descriptorPool = ts->terrain_descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &ts->terrain_descriptor_layout;

        result = vkAllocateDescriptorSets(ts->context->device.logical_device, &descriptor_set_allocate_info,
                &ts->data.descriptor_set[last]);
        sx_assert_rel("Could not create terrain descriptor set!");
    }
    /* Terrain Descriptor update {{{*/
    {
        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer = ts->data.terrain_ubo[last].buffer;
        buffer_info.offset = 0;
        buffer_info.range = ts->data.terrain_ubo[last].size;

        VkDescriptorImageInfo image_info[3];
        image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[0].imageView = ts->data.terrain_heightmap[last].image_buffer.image_view;
        image_info[0].sampler = ts->data.terrain_heightmap[last].sampler;
        image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[1].imageView = ts->data.terrain_layers[last].image_buffer.image_view;
        image_info[1].sampler = ts->data.terrain_layers[last].sampler;
        image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[2].imageView = ts->data.terrain_normal[last].image_buffer.image_view;
        image_info[2].sampler = ts->data.terrain_normal[last].sampler;

        VkWriteDescriptorSet descriptor_writes[4];
        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].pNext = NULL;
        descriptor_writes[0].dstSet = ts->data.descriptor_set[last];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_writes[0].pImageInfo = NULL;
        descriptor_writes[0].pBufferInfo = &buffer_info;

        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].pNext = NULL;
        descriptor_writes[1].dstSet = ts->data.descriptor_set[last];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].pImageInfo = &image_info[0];
        descriptor_writes[1].pBufferInfo = NULL;

        descriptor_writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[2].pNext = NULL;
        descriptor_writes[2].dstSet = ts->data.descriptor_set[last];
        descriptor_writes[2].dstBinding = 2;
        descriptor_writes[2].dstArrayElement = 0;
        descriptor_writes[2].descriptorCount = 1;
        descriptor_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[2].pImageInfo = &image_info[1];
        descriptor_writes[2].pBufferInfo = NULL;

        descriptor_writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[3].pNext = NULL;
        descriptor_writes[3].dstSet = ts->data.descriptor_set[last];
        descriptor_writes[3].dstBinding = 3;
        descriptor_writes[3].dstArrayElement = 0;
        descriptor_writes[3].descriptorCount = 1;
        descriptor_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[3].pImageInfo = &image_info[2];
        descriptor_writes[3].pBufferInfo = NULL;

        vkUpdateDescriptorSets(ts->context->device.logical_device, 4, descriptor_writes, 0, NULL);
    }
    /*}}}*/

    sx_mat4 projection = perspective_mat((Camera*)&device()->camera);
    sx_mat4 view = view_mat((Camera*)&device()->camera);

    sx_vec4 planes[6];
    sx_mat4 matrix = sx_mat4_mul(&projection, &view);

    planes[LEFT].x = matrix.col1.w + matrix.col1.x;
    planes[LEFT].y = matrix.col2.w + matrix.col2.x;
    planes[LEFT].z = matrix.col3.w + matrix.col3.x;
    planes[LEFT].w = matrix.col4.w + matrix.col4.x;

    planes[RIGHT].x = matrix.col1.w - matrix.col1.x;
    planes[RIGHT].y = matrix.col2.w - matrix.col2.x;
    planes[RIGHT].z = matrix.col3.w - matrix.col3.x;
    planes[RIGHT].w = matrix.col4.w - matrix.col4.x;

    planes[TOP].x = matrix.col1.w - matrix.col1.y;
    planes[TOP].y = matrix.col2.w - matrix.col2.y;
    planes[TOP].z = matrix.col3.w - matrix.col3.y;
    planes[TOP].w = matrix.col4.w - matrix.col4.y;

    planes[BOTTOM].x = matrix.col1.w + matrix.col1.y;
    planes[BOTTOM].y = matrix.col2.w + matrix.col2.y;
    planes[BOTTOM].z = matrix.col3.w + matrix.col3.y;
    planes[BOTTOM].w = matrix.col4.w + matrix.col4.y;

    planes[BACK].x = matrix.col1.w + matrix.col1.z;
    planes[BACK].y = matrix.col2.w + matrix.col2.z;
    planes[BACK].z = matrix.col3.w + matrix.col3.z;
    planes[BACK].w = matrix.col4.w + matrix.col4.z;

    planes[FRONT].x = matrix.col1.w - matrix.col1.z;
    planes[FRONT].y = matrix.col2.w - matrix.col2.z;
    planes[FRONT].z = matrix.col3.w - matrix.col3.z;
    planes[FRONT].w = matrix.col4.w - matrix.col4.z;

    for (int i = 0; i < 6; i++) {
        float length = sx_sqrt(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
        planes[i].x /= length;
        planes[i].y /= length;
        planes[i].z /= length;
        planes[i].w /= length;
    }
    TerrainUBO terrain_ubo;
    sx_memcpy(terrain_ubo.frustum_planes, planes, 6 * sizeof(sx_vec4));
    terrain_ubo.viewport_dimensions.x = ts->context->width;
    terrain_ubo.viewport_dimensions.y = ts->context->height;
    terrain_ubo.displacement_factor = info.displacement_factor;
    terrain_ubo.tessellation_factor = info.tessellation_factor;
    terrain_ubo.tessellated_edge_size = info.tessellated_edge_size;
    copy_buffer(&ts->data.terrain_ubo[last],  &terrain_ubo, sizeof(terrain_ubo));

    sx_hashtbl_add(ts->table, sx_hash_u32(e.handle), last);

    TerrainInstance inst;
    inst.i = last;
    return inst;
}

void destroy_terrain_insntace(TerrainSystem* sm, Entity e, TerrainInstance i) {
}

