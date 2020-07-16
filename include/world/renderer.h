#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "macros.h"
#include "sx/math.h"
#include "sx/allocator.h"
#include "sx/hash.h"
#include "device/types.h"
#include "world/types.h"
#include "world/entity_manager.h"

#if SX_PLATFORM_ANDROID
#	define VK_USE_PLATFORM_ANDROID_KHR
#	define KHR_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#	define VK_IMPORT_INSTANCE_PLATFORM VK_IMPORT_INSTANCE_ANDROID
#elif SX_PLATFORM_LINUX
#   define VK_USE_PLATFORM_XCB_KHR
#	define KHR_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#	define VK_IMPORT_INSTANCE_PLATFORM VK_IMPORT_INSTANCE_LINUX
#elif SX_PLATFORM_WINDOWS
#	define VK_USE_PLATFORM_WIN32_KHR
#	define KHR_SURFACE_EXTENSION_NAME  VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#	define VK_IMPORT_INSTANCE_PLATFORM VK_IMPORT_INSTANCE_WINDOWS
#else
#	define KHR_SURFACE_EXTENSION_NAME ""
#	define VK_IMPORT_INSTANCE_PLATFORM
#endif // BX_PLATFORM_*

#define VK_NO_STDINT_H
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define DEBUG_RENDERER

/* Vulkan Funcions imports {{{ */
#define VK_IMPORT                                                          \
			VK_IMPORT_FUNC(vkCreateInstance);                       \
			VK_IMPORT_FUNC(vkGetInstanceProcAddr);                  \
			VK_IMPORT_FUNC(vkGetDeviceProcAddr);                    \
			VK_IMPORT_FUNC(vkEnumerateInstanceExtensionProperties); \
			VK_IMPORT_FUNC(vkEnumerateInstanceLayerProperties);     \

#define VK_IMPORT_INSTANCE_ANDROID \
			VK_IMPORT_INSTANCE_FUNC(vkCreateAndroidSurfaceKHR);

#define VK_IMPORT_INSTANCE_LINUX                                                           \
			VK_IMPORT_INSTANCE_FUNC(vkCreateXcbSurfaceKHR);                         \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceXcbPresentationSupportKHR);  


#define VK_IMPORT_INSTANCE_WINDOWS \
			VK_IMPORT_INSTANCE_FUNC(vkCreateWin32SurfaceKHR); \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceWin32PresentationSupportKHR); \


#define VK_IMPORT_INSTANCE                                                             \
			VK_IMPORT_INSTANCE_FUNC(vkDestroyInstance);                         \
			VK_IMPORT_INSTANCE_FUNC(vkEnumeratePhysicalDevices);                \
			VK_IMPORT_INSTANCE_FUNC(vkEnumerateDeviceExtensionProperties);      \
			VK_IMPORT_INSTANCE_FUNC(vkEnumerateDeviceLayerProperties);          \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceProperties);             \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceFormatProperties);       \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceFeatures);               \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceImageFormatProperties);  \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties);       \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceQueueFamilyProperties);  \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR);      \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR); \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR);      \
			VK_IMPORT_INSTANCE_FUNC(vkCreateDevice);                            \
			VK_IMPORT_INSTANCE_FUNC(vkDestroyDevice);                           \
			VK_IMPORT_INSTANCE_FUNC(vkDestroySurfaceKHR);                       \
			/* VK_EXT_debug_report */                                                  \
			VK_IMPORT_INSTANCE_FUNC(vkCreateDebugUtilsMessengerEXT);  \
			VK_IMPORT_INSTANCE_FUNC(vkDestroyDebugUtilsMessengerEXT); \
			VK_IMPORT_INSTANCE_PLATFORM                                         \

