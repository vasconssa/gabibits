#include "world/terrain_system.h"
#include "camera.h"
#include "device/vk_device.h"

#include "volk/volk.h"
/*#define VK_IMPORT_FUNC(_func) extern PFN_##_func _func*/
/*#define VK_IMPORT_INSTANCE_FUNC VK_IMPORT_FUNC*/
/*#define VK_IMPORT_DEVICE_FUNC   VK_IMPORT_FUNC*/
/*VK_IMPORT*/
/*VK_IMPORT_INSTANCE*/
/*VK_IMPORT_DEVICE*/
/*#undef VK_IMPORT_DEVICE_FUNC*/
/*#undef VK_IMPORT_INSTANCE_FUNC*/
/*#undef VK_IMPORT_FUNC*/

TerrainSystem* create_terrainsystem(const sx_alloc* alloc, EntityManager* em, RendererContext* context) {
    TerrainSystem* ts = sx_malloc(alloc, sizeof(*ts));
    ts->context = context;
    ts->alloc = alloc;
    ts->entity_manager = em;
    ts->table = sx_hashtbl_create(alloc, 1000);
    ts->data.buffer = NULL;
    ts->data.capacity = 0;
    ts->data.size = 0;

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

    if (ts->data.buffer) {
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
    }
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
    sx_free(ts->alloc, vertex_data_terrain);
    sx_free(ts->alloc, index_data_terrain);
    // Terrain descriptor set
    {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.pNext = NULL;
        descriptor_set_allocate_info.descriptorPool = ts->context->terrain_descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &ts->context->terrain_descriptor_layout;

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

void destroy_terrainsystem(TerrainSystem* ts) {
    vkDeviceWaitIdle(ts->context->device.logical_device);
    for (int32_t i = 0; i < ts->data.size; i++) {
        clear_buffer(&ts->context->device, &ts->data.vertex_buffer[i]);
        clear_buffer(&ts->context->device, &ts->data.index_buffer[i]);
        clear_buffer(&ts->context->device, &ts->data.terrain_ubo[i]);
        clear_texture(&ts->context->device, &ts->data.terrain_layers[i]);
        clear_texture(&ts->context->device, &ts->data.terrain_heightmap[i]);
        clear_texture(&ts->context->device, &ts->data.terrain_normal[i]);
    }
    sx_free(ts->alloc, ts->data.buffer);
    sx_hashtbl_destroy(ts->table, ts->alloc);
    sx_free(ts->alloc, ts);
}
