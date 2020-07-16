#pragma once

#include <stdint.h>
#include <xcb/xcb.h>
#include <stdbool.h>
#include "device/types.h"
#include "camera.h"
#include <stdarg.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear/nuklear.h"

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
/* Vulkan function imports {{{ */
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
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties2KHR);   \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceQueueFamilyProperties);  \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR);      \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR); \
			VK_IMPORT_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR);      \
			VK_IMPORT_INSTANCE_FUNC(vkCreateDevice);                            \
			VK_IMPORT_INSTANCE_FUNC(vkDestroyDevice);                           \
			VK_IMPORT_INSTANCE_FUNC(vkDestroySurfaceKHR);                       \
			/* VK_EXT_debug_report */                                                  \
			VK_IMPORT_INSTANCE_FUNC(vkCreateDebugReportCallbackEXT);            \
			VK_IMPORT_INSTANCE_FUNC(vkDestroyDebugReportCallbackEXT);           \
			VK_IMPORT_INSTANCE_FUNC(vkDebugReportMessageEXT);                   \
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
			VK_IMPORT_DEVICE_FUNC(vkDebugMarkerSetObjectTagEXT);    \
			VK_IMPORT_DEVICE_FUNC(vkDebugMarkerSetObjectNameEXT);   \
			VK_IMPORT_DEVICE_FUNC(vkCmdDebugMarkerBeginEXT);        \
			VK_IMPORT_DEVICE_FUNC(vkCmdDebugMarkerEndEXT);          \
			VK_IMPORT_DEVICE_FUNC(vkCmdDebugMarkerInsertEXT);       \
			/* VK_EXT_debug_utils */                                       \
			VK_IMPORT_DEVICE_FUNC(vkSetDebugUtilsObjectNameEXT);    \
			VK_IMPORT_DEVICE_FUNC(vkSetDebugUtilsObjectTagEXT);     \
			VK_IMPORT_DEVICE_FUNC(vkQueueBeginDebugUtilsLabelEXT);  \
			VK_IMPORT_DEVICE_FUNC(vkQueueEndDebugUtilsLabelEXT);    \
			VK_IMPORT_DEVICE_FUNC(vkQueueInsertDebugUtilsLabelEXT); \
			VK_IMPORT_DEVICE_FUNC(vkCmdBeginDebugUtilsLabelEXT);    \
			VK_IMPORT_DEVICE_FUNC(vkCmdEndDebugUtilsLabelEXT);      \
			VK_IMPORT_DEVICE_FUNC(vkCmdInsertDebugUtilsLabelEXT);   \
			VK_IMPORT_DEVICE_FUNC(vkSubmitDebugUtilsMessageEXT);    \
/* }}} */

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

typedef struct Swapchain {
	VkSwapchainKHR swapchain;
	VkSurfaceFormatKHR format;
    VkImage images[4];
    VkImageView image_views[4];
} Swapchain;

typedef struct Device {
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
	VkSurfaceKHR presentation_surface;
} Device;

struct nk_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

typedef struct NkContext {
    struct nk_buffer cmds;
    struct nk_draw_null_texture null;
    struct nk_context gui_context;
    struct nk_font_atlas atlas;
    struct nk_vec2 fb_scale;

    Buffer vertex_buffer;
    Buffer index_buffer;
    Buffer uniform_buffer;
    Buffer staging_buffer;

    ImageBuffer font_image;
	VkSampler font_sampler;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorPool descriptor_set_pool;
	VkDescriptorSet  descriptor_set;

	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
} NkContext;

typedef struct PushConstantsBlock {
    sx_vec4 base_color_factor;
    sx_vec3 emissive_factor;
    float metallic_factor;
    float roughness_factor;
} PushConstantsBlock;

typedef struct Mesh {
    Buffer vertex_buffer;
    Buffer index_buffer;
	VkDescriptorSet  descriptor_set;
} Mesh;

typedef struct TextureSampler {
    VkFilter mag_filter;
    VkFilter min_filter;
    VkSamplerAddressMode address_modeU;
    VkSamplerAddressMode address_modeV;
    VkSamplerAddressMode address_modeW;
} TextureSampler;

typedef struct Texture {
    ImageBuffer image_buffer;
    VkSampler sampler;
} Texture;

typedef struct Materials {
    Texture* base_color_texture;
    sx_vec4* base_color_factors;
    sx_vec3* emissive_factors;
    float*   metallic_factors;
    float*   roughness_factor;
} Materials;

typedef struct MeshData {
    float* vertex_data;
    uint32_t* index_data;
    int vertex_size;
    int index_size;
} MeshData;

typedef struct Scene {
    MeshData* mesh_data; // Each mesh is a primitive
    Materials materials; // We have one material per mesh 
} Scene;


typedef struct RendererContext {
    void *vulkan_library;
	VkDebugUtilsMessengerEXT vk_debugmessenger;
    VkInstance vk_instance;

    Device device;

    uint32_t graphic_queue_index;
	uint32_t present_queue_index;
    uint32_t transfer_queue_index;
	VkQueue  vk_graphic_queue;
	VkQueue  vk_present_queue;
	VkQueue  vk_transfer_queue;
	VkCommandPool present_queue_cmdpool;
	VkCommandPool graphic_queue_cmdpool;
	VkCommandPool transfer_queue_cmdpool;
	VkCommandBuffer* present_queue_cmdbuffer;
	VkCommandBuffer* graphic_queue_cmdbuffer;
	VkCommandBuffer transfer_queue_cmdbuffer;
    VkCommandBuffer copy_cmdbuffer;

	VkSemaphore *image_available_semaphore;
	VkSemaphore *rendering_finished_semaphore;
	VkFence *fences;

    Swapchain swapchain;

    ImageBuffer depth_image;

	uint32_t rendering_resources_size;
	VkPipelineLayout pipeline_layout;
	VkRenderPass render_pass;
	VkFramebuffer* framebuffer;

    Buffer vertex_buffer;
    Buffer index_buffer;
    Buffer uniform_buffer;
	VkPipeline graphics_pipeline;
	VkDescriptorSet  descriptor_set;

    Buffer vertex_buffer_sky;
    Buffer index_buffer_sky;
    Buffer uniform_buffer_sky;
	VkDescriptorSet  descriptor_set_sky;
	VkPipeline graphics_pipeline_sky;

    Buffer staging_buffer;

    Texture texture;

    ImageBuffer cubemap;
    VkSampler cubemap_sampler;

	VkDescriptorSetLayout descriptor_set_layout;
	VkDescriptorPool descriptor_set_pool;

    uint32_t width;
    uint32_t height;
    
} RendererContext;


typedef struct VertexData {
	float x;
	float y;
	float z;
	float w;

	float u;
	float v;
} VertexData;

//bool vk_renderer_init(DeviceWindow win);
bool vk_renderer_draw(float dt);
bool vk_resize(uint32_t width, uint32_t height);
void vk_renderer_cleanup();

VkPipelineShaderStageCreateInfo load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage);
Texture create_texture(const char* filename, TextureSampler sampler, bool gen_mip);

NK_API struct nk_context* nk_gui_init();
NK_API void   nk_font_stash_begin(struct nk_font_atlas** atlas);
NK_API void   nk_font_stash_end();
NK_API void   nk_draw(VkCommandBuffer command_buffer);