#define VK_IMPORT_DEVICE                                                   \
			VK_IMPORT_DEVICE_FUNC(vkGetDeviceQueue);                \
			VK_IMPORT_DEVICE_FUNC(vkCreateSwapchainKHR);            \
			VK_IMPORT_DEVICE_FUNC(vkDestroySwapchainKHR);           \
			VK_IMPORT_DEVICE_FUNC(vkGetSwapchainImagesKHR);         \
			VK_IMPORT_DEVICE_FUNC(vkAcquireNextImageKHR);           \
			VK_IMPORT_DEVICE_FUNC(vkQueuePresentKHR);               \
			VK_IMPORT_DEVICE_FUNC(vkCreateFence);                   \
			VK_IMPORT_DEVICE_FUNC(vkDestroyFence);                  \
			VK_IMPORT_DEVICE_FUNC(vkCreateSemaphore);               \
			VK_IMPORT_DEVICE_FUNC(vkDestroySemaphore);              \
			VK_IMPORT_DEVICE_FUNC(vkResetFences);                   \
			VK_IMPORT_DEVICE_FUNC(vkCreateCommandPool);             \
			VK_IMPORT_DEVICE_FUNC(vkDestroyCommandPool);            \
			VK_IMPORT_DEVICE_FUNC(vkResetCommandPool);              \
			VK_IMPORT_DEVICE_FUNC(vkAllocateCommandBuffers);        \
			VK_IMPORT_DEVICE_FUNC(vkFreeCommandBuffers);            \
			VK_IMPORT_DEVICE_FUNC(vkGetBufferMemoryRequirements);   \
			VK_IMPORT_DEVICE_FUNC(vkGetImageMemoryRequirements);    \
			VK_IMPORT_DEVICE_FUNC(vkAllocateMemory);                \
			VK_IMPORT_DEVICE_FUNC(vkFreeMemory);                    \
			VK_IMPORT_DEVICE_FUNC(vkCreateImage);                   \
			VK_IMPORT_DEVICE_FUNC(vkDestroyImage);                  \
			VK_IMPORT_DEVICE_FUNC(vkCreateImageView);               \
			VK_IMPORT_DEVICE_FUNC(vkDestroyImageView);              \
			VK_IMPORT_DEVICE_FUNC(vkCreateBuffer);                  \
			VK_IMPORT_DEVICE_FUNC(vkDestroyBuffer);                 \
			VK_IMPORT_DEVICE_FUNC(vkCreateFramebuffer);             \
			VK_IMPORT_DEVICE_FUNC(vkDestroyFramebuffer);            \
			VK_IMPORT_DEVICE_FUNC(vkCreateRenderPass);              \
			VK_IMPORT_DEVICE_FUNC(vkDestroyRenderPass);             \
			VK_IMPORT_DEVICE_FUNC(vkCreateShaderModule);            \
			VK_IMPORT_DEVICE_FUNC(vkDestroyShaderModule);           \
			VK_IMPORT_DEVICE_FUNC(vkCreatePipelineCache);           \
			VK_IMPORT_DEVICE_FUNC(vkDestroyPipelineCache);          \
			VK_IMPORT_DEVICE_FUNC(vkGetPipelineCacheData);          \
			VK_IMPORT_DEVICE_FUNC(vkMergePipelineCaches);           \
			VK_IMPORT_DEVICE_FUNC(vkCreateGraphicsPipelines);       \
			VK_IMPORT_DEVICE_FUNC(vkCreateComputePipelines);        \
			VK_IMPORT_DEVICE_FUNC(vkDestroyPipeline);               \
			VK_IMPORT_DEVICE_FUNC(vkCreatePipelineLayout);          \
			VK_IMPORT_DEVICE_FUNC(vkDestroyPipelineLayout);         \
			VK_IMPORT_DEVICE_FUNC(vkCreateSampler);                 \
			VK_IMPORT_DEVICE_FUNC(vkDestroySampler);                \
			VK_IMPORT_DEVICE_FUNC(vkCreateDescriptorSetLayout);     \
			VK_IMPORT_DEVICE_FUNC(vkDestroyDescriptorSetLayout);    \
			VK_IMPORT_DEVICE_FUNC(vkCreateDescriptorPool);          \
			VK_IMPORT_DEVICE_FUNC(vkDestroyDescriptorPool);         \
			VK_IMPORT_DEVICE_FUNC(vkResetDescriptorPool);           \
			VK_IMPORT_DEVICE_FUNC(vkAllocateDescriptorSets);        \
			VK_IMPORT_DEVICE_FUNC(vkFreeDescriptorSets);            \
			VK_IMPORT_DEVICE_FUNC(vkUpdateDescriptorSets);          \
			VK_IMPORT_DEVICE_FUNC(vkQueueSubmit);                   \
			VK_IMPORT_DEVICE_FUNC(vkQueueWaitIdle);                 \
			VK_IMPORT_DEVICE_FUNC(vkDeviceWaitIdle);                \
			VK_IMPORT_DEVICE_FUNC(vkWaitForFences);                 \
			VK_IMPORT_DEVICE_FUNC(vkBeginCommandBuffer);            \
			VK_IMPORT_DEVICE_FUNC(vkEndCommandBuffer);              \
			VK_IMPORT_DEVICE_FUNC(vkCmdPipelineBarrier);            \
			VK_IMPORT_DEVICE_FUNC(vkCmdBeginRenderPass);            \
			VK_IMPORT_DEVICE_FUNC(vkCmdEndRenderPass);              \
			VK_IMPORT_DEVICE_FUNC(vkCmdNextSubpass);              \
			VK_IMPORT_DEVICE_FUNC(vkCmdSetViewport);                \
			VK_IMPORT_DEVICE_FUNC(vkCmdPushConstants);              \
			VK_IMPORT_DEVICE_FUNC(vkCmdDraw);                       \
			VK_IMPORT_DEVICE_FUNC(vkCmdDrawIndexed);                \
			VK_IMPORT_DEVICE_FUNC(vkCmdDrawIndirect);               \
			VK_IMPORT_DEVICE_FUNC(vkCmdDrawIndexedIndirect);        \
			VK_IMPORT_DEVICE_FUNC(vkCmdDispatch);                   \
			VK_IMPORT_DEVICE_FUNC(vkCmdDispatchIndirect);           \
			VK_IMPORT_DEVICE_FUNC(vkCmdBindPipeline);               \
			VK_IMPORT_DEVICE_FUNC(vkCmdSetStencilReference);        \
			VK_IMPORT_DEVICE_FUNC(vkCmdSetBlendConstants);          \
			VK_IMPORT_DEVICE_FUNC(vkCmdSetScissor);                 \
			VK_IMPORT_DEVICE_FUNC(vkCmdBindDescriptorSets);         \
			VK_IMPORT_DEVICE_FUNC(vkCmdBindIndexBuffer);            \
			VK_IMPORT_DEVICE_FUNC(vkCmdBindVertexBuffers);          \
			VK_IMPORT_DEVICE_FUNC(vkCmdUpdateBuffer);               \
			VK_IMPORT_DEVICE_FUNC(vkCmdClearColorImage);            \
			VK_IMPORT_DEVICE_FUNC(vkCmdClearDepthStencilImage);     \
			VK_IMPORT_DEVICE_FUNC(vkCmdClearAttachments);           \
			VK_IMPORT_DEVICE_FUNC(vkCmdResolveImage);               \
			VK_IMPORT_DEVICE_FUNC(vkCmdCopyBuffer);                 \
			VK_IMPORT_DEVICE_FUNC(vkCmdCopyBufferToImage);          \
			VK_IMPORT_DEVICE_FUNC(vkCmdBlitImage);                  \
			VK_IMPORT_DEVICE_FUNC(vkMapMemory);                     \
			VK_IMPORT_DEVICE_FUNC(vkUnmapMemory);                   \
			VK_IMPORT_DEVICE_FUNC(vkFlushMappedMemoryRanges);       \
			VK_IMPORT_DEVICE_FUNC(vkInvalidateMappedMemoryRanges);  \
			VK_IMPORT_DEVICE_FUNC(vkBindBufferMemory);              \
			VK_IMPORT_DEVICE_FUNC(vkBindImageMemory);               \
			/* VK_EXT_debug_marker */                                      \
			/* VK_EXT_debug_utils */                                       \
			VK_IMPORT_DEVICE_FUNC(vkSetDebugUtilsObjectNameEXT);    \
			VK_IMPORT_DEVICE_FUNC(vkSetDebugUtilsObjectTagEXT);     \
			VK_IMPORT_DEVICE_FUNC(vkQueueBeginDebugUtilsLabelEXT);  \
			VK_IMPORT_DEVICE_FUNC(vkQueueEndDebugUtilsLabelEXT);    \
			VK_IMPORT_DEVICE_FUNC(vkQueueInsertDebugUtilsLabelEXT); \
			VK_IMPORT_DEVICE_FUNC(vkCmdBeginDebugUtilsLabelEXT);    \
			VK_IMPORT_DEVICE_FUNC(vkCmdEndDebugUtilsLabelEXT);      \
			VK_IMPORT_DEVICE_FUNC(vkCmdInsertDebugUtilsLabelEXT);   \

