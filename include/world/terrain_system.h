#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sx/math.h"
#include "sx/allocator.h"
#include "sx/hash.h"
#include "world/types.h"
#include "world/entity_manager.h"
#include "world/renderer.h"

typedef struct TerrainTileInfo {
    float displacement_factor; // 1300
    float tessellation_factor; // 1.5
    float tessellated_edge_size; // 512
    sx_vec3 position;
    //ktx texture filenames
    char terrain_layers_texture[200];
    char terrain_heightmap_texture[200];
    char terrain_normal_texture[200];
} TerrainTileInfo;

// Each instance is a terrain tile
typedef struct TerrainInstanceData {
    unsigned size;              // Number of used entries in arrays
    unsigned capacity;          // Number of allocated entries in arrays
    void *buffer;               // Raw buffer for data.

    Entity *entity;             
    Buffer* vertex_buffer;
    Buffer* index_buffer;
    Texture* terrain_layers;
    Texture* terrain_heightmap;
    Texture* terrain_normal;
    Buffer* terrain_ubo;
	VkDescriptorSet*  descriptor_set;
} TerrainInstanceData;

typedef struct TerrainSystem {
    const sx_alloc* alloc;
    EntityManager* entity_manager;
    TerrainInstanceData data;
    RendererContext* context;
    //VkDescriptorPool terrain_descriptor_pool;
    //VkDescriptorSetLayout terrain_descriptor_layout;
    //VkPipelineLayout terrain_pipeline_layout;
    //VkPipeline terrain_pipeline;
    sx_hashtbl* table;
} TerrainSystem;

TerrainSystem* create_terrainsystem(const sx_alloc* alloc, EntityManager* em, RendererContext* context);
void terrainsystem_allocate(TerrainSystem* sm, uint32_t size);
void terrainsystem_grow(TerrainSystem* sm);
TerrainInstance create_terrain_instance(TerrainSystem* sm, Entity e, TerrainTileInfo info);
void destroy_terrainsystem(TerrainSystem* ts);