/* }}} */

typedef struct MeshData {
    uint32_t vertex_size;
    uint32_t index_size;
    uint32_t num_primitives;
    uint32_t* index_data;
    uint32_t* vertex_offsets;
    uint32_t* index_offsets;
    uint32_t* material_indices;
    float* vertex_data;
} MeshData;

typedef struct TextureNames {
    char names[200];
} TextureNames;

typedef struct MaterialsData {
    uint32_t num_materials;
    //uint32_t* base_color_texture_indices;
    TextureNames* base_color_texture_names;
    TextureNames* metallic_roughness_texture_names;
    TextureNames* normal_texture_names;
    TextureNames* occlusion_texture_names;
    TextureNames* emissive_texture_names;
    float*   metallic_factors;
    float*   roughness_factors;
    sx_vec4* base_color_factors;
    sx_vec3* emissive_factors;
} MaterialsData;


typedef struct TextureData2D {
    uint32_t width;
    uint32_t height;
    uint32_t num_mips;
    uint8_t* data;
} TextureData2D;

typedef struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    uint32_t size;
} Buffer;

typedef struct ImageBuffer {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView image_view;
    VkFormat format;
} ImageBuffer;

typedef struct Texture {
    ImageBuffer image_buffer;
    VkSampler sampler;
} Texture;

typedef struct ObjectsUBO {
    sx_mat4 model;
    sx_vec4 base_color;
    sx_vec4 metallic_roughness;
} ObjectsUBO;

typedef struct PbrMaterials {
    unsigned size;              ///< Number of used entries in arrays
    unsigned capacity;          ///< Number of allocated entries in arrays
    void *buffer;               ///< Raw buffer for data.

    Texture* base_color_texture;
    Texture* metallic_roughness_texture;
    Texture* normal_texture;
    Texture* occlusion_texture;
    Texture* emissive_texture;
    Buffer* object_ubos;
} PbrMaterials;

typedef struct PrimitiveData {
    uint32_t* vertex_offsets;
    uint32_t* index_offsets;
    uint32_t* material_indices;
} PrimitiveData;

typedef struct MeshInstanceData {
    unsigned size;              ///< Number of used entries in arrays
    unsigned capacity;          ///< Number of allocated entries in arrays
    void *buffer;               ///< Raw buffer for data.

    Entity *entity;             ///< The entity owning this instance.
    Buffer* vertex_buffer;
    Buffer* index_buffer;
	VkDescriptorSet*  descriptor_set;
} MeshInstanceData;

typedef struct Renderer {
    const sx_alloc* alloc;
    EntityManager* entity_manager;
    PbrMaterials materials;
    MeshInstanceData data;
    sx_hashtbl* table;
} Renderer;

bool vk_renderer_init(DeviceWindow win);
bool vk_resize(uint32_t width, uint32_t height);
void vk_renderer_cleanup();

Renderer* create_renderer(const sx_alloc* alloc, EntityManager* em);
void renderer_allocate(Renderer* rd, uint32_t size);
void renderer_grow(Renderer* rd);
void renderer_prepare(Renderer* rd);
void renderer_render(Renderer* rd);
void init_pbr_materials(Renderer* rd, MaterialsData* material_data, const char* path);
MeshInstance create_mesh_instance(Renderer* rd, Entity e, MeshData* mesh_data);
void update_transform(Renderer* rd, Entity e, sx_mat4* mat);
void destroy_mesh_instance(Renderer* rd, Entity e, MeshInstance i);
bool renderer_lookup(Renderer* rd, Entity e);
void destroy_renderer(Renderer* rd);
//void transform(sx_mat4* parent, TransformInstance i);

