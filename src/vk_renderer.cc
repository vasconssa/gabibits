/* Includes {{{ */
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
#include "vk_renderer.h"
#include <math.h>
#include "math/math.h"
#include "sx/math.h"
#include "sx/os.h"
#include "device/vk_device.h"
#include "vulkan/vulkan_core.h"
#include "resource/gltf_importer.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define WIDTH 800
#define HEIGHT 800
#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define VK_IMPORT_FUNC(_func) PFN_##_func _func
#define VK_IMPORT_INSTANCE_FUNC VK_IMPORT_FUNC
#define VK_IMPORT_DEVICE_FUNC   VK_IMPORT_FUNC
VK_IMPORT
VK_IMPORT_INSTANCE
VK_IMPORT_DEVICE
#undef VK_IMPORT_DEVICE_FUNC
#undef VK_IMPORT_INSTANCE_FUNC
#undef VK_IMPORT_FUNC
/*}}}*/

static RendererContext vk_context;
static NkContext nk_gui;


static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			   VkDebugUtilsMessageTypeFlagsEXT messageType,
			   const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
			   void* p_user_data) {
	printf("Validation layer: %s\n", p_callback_data->pMessage);

	return VK_FALSE;
}

uint32_t get_memory_type(VkMemoryRequirements image_memory_requirements, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(vk_context.device.physical_device, &memory_properties);
    uint32_t index = UINT32_MAX;
    for(uint32_t k = 0; k < memory_properties.memoryTypeCount; k++) {
        if((image_memory_requirements.memoryTypeBits & (1<<k)) &&
           (memory_properties.memoryTypes[k].propertyFlags &
            properties)) {
            index = k;
        }
    }
    return index;
}

VkResult create_buffer(VkPhysicalDevice physical_device, VkDevice device, Buffer* buffer, VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags memory_properties_flags, VkDeviceSize size);

VkResult create_image(Device* device, ImageBuffer* image, 
                        uint32_t width, uint32_t height, 
                        VkImageUsageFlags usage, VkImageAspectFlags aspect, uint32_t mip_levels);
VkResult create_cubemap();

VkResult create_swapchain( uint32_t width, uint32_t height) ;
VkResult copy_image_staged(ImageBuffer* dst_buffer, Buffer* staging_buffer, VkImageAspectFlags aspect,
                            void* data, uint32_t width, uint32_t height);

VkResult copy_buffer_staged(Buffer* dst_buffer, Buffer* staging_buffer, void* data, uint32_t size);

typedef struct Ubo {
    sx_mat4 model;
    sx_mat4 view;
    sx_mat4 projection;
} Ubo;

/* update uniform buffer */
bool update_uniform_buffer(Vector3 tranlation, float dt) {
        static float r = 0;
        Ubo ubo;
        sx_mat4 r_matrix = sx_mat4_rotateXYZ(sx_torad(90), sx_torad(180), sx_torad(180));
        sx_mat4 t_matrix = sx_mat4_translate(tranlation.x, tranlation.y, tranlation.z);
        r += 10*dt;
        r_matrix = sx_mat4_ident();
        ubo.model = sx_mat4_mul(&t_matrix, &r_matrix);
        /*orth_proj_matrix(proj_matrix, -WIDTH/2, WIDTH/2, -HEIGHT/2, HEIGHT/2, 1000, -1000);*/
        ubo.projection = perspective_mat((Camera*)&device()->camera);
        /*ubo.view = sx_mat4_ident();*/
        ubo.view = view_mat((Camera*)&device()->camera);
        /*memcpy(&ubo.view, &vk_context.camera->view, 16 * sizeof(float));*/
        /*mat4_transpose(res_matrix, m);*/
        copy_buffer_staged(&vk_context.uniform_buffer, 
                &vk_context.staging_buffer, &ubo, sizeof(ubo));
        return true;
}

bool update_uniform_buffer_sky(Vector3 tranlation, float dt) {
        Ubo ubo;
        ubo.model = sx_mat4_ident();
        /*orth_proj_matrix(proj_matrix, -WIDTH/2, WIDTH/2, -HEIGHT/2, HEIGHT/2, 1000, -1000);*/
        ubo.projection = perspective_mat((Camera*)&device()->camera);
        /*ubo.view = sx_mat4_ident();*/
        ubo.view = view_mat((Camera*)&device()->camera);
        ubo.view.col4 = SX_VEC4_ZERO;
        /*memcpy(&ubo.view, &vk_context.camera->view, 16 * sizeof(float));*/
        /*mat4_transpose(res_matrix, m);*/
        copy_buffer_staged(&vk_context.uniform_buffer_sky, 
                &vk_context.staging_buffer, &ubo, sizeof(ubo));
        return true;
}

Scene scene = {};
MeshData generate_vertex_data();

bool vk_renderer_init(DeviceWindow win) {
    vk_context.width = win.width;
    vk_context.height = win.height;
    vk_context.vulkan_library = sx_os_dlopen("libvulkan.so");
    if (vk_context.vulkan_library != NULL)
        printf("Library open with success\n");

    bool imported = true;

#define VK_IMPORT_FUNC( _func )                               \
    _func = (PFN_##_func)dlsym(vk_context.vulkan_library, #_func); \
    printf(#_func); printf("\n");                                   \
    imported &= NULL != _func;
VK_IMPORT
#undef VK_IMPORT_FUNC

    if (imported)
    {
        printf("Success \n");
    }


	uint32_t num_layers = 1;

	const char *validation_layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};

	uint32_t num_extensions = 3;

	const char *extensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
		VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
	};

	VkResult result;
	/* Instance Creation {{{ */
	{
		uint32_t layer_count;
		vkEnumerateInstanceLayerProperties(&layer_count, NULL);
		VkLayerProperties *layer_properties;
		layer_properties = (VkLayerProperties *)malloc(layer_count * sizeof(VkLayerProperties));
		vkEnumerateInstanceLayerProperties(&layer_count, layer_properties);
		for (uint32_t i = 0; i < num_layers; i++) {
			bool has_layer = false;
			for (uint32_t j = 0; j < layer_count; j++) {
				if (strcmp(validation_layers[i], layer_properties[j].layerName) == 0) {
					printf("Layer %s found\n", layer_properties[j].layerName);
					has_layer = true;
					break;
				}
			}
			if (!has_layer) {
				printf("Layer %s not found\n", validation_layers[i]);
				return false;
			}
		}
		free(layer_properties);


		uint32_t extension_count = 0;
		vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
		VkExtensionProperties *extension_properties;
		extension_properties = (VkExtensionProperties*)malloc(extension_count * sizeof(VkExtensionProperties));
		vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extension_properties);

		for (uint32_t i = 0; i < num_extensions; i++)
		{
			bool has_extension = false;
			for(uint32_t j = 0; j < extension_count; j++)
			{
			   if (strcmp(extensions[i], extension_properties[j].extensionName) == 0)
			   {
				   printf("Extension %s found\n", extensions[i]);
				   has_extension = true;
				   break;
			   }
			}
			if (!has_extension)
			{
			   printf("Extension %s not found\n", extensions[i]);
			   return false;
			}
		}
		free(extension_properties);

		VkDebugUtilsMessengerCreateInfoEXT dbg_create_info;
		dbg_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbg_create_info.flags = 0;
		dbg_create_info.pNext = NULL;
		dbg_create_info.pUserData = NULL;
		dbg_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
										  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
										  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dbg_create_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
										  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
										  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		dbg_create_info.pfnUserCallback = debug_callback;


		VkApplicationInfo app_info;
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = NULL;
		app_info.pApplicationName = "vasconssa";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "vasconssa";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo inst_create_info;
		inst_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&dbg_create_info;
		inst_create_info.flags = 0;
		inst_create_info.pApplicationInfo = &app_info;
		inst_create_info.enabledLayerCount = num_layers;
		inst_create_info.ppEnabledLayerNames = validation_layers;
		inst_create_info.enabledExtensionCount = num_extensions;
		inst_create_info.ppEnabledExtensionNames = extensions;

		result = vkCreateInstance(&inst_create_info, NULL, &vk_context.vk_instance);
		if (result != VK_SUCCESS)
		{
			printf("Could not create instance\n");
			return false;
		}

#define VK_IMPORT_INSTANCE_FUNC( _func )                           \
    _func = (PFN_##_func)vkGetInstanceProcAddr(vk_context.vk_instance, #_func);       \
    printf(#_func); printf("\n");                                   \
    imported &= NULL != _func;
VK_IMPORT_INSTANCE
#undef VK_IMPORT_INSTANCE_FUNC


        if (imported)
        {
            printf("Success again\n");
        }

		result = vkCreateDebugUtilsMessengerEXT(vk_context.vk_instance, &dbg_create_info, NULL, &vk_context.vk_debugmessenger);
		if (result != VK_SUCCESS)
		{
			printf("Could not create instance\n");
			return false;
		}
	}

    /*}}}*/



	/* Initiate swap chain */
	{
		VkXcbSurfaceCreateInfoKHR surface_create_info;
		surface_create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
		surface_create_info.pNext = NULL;
		surface_create_info.flags = 0;
		surface_create_info.connection = win.connection;
		surface_create_info.window = win.window;

		result = vkCreateXcbSurfaceKHR( vk_context.vk_instance, &surface_create_info, NULL,
				&vk_context.device.presentation_surface);
		if (result != VK_SUCCESS)
		{
			printf("Failed in Xcb surface creation\n");
			return false;
		}
	}

    /* Logical Device creation */
	{
		uint32_t physical_device_count;
		result = vkEnumeratePhysicalDevices(vk_context.vk_instance, &physical_device_count, NULL);
		if (result != VK_SUCCESS)
		{
			printf("Failed in device enumeration\n");
			return false;
		}

		VkPhysicalDevice *physical_devices =
			(VkPhysicalDevice*)malloc(physical_device_count * sizeof(VkPhysicalDevice));
		result = vkEnumeratePhysicalDevices(vk_context.vk_instance, &physical_device_count, physical_devices);

		vk_context.device.physical_device = physical_devices[0];
		for (uint32_t i = 0; i < physical_device_count; i++)
		{
			VkPhysicalDeviceProperties device_props;
			vkGetPhysicalDeviceProperties(physical_devices[i], &device_props);
			if (device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				vk_context.device.physical_device = physical_devices[i];
				printf("device name: %s\n", device_props.deviceName);
			}
		}
		free(physical_devices);

		uint32_t extension_count = 0;

		vkEnumerateDeviceExtensionProperties(vk_context.device.physical_device, NULL, &extension_count,
				NULL);
		VkExtensionProperties *available_extensions;
		available_extensions = (VkExtensionProperties *)malloc(extension_count *
				sizeof(VkExtensionProperties));
		vkEnumerateDeviceExtensionProperties(vk_context.device.physical_device, NULL, &extension_count,
				available_extensions);

		const char *device_extensions[] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		for (uint32_t i = 0; i < 1; i++)
		{
			bool has_extension = false;
			for(uint32_t j = 0; j < extension_count; j++)
			{
			   if (strcmp(device_extensions[i], available_extensions[j].extensionName) == 0)
			   {
				   printf("Extension %s found\n", device_extensions[i]);
				   has_extension = true;
				   break;
			   }
			}
			if (!has_extension)
			{
			   printf("Extension %s not found\n", device_extensions[i]);
			   return false;
			}
		}
		free(available_extensions);

		/* Choose queues */
		uint32_t queue_family_count;
		vkGetPhysicalDeviceQueueFamilyProperties(vk_context.device.physical_device, &queue_family_count, NULL);
		VkQueueFamilyProperties *queue_family_properties =
			(VkQueueFamilyProperties*)malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(vk_context.device.physical_device, &queue_family_count,
																				queue_family_properties);
		for (uint32_t i = 0; i < queue_family_count; i++)
		{
			VkBool32 queue_present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(vk_context.device.physical_device, i,
					vk_context.device.presentation_surface, &queue_present_support);
			if(queue_present_support) {
				vk_context.present_queue_index = i;
			}
			if (queue_family_properties[i].queueCount >0 &&
				queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				vk_context.graphic_queue_index = i;
				if(queue_present_support) break;
			}

		}
		for (uint32_t i = 0; i < queue_family_count; i++)
		{
			if (queue_family_count > 0 &&
				queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
				vk_context.transfer_queue_index = i;
			}
			if (queue_family_count > 0 &&
				!(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
				break;
			}
		}
		free(queue_family_properties);

		VkDeviceQueueCreateInfo queue_create_info[3];
		float queue_priority = 1.0;
		uint32_t number_of_queues = 1;
		uint32_t idx = 1;
		queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info[0].pNext = NULL;
		queue_create_info[0].flags = 0;
		queue_create_info[0].queueFamilyIndex = vk_context.graphic_queue_index;
		queue_create_info[0].queueCount = 1;
		queue_create_info[0].pQueuePriorities = &queue_priority;

		if (vk_context.graphic_queue_index != vk_context.transfer_queue_index) {
			number_of_queues++;
			queue_create_info[idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info[idx].pNext = NULL;
			queue_create_info[idx].flags = 0;
			queue_create_info[idx].queueFamilyIndex = vk_context.transfer_queue_index;
			queue_create_info[idx].queueCount = 1;
			queue_create_info[idx].pQueuePriorities = &queue_priority;
			idx++;
		}

		if (vk_context.graphic_queue_index != vk_context.present_queue_index) {
			number_of_queues++;
			queue_create_info[idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info[idx].pNext = NULL;
			queue_create_info[idx].flags = 0;
			queue_create_info[idx].queueFamilyIndex = vk_context.present_queue_index;
			queue_create_info[idx].queueCount = 1;
			queue_create_info[idx].pQueuePriorities = &queue_priority;
		}

		VkDeviceCreateInfo device_create_info;
		device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_create_info.pNext = NULL;
		device_create_info.flags = 0;
		device_create_info.queueCreateInfoCount = number_of_queues;
		device_create_info.pQueueCreateInfos = &queue_create_info[0];
		device_create_info.enabledExtensionCount = 1;
		device_create_info.ppEnabledExtensionNames = device_extensions;
		device_create_info.enabledLayerCount = 1;
		device_create_info.ppEnabledLayerNames = validation_layers;
		device_create_info.pEnabledFeatures = NULL;

		result = vkCreateDevice(vk_context.device.physical_device, &device_create_info, NULL, &vk_context.device.logical_device);
		if (result != VK_SUCCESS)
		{
			printf("Could not create logical device\n");
			return false;
		}
	}
#define VK_IMPORT_DEVICE_FUNC( _func )                                       \
    _func = (PFN_##_func)vkGetDeviceProcAddr(vk_context.device.logical_device, #_func);  \
    printf(#_func); printf("\n");                                                 \
    imported &= NULL != _func;
VK_IMPORT_DEVICE
#undef VK_IMPORT_DEVICE_FUNC

	/*Request queue handles */
	vkGetDeviceQueue(vk_context.device.logical_device, vk_context.graphic_queue_index, 0,
			&vk_context.vk_graphic_queue);
	vkGetDeviceQueue(vk_context.device.logical_device, vk_context.present_queue_index, 0,
			&vk_context.vk_present_queue);
	vkGetDeviceQueue(vk_context.device.logical_device, vk_context.transfer_queue_index, 0,
			&vk_context.vk_transfer_queue);

    result = create_swapchain(win.width, win.height);
    

    /* Depth stencil creation */
	{
        VkFormat depth_formats[5] = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        for (uint32_t i = 0; i < 5; i++) {
            VkFormatProperties format_props;
            vkGetPhysicalDeviceFormatProperties(vk_context.device.physical_device, depth_formats[i], &format_props);
            if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                vk_context.depth_image.format = depth_formats[i];
            }
        }
		VkImageAspectFlags aspect  = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (vk_context.depth_image.format >= VK_FORMAT_D16_UNORM_S8_UINT) {
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        VkResult result = create_image(&vk_context.device, &vk_context.depth_image, win.width, win.height, usage, aspect, 1);
		if(result != VK_SUCCESS) {
			printf("Could not create depth image!\n");
			return false;
		}

    }

	/* Render pass creation */
	{
		VkAttachmentDescription attachment_descriptions[2];
        /* Color attachment */
		attachment_descriptions[0].flags = 0;
		attachment_descriptions[0].format = vk_context.swapchain.format.format;
		attachment_descriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        /* Depth attachment */
		attachment_descriptions[1].flags = 0;
		attachment_descriptions[1].format = vk_context.depth_image.format;
		attachment_descriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference color_attachment_references[1];
		color_attachment_references[0].attachment = 0;
		color_attachment_references[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_attachment_references[1];
		depth_attachment_references[0].attachment = 1;
		depth_attachment_references[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass_descriptions[1];
		subpass_descriptions[0].flags = 0;
		subpass_descriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass_descriptions[0].inputAttachmentCount = 0;
		subpass_descriptions[0].pInputAttachments = NULL;
		subpass_descriptions[0].colorAttachmentCount = 1;
		subpass_descriptions[0].pColorAttachments = color_attachment_references;
		subpass_descriptions[0].pDepthStencilAttachment = depth_attachment_references;
		subpass_descriptions[0].pResolveAttachments = NULL;
		subpass_descriptions[0].preserveAttachmentCount = 0;
		subpass_descriptions[0].pPreserveAttachments = NULL;

		/*VkSubpassDependency dependencies[1];*/
		/*dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;*/
		/*dependencies[0].dstSubpass = 0;*/
		/*dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;*/
		/*dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;*/
		/*dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;*/
		/*dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;*/
		/*dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;*/
		VkSubpassDependency dependencies[1];
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = 0;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].dstAccessMask =  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		/*dependencies[1].srcSubpass = 0;*/
		/*dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;*/
		/*dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;*/
		/*dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;*/
		/*dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;*/
		/*dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;*/
		/*dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;*/


		VkRenderPassCreateInfo render_pass_create_info;
		render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_create_info.pNext = NULL;
		render_pass_create_info.flags = 0;
		render_pass_create_info.attachmentCount = 2;
		render_pass_create_info.pAttachments = attachment_descriptions;
		render_pass_create_info.subpassCount = 1;
		render_pass_create_info.pSubpasses = subpass_descriptions;
		render_pass_create_info.dependencyCount = 1;
		render_pass_create_info.pDependencies = dependencies;

		result = vkCreateRenderPass(vk_context.device.logical_device, &render_pass_create_info, NULL,
				&vk_context.render_pass);
		if(result != VK_SUCCESS) {
			printf("Could not create render pass!\n");
			return false;
		}

	}

	vk_context.rendering_resources_size = 2;

    /* Command pool creation and command buffer allocation */
	{
		VkCommandPoolCreateInfo cmd_pool_create_info;
		cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmd_pool_create_info.pNext = NULL;
		cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
									 VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		cmd_pool_create_info.queueFamilyIndex = vk_context.graphic_queue_index;
		result = vkCreateCommandPool(vk_context.device.logical_device, &cmd_pool_create_info, NULL,
				&vk_context.graphic_queue_cmdpool);
		if(result != VK_SUCCESS) {
			printf("Could not create command pool!\n");
			return false;
		}

		cmd_pool_create_info.queueFamilyIndex = vk_context.transfer_queue_index;
		result = vkCreateCommandPool(vk_context.device.logical_device, &cmd_pool_create_info, NULL,
				&vk_context.transfer_queue_cmdpool);
		if(result != VK_SUCCESS) {
			printf("Could not create command pool!\n");
			return false;
		}

		VkCommandBufferAllocateInfo cmd_buffer_allocate_info;
		cmd_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_buffer_allocate_info.pNext = NULL;
		cmd_buffer_allocate_info.commandPool = vk_context.graphic_queue_cmdpool;
		cmd_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_buffer_allocate_info.commandBufferCount = vk_context.rendering_resources_size;

		vk_context.graphic_queue_cmdbuffer = malloc(vk_context.rendering_resources_size *
				sizeof(*(vk_context.graphic_queue_cmdbuffer)));

		result = vkAllocateCommandBuffers(vk_context.device.logical_device, &cmd_buffer_allocate_info,
				vk_context.graphic_queue_cmdbuffer);
		if(result != VK_SUCCESS) {
			printf("Could not create command buffer!\n");
			return false;
		}

		cmd_buffer_allocate_info.commandPool = vk_context.transfer_queue_cmdpool;
		cmd_buffer_allocate_info.commandBufferCount = 1;

		result = vkAllocateCommandBuffers(vk_context.device.logical_device, &cmd_buffer_allocate_info,
				&vk_context.transfer_queue_cmdbuffer);
		if(result != VK_SUCCESS) {
			printf("Could not create command buffer!\n");
			return false;
		}

        cmd_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd_buffer_allocate_info.pNext = NULL;
        cmd_buffer_allocate_info.commandPool = vk_context.graphic_queue_cmdpool;
        cmd_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd_buffer_allocate_info.commandBufferCount = 1;


        result = vkAllocateCommandBuffers(vk_context.device.logical_device, &cmd_buffer_allocate_info,
                &vk_context.copy_cmdbuffer);
        if(result != VK_SUCCESS) {
            printf("Could not create command buffer!\n");
            return result;
        }
	}

    /* staging buffer creation */
    {
        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &vk_context.staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, powl(2, 25) * sizeof(VertexData));

		if(result != VK_SUCCESS) {
			printf("Could not bind memory for a vertex buffer!\n");
			return false;
		}

    }

    /* Uniform Buffer */
    
    {
        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &vk_context.uniform_buffer, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 3 * 16 * sizeof(float));

		if(result != VK_SUCCESS) {
			printf("Could not create uniform buffer!\n");
			return false;
		}

        Vector3 tranlation = {0.0, 0.0, 0};
        update_uniform_buffer(tranlation, 0);
    }

    /* Sky Uniform Buffer */
    
    {
        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &vk_context.uniform_buffer_sky, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 3 * 16 * sizeof(float));

		if(result != VK_SUCCESS) {
			printf("Could not create uniform buffer!\n");
			return false;
		}

        Vector3 tranlation = {0.0, 0.0, 0};
        update_uniform_buffer_sky(tranlation, 0);
    }

    MeshData planet = generate_vertex_data();
	{

        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &vk_context.vertex_buffer, 
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, planet.vertex_size);

		if(result != VK_SUCCESS) {
			printf("Could not create vertex buffer!\n");
			return false;
		}
        result = copy_buffer_staged(&vk_context.vertex_buffer, &vk_context.staging_buffer, planet.vertex_data, planet.vertex_size);
		if(result != VK_SUCCESS) {
			printf("Could not copy vertex buffer!\n");
			return false;
		}
	}

    {

        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &vk_context.index_buffer, 
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, planet.index_size);
		if(result != VK_SUCCESS) {
			printf("Could not create index buffer!\n");
			return false;
		}
        result = copy_buffer_staged(&vk_context.index_buffer, 
                &vk_context.staging_buffer, planet.index_data, planet.index_size);
		if(result != VK_SUCCESS) {
			printf("Could not copy index buffer!\n");
			return false;
		}
	}
    //skybox
    {
        float vertex_data[] = {
                    /*Position - Normal - uv*/
                 100.0,  100.0,  100.0,  0.0, 0.0, 1.0, 1.0, 0.0, /*0*/
                -100.0,  100.0,  100.0,  0.0, 0.0, 1.0, 0.0, 0.0, /*1*/
                -100.0, -100.0,  100.0,  0.0, 0.0, 1.0, 0.0, 1.0, /*2*/
                 100.0, -100.0,  100.0,  0.0, 0.0, 1.0, 1.0, 1.0, /*3*/

                 100.0,  100.0, -100.0,  0.0, 0.0, -1.0, 1.0, 0.0, /*4*/
                -100.0,  100.0, -100.0,  0.0, 0.0, -1.0, 0.0, 0.0, /*5*/
                -100.0, -100.0, -100.0,  0.0, 0.0, -1.0, 0.0, 1.0, /*6*/
                 100.0, -100.0, -100.0,  0.0, 0.0, -1.0, 1.0, 1.0, /*7*/

                 100.0,  100.0,  100.0,  1.0, 0.0, 0.0, 0.0, 0.0, /*8*/
                 100.0, -100.0,  100.0,  1.0, 0.0, 0.0, 0.0, 1.0, /*9*/
                 100.0,  100.0, -100.0,  1.0, 0.0, 0.0, 1.0, 0.0, /*10*/
                 100.0, -100.0, -100.0,  1.0, 0.0, 0.0, 1.0, 1.0, /*11*/

                -100.0,  100.0,  100.0, -1.0, 0.0, 0.0, 0.0, 0.0, /*12*/
                -100.0, -100.0,  100.0, -1.0, 0.0, 0.0, 0.0, 1.0, /*13*/
                -100.0,  100.0, -100.0, -1.0, 0.0, 0.0, 1.0, 0.0, /*14*/
                -100.0, -100.0, -100.0, -1.0, 0.0, 0.0, 1.0, 1.0, /*15*/

                 100.0,  100.0,  100.0,  0.0, 1.0, 0.0, 1.0, 1.0, /*16*/
                 100.0,  100.0, -100.0,  0.0, 1.0, 0.0, 1.0, 0.0, /*17*/
                -100.0,  100.0, -100.0,  0.0, 1.0, 0.0, 0.0, 0.0, /*18*/
                -100.0,  100.0,  100.0,  0.0, 1.0, 0.0, 0.0, 1.0, /*19*/

                 100.0, -100.0,  100.0,  0.0, -1.0, 0.0, 1.0, 1.0, /*20*/
                 100.0, -100.0, -100.0,  0.0, -1.0, 0.0, 1.0, 0.0, /*22*/
                -100.0, -100.0, -100.0,  0.0, -1.0, 0.0, 0.0, 0.0, /*23*/
                -100.0, -100.0,  100.0,  0.0, -1.0, 0.0, 0.0, 1.0, /*24*/
        };



        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &vk_context.vertex_buffer_sky, 
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(vertex_data));

		if(result != VK_SUCCESS) {
			printf("Could not create vertex buffer!\n");
			return false;
		}
        result = copy_buffer_staged(&vk_context.vertex_buffer_sky, &vk_context.staging_buffer, vertex_data, sizeof(vertex_data));
		if(result != VK_SUCCESS) {
			printf("Could not copy vertex buffer!\n");
			return false;
		}
	}

    {

        uint32_t index_data[] = {
            0, 1, 2,
            2, 3, 0,
            4, 6, 5,
            4, 7, 6,
            8, 9, 10,
            11, 10, 9,
            12, 14, 15,
            15, 13, 12,
            16, 17, 18,
            18, 19, 16,
            20, 23, 22,
            22, 21, 20

        };

        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &vk_context.index_buffer_sky, 
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(index_data));
		if(result != VK_SUCCESS) {
			printf("Could not create index buffer!\n");
			return false;
		}
        result = copy_buffer_staged(&vk_context.index_buffer_sky, 
                &vk_context.staging_buffer, index_data, sizeof(index_data));
		if(result != VK_SUCCESS) {
			printf("Could not copy index buffer!\n");
			return false;
		}
	}

	{
        TextureSampler sampler;
        sampler.mag_filter = VK_FILTER_LINEAR;
        sampler.min_filter = VK_FILTER_LINEAR;
        sampler.address_modeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.address_modeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.address_modeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        /*vk_context.texture = create_texture("misc/world.topo.bathy.200410.3x5400x2700.jpg", sampler, false);*/
        create_cubemap();
    }
    

    /* Descriptor set creation */
    {
		VkDescriptorSetLayoutBinding layout_bindings[2];
		layout_bindings[0].binding = 0;
		layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layout_bindings[0].descriptorCount = 1;
		layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings[0].pImmutableSamplers = NULL;
		layout_bindings[1].binding = 1;
		layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layout_bindings[1].descriptorCount = 1;
		layout_bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layout_bindings[1].pImmutableSamplers = NULL;

		VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
		descriptor_set_layout_create_info.sType =
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_set_layout_create_info.pNext = NULL;
		descriptor_set_layout_create_info.flags = 0;
		descriptor_set_layout_create_info.bindingCount = 2;
		descriptor_set_layout_create_info.pBindings = layout_bindings;

		result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
				&descriptor_set_layout_create_info, NULL, &vk_context.descriptor_set_layout);
		if(result != VK_SUCCESS) {
			printf("Could not submit command buffer!\n");
			return false;
		}

		VkDescriptorPoolSize pool_sizes[2];
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 2;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_sizes[1].descriptorCount = 2;

		VkDescriptorPoolCreateInfo descriptor_pool_create_info;
		descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool_create_info.pNext = NULL;
		descriptor_pool_create_info.flags = 0;
		descriptor_pool_create_info.maxSets = 2;
		descriptor_pool_create_info.poolSizeCount = 2;
		descriptor_pool_create_info.pPoolSizes = pool_sizes;

		result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
				&vk_context.descriptor_set_pool);
		if(result != VK_SUCCESS) {
			printf("Could not create descriptor pool!\n");
			return false;
		}

		VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
		descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptor_set_allocate_info.pNext = NULL;
		descriptor_set_allocate_info.descriptorPool = vk_context.descriptor_set_pool;
		descriptor_set_allocate_info.descriptorSetCount = 1;
		descriptor_set_allocate_info.pSetLayouts = &vk_context.descriptor_set_layout;

		result = vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info,
				&vk_context.descriptor_set);
		if(result != VK_SUCCESS) {
			printf("Could not create descriptor set!\n");
			return false;
		}

		result = vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info,
				&vk_context.descriptor_set_sky);
		if(result != VK_SUCCESS) {
			printf("Could not create descriptor set!\n");
			return false;
		}
    }
    
    /* Descriptor set update */
    {

		VkDescriptorImageInfo image_info;
		image_info.sampler = scene.materials.base_color_texture[0].sampler;
		image_info.imageView = scene.materials.base_color_texture[0].image_buffer.image_view;
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer = vk_context.uniform_buffer.buffer;
        buffer_info.offset = 0;
        buffer_info.range = vk_context.uniform_buffer.size;

		VkWriteDescriptorSet descriptor_writes[2];
		descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[0].pNext = NULL;
		descriptor_writes[0].dstSet = vk_context.descriptor_set;
		descriptor_writes[0].dstBinding = 0;
		descriptor_writes[0].dstArrayElement = 0;
		descriptor_writes[0].descriptorCount = 1;
		descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_writes[0].pImageInfo = &image_info;
		descriptor_writes[0].pBufferInfo = NULL;
		descriptor_writes[0].pTexelBufferView = NULL;
		descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[1].pNext = NULL;
		descriptor_writes[1].dstSet = vk_context.descriptor_set;
		descriptor_writes[1].dstBinding = 1;
		descriptor_writes[1].dstArrayElement = 0;
		descriptor_writes[1].descriptorCount = 1;
		descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes[1].pImageInfo = NULL;
		descriptor_writes[1].pBufferInfo = &buffer_info;
		descriptor_writes[1].pTexelBufferView = NULL;

		vkUpdateDescriptorSets(vk_context.device.logical_device, 2, descriptor_writes, 0, NULL);
	}

    /* Skybox Descriptor set update */
    {

		VkDescriptorImageInfo image_info;
		image_info.sampler = vk_context.cubemap_sampler;
		image_info.imageView = vk_context.cubemap.image_view;
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer = vk_context.uniform_buffer_sky.buffer;
        buffer_info.offset = 0;
        buffer_info.range = vk_context.uniform_buffer_sky.size;

		VkWriteDescriptorSet descriptor_writes[2];
		descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[0].pNext = NULL;
		descriptor_writes[0].dstSet = vk_context.descriptor_set_sky;
		descriptor_writes[0].dstBinding = 0;
		descriptor_writes[0].dstArrayElement = 0;
		descriptor_writes[0].descriptorCount = 1;
		descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_writes[0].pImageInfo = &image_info;
		descriptor_writes[0].pBufferInfo = NULL;
		descriptor_writes[0].pTexelBufferView = NULL;
		descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[1].pNext = NULL;
		descriptor_writes[1].dstSet = vk_context.descriptor_set_sky;
		descriptor_writes[1].dstBinding = 1;
		descriptor_writes[1].dstArrayElement = 0;
		descriptor_writes[1].descriptorCount = 1;
		descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes[1].pImageInfo = NULL;
		descriptor_writes[1].pBufferInfo = &buffer_info;
		descriptor_writes[1].pTexelBufferView = NULL;

		vkUpdateDescriptorSets(vk_context.device.logical_device, 2, descriptor_writes, 0, NULL);
	}



	/* Pipeline creation */
	{

		VkVertexInputBindingDescription vertex_binding_descriptions[1];
		vertex_binding_descriptions[0].binding = 0;
		vertex_binding_descriptions[0].stride = 8*sizeof(float);
		vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertex_attribute_descriptions[3];
		vertex_attribute_descriptions[0].location = 0;
		vertex_attribute_descriptions[0].binding = vertex_binding_descriptions[0].binding;
		vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_descriptions[0].offset = 0;
		vertex_attribute_descriptions[1].location = 1;
		vertex_attribute_descriptions[1].binding = vertex_binding_descriptions[0].binding;
		vertex_attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_descriptions[1].offset = 3*sizeof(float);
		vertex_attribute_descriptions[2].location = 2;
		vertex_attribute_descriptions[2].binding = vertex_binding_descriptions[0].binding;
		vertex_attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_descriptions[2].offset = 6*sizeof(float);

		VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info;
		vertex_input_state_create_info.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_state_create_info.pNext = NULL;
		vertex_input_state_create_info.flags = 0;
		vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
		vertex_input_state_create_info.pVertexBindingDescriptions = vertex_binding_descriptions;
		vertex_input_state_create_info.vertexAttributeDescriptionCount = 3;
		vertex_input_state_create_info.pVertexAttributeDescriptions = vertex_attribute_descriptions;

		VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info;
		input_assembly_state_create_info.sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_state_create_info.pNext = NULL;
		input_assembly_state_create_info.flags = 0;
		input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewport_state_create_info;
		viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state_create_info.pNext = NULL;
		viewport_state_create_info.flags = 0;
		viewport_state_create_info.viewportCount = 1;
		viewport_state_create_info.pViewports = NULL;
		viewport_state_create_info.scissorCount = 1;
		viewport_state_create_info.pScissors = NULL;

		VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic_state_create_info;
		dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_create_info.pNext = NULL;
		dynamic_state_create_info.flags = 0;
		dynamic_state_create_info.dynamicStateCount = 2;
		dynamic_state_create_info.pDynamicStates = dynamic_states;

		VkPipelineRasterizationStateCreateInfo rasterization_state_create_info;
		rasterization_state_create_info.sType =
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_state_create_info.pNext = NULL;
		rasterization_state_create_info.flags = 0;
		rasterization_state_create_info.depthClampEnable = VK_FALSE;
		rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_state_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterization_state_create_info.depthBiasEnable = VK_FALSE;
		rasterization_state_create_info.depthBiasConstantFactor = 0.0;
		rasterization_state_create_info.depthBiasClamp = 0.0;
		rasterization_state_create_info.depthBiasSlopeFactor = 0.0;
		rasterization_state_create_info.lineWidth = 1.0;

		VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE
        };

		VkPipelineColorBlendAttachmentState color_blend_attachment_state;
		color_blend_attachment_state.blendEnable = VK_TRUE;
		color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment_state
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        };

        VkPushConstantRange push_constants_range;
        push_constants_range.offset = 0;
        push_constants_range.size = sizeof(PushConstantsBlock);
        push_constants_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkPipelineLayoutCreateInfo layout_create_info;
		layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_create_info.pNext = NULL;
		layout_create_info.flags = 0;
		layout_create_info.setLayoutCount = 1;
		layout_create_info.pSetLayouts = &vk_context.descriptor_set_layout;
		layout_create_info.pushConstantRangeCount = 1;
		layout_create_info.pPushConstantRanges = &push_constants_range;


		result = vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL,
				&vk_context.pipeline_layout);
		if(result != VK_SUCCESS) {
			printf("Could not create pipeline layout!\n");
			return false;
		}

		VkGraphicsPipelineCreateInfo pipeline_create_info;
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.pNext = NULL;
		pipeline_create_info.flags = 0;
		pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
		pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
		pipeline_create_info.pTessellationState = NULL;
		pipeline_create_info.pViewportState = &viewport_state_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
		pipeline_create_info.pMultisampleState = &multisample_state_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
		pipeline_create_info.pDynamicState = &dynamic_state_create_info;
		pipeline_create_info.layout = vk_context.pipeline_layout;
		pipeline_create_info.renderPass = vk_context.render_pass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_create_info.basePipelineIndex = -1;

		VkPipelineShaderStageCreateInfo shader_stages[2];
        shader_stages[0] = load_shader(vk_context.device.logical_device, "vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	    shader_stages[1] = load_shader(vk_context.device.logical_device, "frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = shader_stages;

		result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
				&pipeline_create_info, NULL, &vk_context.graphics_pipeline);
		if(result != VK_SUCCESS) {
			printf("Could not create pipeline!\n");
			return false;
		}
        vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
        vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);

		rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
        depth_stencil_state_create_info.depthTestEnable = VK_FALSE;
        depth_stencil_state_create_info.depthWriteEnable = VK_FALSE;
        shader_stages[0] = load_shader(vk_context.device.logical_device, "vert_sky.spv", VK_SHADER_STAGE_VERTEX_BIT);
	    shader_stages[1] = load_shader(vk_context.device.logical_device, "frag_sky.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = shader_stages;

		result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
				&pipeline_create_info, NULL, &vk_context.graphics_pipeline_sky);
		if(result != VK_SUCCESS) {
			printf("Could not create pipeline!\n");
			return false;
		}
        vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
        vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);
	}
	/* Semaphore creation */
	{
		VkSemaphoreCreateInfo semaphore_create_info;
		semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphore_create_info.pNext = NULL;
		semaphore_create_info.flags = 0;
		vk_context.image_available_semaphore = malloc(vk_context.rendering_resources_size *
				sizeof(*(vk_context.image_available_semaphore)));
		vk_context.rendering_finished_semaphore = malloc(vk_context.rendering_resources_size *
				sizeof(*(vk_context.rendering_finished_semaphore)));

		for(uint32_t i = 0; i < vk_context.rendering_resources_size; i++) {
			result = vkCreateSemaphore(vk_context.device.logical_device, &semaphore_create_info, NULL,
					&vk_context.image_available_semaphore[i]);
			if (result != VK_SUCCESS) {
				printf("Could not create semaphores\n");
				return false;
			}
			result = vkCreateSemaphore(vk_context.device.logical_device, &semaphore_create_info, NULL,
					&vk_context.rendering_finished_semaphore[i]);

			if (result != VK_SUCCESS) {
				printf("Could not create semaphores\n");
				return false;
			}
		}
	}

	/* fences creation */
	{
		VkFenceCreateInfo fence_create_info;
		fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_create_info.pNext = NULL;
		fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vk_context.fences = malloc(vk_context.rendering_resources_size *
				sizeof(*(vk_context.fences)));

		for(uint32_t i = 0; i < vk_context.rendering_resources_size; i++) {

			result = vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL,
					&vk_context.fences[i]);
			if(result != VK_SUCCESS) {
				printf("Could not create fences!\n");
				return false;
			}
		}
	}

	/* commands creation */
	vk_context.framebuffer = malloc(vk_context.rendering_resources_size *
			sizeof(*(vk_context.framebuffer)));
	for (uint32_t i = 0; i < vk_context.rendering_resources_size; i++) {
		vk_context.framebuffer[i] = VK_NULL_HANDLE;
	}

    return true;
}

bool vk_resize(uint32_t width, uint32_t height) {
    vk_context.width = width;
    vk_context.height = height;
    create_swapchain(width, height);
    VkImageAspectFlags aspect  = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (vk_context.depth_image.format >= VK_FORMAT_D16_UNORM_S8_UINT) {
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkResult result = create_image(&vk_context.device, &vk_context.depth_image, width, height, usage, aspect, 1);
    if(result != VK_SUCCESS) {
        printf("Could not create depth image!\n");
        return false;
    }
    return true;
}

VkResult frame(uint32_t resource_index, uint32_t image_index, float dt) {

	if(vk_context.framebuffer[resource_index] != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(vk_context.device.logical_device, vk_context.framebuffer[resource_index], NULL);
	}

    VkImageView attachments[2] = {vk_context.swapchain.image_views[image_index], vk_context.depth_image.image_view };
	VkFramebufferCreateInfo framebuffer_create_info;
	framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_create_info.pNext = NULL;
	framebuffer_create_info.flags = 0;
	framebuffer_create_info.renderPass = vk_context.render_pass;
	framebuffer_create_info.attachmentCount = 2;
	framebuffer_create_info.pAttachments = attachments;
	framebuffer_create_info.width = vk_context.width;
	framebuffer_create_info.height = vk_context.height;
	framebuffer_create_info.layers = 1;
	VkResult result = vkCreateFramebuffer(vk_context.device.logical_device, &framebuffer_create_info, NULL,
			&vk_context.framebuffer[resource_index]);
	if(result != VK_SUCCESS) {
		printf("Could not create framebuffer!\n");
		return result;
	}

	VkCommandBufferBeginInfo command_buffer_begin_info;
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.pNext = NULL;
	command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	command_buffer_begin_info.pInheritanceInfo = NULL;
	vkBeginCommandBuffer(vk_context.graphic_queue_cmdbuffer[resource_index],
			&command_buffer_begin_info);
	VkImageSubresourceRange image_subresource_range;
	image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_subresource_range.baseMipLevel = 0;
	image_subresource_range.levelCount = 1;
	image_subresource_range.baseArrayLayer = 0;
	image_subresource_range.layerCount = 1;


	if (vk_context.vk_graphic_queue != vk_context.vk_present_queue) {
		VkImageMemoryBarrier barrier_from_present_to_draw;
		barrier_from_present_to_draw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier_from_present_to_draw.pNext = NULL;
		barrier_from_present_to_draw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		barrier_from_present_to_draw.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier_from_present_to_draw.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier_from_present_to_draw.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier_from_present_to_draw.srcQueueFamilyIndex = vk_context.present_queue_index;
		barrier_from_present_to_draw.dstQueueFamilyIndex = vk_context.graphic_queue_index;
		barrier_from_present_to_draw.image = vk_context.swapchain.images[image_index];
		barrier_from_present_to_draw.subresourceRange = image_subresource_range;
		vkCmdPipelineBarrier(vk_context.graphic_queue_cmdbuffer[resource_index],
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
				&barrier_from_present_to_draw);
	}

	VkClearValue clear_value[2];
    VkClearColorValue color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    VkClearDepthStencilValue depth = {1.0, 0.0};
    clear_value[0].color = color;
    clear_value[1].depthStencil = depth;

	VkRect2D render_area;
	render_area.offset.x = 0;
	render_area.offset.y = 0;
	render_area.extent.width = vk_context.width;
	render_area.extent.height = vk_context.height;
	VkRenderPassBeginInfo render_pass_begin_info;
	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = NULL;
	render_pass_begin_info.renderPass = vk_context.render_pass;
	render_pass_begin_info.framebuffer = vk_context.framebuffer[resource_index];
	render_pass_begin_info.renderArea = render_area;
	render_pass_begin_info.clearValueCount = 2;
	render_pass_begin_info.pClearValues = clear_value;

	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = vk_context.width;
	viewport.height = vk_context.height;
	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = vk_context.width;
	scissor.extent.height = vk_context.height;

	vkCmdBeginRenderPass(vk_context.graphic_queue_cmdbuffer[resource_index], &render_pass_begin_info,
			VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(vk_context.graphic_queue_cmdbuffer[resource_index], 0, 1, &viewport);
	vkCmdSetScissor(vk_context.graphic_queue_cmdbuffer[resource_index], 0, 1, &scissor);
	VkDeviceSize offset = 0;


    // draw skybox
	vkCmdBindVertexBuffers(vk_context.graphic_queue_cmdbuffer[resource_index], 0, 1,
			&vk_context.vertex_buffer_sky.buffer, &offset);
    vkCmdBindIndexBuffer(vk_context.graphic_queue_cmdbuffer[resource_index], vk_context.index_buffer_sky.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets( vk_context.graphic_queue_cmdbuffer[resource_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk_context.pipeline_layout, 0, 1, &vk_context.descriptor_set_sky, 0, NULL );
    vkCmdBindPipeline(vk_context.graphic_queue_cmdbuffer[resource_index],
            VK_PIPELINE_BIND_POINT_GRAPHICS, vk_context.graphics_pipeline_sky);
    vkCmdDrawIndexed(vk_context.graphic_queue_cmdbuffer[resource_index], vk_context.index_buffer_sky.size/(sizeof(uint32_t)), 1, 0, 0, 1);


    // draw planet
	vkCmdBindVertexBuffers(vk_context.graphic_queue_cmdbuffer[resource_index], 0, 1,
			&vk_context.vertex_buffer.buffer, &offset);
    vkCmdBindIndexBuffer(vk_context.graphic_queue_cmdbuffer[resource_index], vk_context.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindPipeline(vk_context.graphic_queue_cmdbuffer[resource_index],
			VK_PIPELINE_BIND_POINT_GRAPHICS, vk_context.graphics_pipeline);

	vkCmdBindDescriptorSets( vk_context.graphic_queue_cmdbuffer[resource_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk_context.pipeline_layout, 0, 1, &vk_context.descriptor_set, 0, NULL );
    PushConstantsBlock push_constants;
    push_constants.base_color_factor = scene.materials.base_color_factors[0];
    push_constants.emissive_factor = scene.materials.emissive_factors[0];
    push_constants.metallic_factor = scene.materials.metallic_factors[0];
    push_constants.roughness_factor = scene.materials.roughness_factor[0];
    vkCmdPushConstants(vk_context.graphic_queue_cmdbuffer[resource_index], vk_context.pipeline_layout,
            VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantsBlock), &push_constants);

    vkCmdDrawIndexed(vk_context.graphic_queue_cmdbuffer[resource_index], vk_context.index_buffer.size/(sizeof(uint32_t)), 1, 0, 0, 1);

    nk_draw(vk_context.graphic_queue_cmdbuffer[resource_index]);

	vkCmdEndRenderPass(vk_context.graphic_queue_cmdbuffer[resource_index]);

	if (vk_context.vk_graphic_queue != vk_context.vk_present_queue) {
		VkImageMemoryBarrier barrier_from_draw_to_present;
		barrier_from_draw_to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier_from_draw_to_present.pNext = NULL;
		barrier_from_draw_to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier_from_draw_to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		barrier_from_draw_to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier_from_draw_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier_from_draw_to_present.srcQueueFamilyIndex = vk_context.graphic_queue_index;
		barrier_from_draw_to_present.dstQueueFamilyIndex = vk_context.present_queue_index;
		barrier_from_draw_to_present.image = vk_context.swapchain.images[image_index];
		barrier_from_draw_to_present.subresourceRange = image_subresource_range;
		vkCmdPipelineBarrier(vk_context.graphic_queue_cmdbuffer[resource_index],
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
				&barrier_from_draw_to_present);
	}

	 result = vkEndCommandBuffer(vk_context.graphic_queue_cmdbuffer[resource_index]);
	if(result != VK_SUCCESS) {
		printf("Could not create pipeline!\n");
		return result;
	}



	VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &vk_context.image_available_semaphore[resource_index];
	submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk_context.graphic_queue_cmdbuffer[resource_index];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &vk_context.rendering_finished_semaphore[resource_index];
	result = vkQueueSubmit(vk_context.vk_graphic_queue, 1, &submit_info,
			vk_context.fences[resource_index]);
	if (result != VK_SUCCESS) {
		return result;
	}
    return result;

}

bool vk_renderer_draw(float dt) {
    /*fly_camera_update(vk_context.camera, dt);*/
	static uint32_t resource_index = 0;
	uint32_t next_resource_index = (resource_index + 1) % vk_context.rendering_resources_size;
	uint32_t image_index;
	VkResult result = vkWaitForFences(vk_context.device.logical_device, 1,
			&vk_context.fences[resource_index],
			VK_FALSE, 1000000000);
	if(result != VK_SUCCESS) {
			printf("Waiting for fence takes too long!\n");
			return false;
	}
	vkResetFences(vk_context.device.logical_device, 1, &vk_context.fences[resource_index]);

	result = vkAcquireNextImageKHR(vk_context.device.logical_device, vk_context.swapchain.swapchain,
			UINT64_MAX, vk_context.image_available_semaphore[resource_index], VK_NULL_HANDLE, &image_index);
	switch(result) {
		case VK_SUCCESS:
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
			break;
		default:
			printf("Problem occurred during swap chain image acquisition!\n");
			return false;
	}

    Vector3 translation = {0.0, 0.0, -1};
    update_uniform_buffer(translation, dt);
    Vector3 tranlation_sky = {0.0, 0.0, 0};
    update_uniform_buffer_sky(tranlation_sky, 0);
    frame(resource_index, image_index, dt);

	VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk_context.rendering_finished_semaphore[resource_index];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk_context.swapchain.swapchain;
	present_info.pImageIndices = &image_index;
	present_info.pResults = NULL;

	result = vkQueuePresentKHR(vk_context.vk_present_queue, &present_info);
	switch(result) {
		case VK_SUCCESS:
		case VK_SUBOPTIMAL_KHR:
			break;
		case VK_ERROR_OUT_OF_DATE_KHR:
		default:
			printf("Problem occurred during image presentation!\n");
			return false;
	}
	resource_index = next_resource_index;

    vkDeviceWaitIdle(vk_context.device.logical_device);
	return true;
}

void vk_renderer_cleanup()
{
	vkDeviceWaitIdle(vk_context.device.logical_device);

	vkFreeCommandBuffers(vk_context.device.logical_device, vk_context.graphic_queue_cmdpool,
			vk_context.rendering_resources_size, vk_context.graphic_queue_cmdbuffer);
	vkDestroyCommandPool(vk_context.device.logical_device, vk_context.graphic_queue_cmdpool, NULL);
	vkDestroyPipeline(vk_context.device.logical_device, vk_context.graphics_pipeline, NULL);
	vkDestroyRenderPass(vk_context.device.logical_device, vk_context.render_pass, NULL);
	for (uint32_t i = 0; i < 4444; i++) {
		vkDestroyImageView(vk_context.device.logical_device, vk_context.swapchain.image_views[i], NULL);
	}
	vkDestroyPipelineLayout(vk_context.device.logical_device, vk_context.pipeline_layout, NULL);
	vkDestroySwapchainKHR(vk_context.device.logical_device, vk_context.swapchain.swapchain, NULL);
	/*vkDestroySemaphore(vk_context.device.logical_device, vk_context.image_available_semaphore, NULL);*/
	/*vkDestroySemaphore(vk_context.device.logical_device, vk_context.rendering_finished_semaphore, NULL);*/

	vkDestroyDevice(vk_context.device.logical_device, NULL);

	vkDestroyDebugUtilsMessengerEXT(vk_context.vk_instance, vk_context.vk_debugmessenger, NULL);
	vkDestroySurfaceKHR(vk_context.vk_instance, vk_context.device.presentation_surface, NULL);


	vkDestroyInstance(vk_context.vk_instance, NULL);
	dlclose(vk_context.vulkan_library);
}


NK_API struct nk_context* nk_gui_init() {
    VkResult result;
    nk_buffer_init_default(&nk_gui.cmds);
    nk_init_default(&nk_gui.gui_context, 0);
    nk_gui.fb_scale.x = 1;
    nk_gui.fb_scale.y = 1;

    /* staging buffer creation */
    {
        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &nk_gui.staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, powl(2, 20) * sizeof(VertexData));

		if(result != VK_SUCCESS) {
			printf("Could not bind memory for a vertex buffer!\n");
			return false;
		}

    }

    /* Uniform Buffer */
    
    {
        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &nk_gui.uniform_buffer, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 3 * 16 * sizeof(float));

		if(result != VK_SUCCESS) {
			printf("Could not create uniform buffer!\n");
			return false;
		}

        /*Vector3 tranlation = {0.0, 0.0, -500};*/
        /*update_uniform_buffer(tranlation, 0);*/
    }

	{

        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &nk_gui.vertex_buffer, 
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MAX_VERTEX_BUFFER);

		if(result != VK_SUCCESS) {
			printf("Could not create vertex buffer!\n");
			return false;
		}
	}

    {
        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &nk_gui.index_buffer, 
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MAX_INDEX_BUFFER);
		if(result != VK_SUCCESS) {
			printf("Could not create index buffer!\n");
			return false;
		}
	}


    /* Descriptor set creation */
    {
		VkDescriptorSetLayoutBinding layout_bindings[2];
		layout_bindings[0].binding = 0;
		layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layout_bindings[0].descriptorCount = 1;
		layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		layout_bindings[0].pImmutableSamplers = NULL;
		layout_bindings[1].binding = 1;
		layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layout_bindings[1].descriptorCount = 1;
		layout_bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layout_bindings[1].pImmutableSamplers = NULL;

		VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
		descriptor_set_layout_create_info.sType =
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptor_set_layout_create_info.pNext = NULL;
		descriptor_set_layout_create_info.flags = 0;
		descriptor_set_layout_create_info.bindingCount = 2;
		descriptor_set_layout_create_info.pBindings = layout_bindings;

		result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
				&descriptor_set_layout_create_info, NULL, &nk_gui.descriptor_set_layout);
		if(result != VK_SUCCESS) {
			printf("Could not submit command buffer!\n");
			return false;
		}

		VkDescriptorPoolSize pool_sizes[2];
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 1;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_sizes[1].descriptorCount = 1;

		VkDescriptorPoolCreateInfo descriptor_pool_create_info;
		descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool_create_info.pNext = NULL;
		descriptor_pool_create_info.flags = 0;
		descriptor_pool_create_info.maxSets = 1;
		descriptor_pool_create_info.poolSizeCount = 2;
		descriptor_pool_create_info.pPoolSizes = pool_sizes;

		result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
				&nk_gui.descriptor_set_pool);
		if(result != VK_SUCCESS) {
			printf("Could not create descriptor pool!\n");
			return false;
		}

		VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
		descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptor_set_allocate_info.pNext = NULL;
		descriptor_set_allocate_info.descriptorPool = nk_gui.descriptor_set_pool;
		descriptor_set_allocate_info.descriptorSetCount = 1;
		descriptor_set_allocate_info.pSetLayouts = &nk_gui.descriptor_set_layout;

		result = vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info,
				&nk_gui.descriptor_set);
		if(result != VK_SUCCESS) {
			printf("Could not create descriptor set!\n");
			return false;
		}
    }


    /* Pipeline creation */
	{
		VkVertexInputBindingDescription vertex_binding_descriptions[1];
		vertex_binding_descriptions[0].binding = 0;
		vertex_binding_descriptions[0].stride = sizeof(struct nk_vertex);
		vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertex_attribute_descriptions[3];
		vertex_attribute_descriptions[0].location = 0;
		vertex_attribute_descriptions[0].binding = vertex_binding_descriptions[0].binding;
		vertex_attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_descriptions[0].offset = offsetof(struct nk_vertex, position);
		vertex_attribute_descriptions[1].location = 1;
		vertex_attribute_descriptions[1].binding = vertex_binding_descriptions[0].binding;
		vertex_attribute_descriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_descriptions[1].offset = offsetof(struct nk_vertex, uv);
		vertex_attribute_descriptions[2].location = 2;
		vertex_attribute_descriptions[2].binding = vertex_binding_descriptions[0].binding;
		vertex_attribute_descriptions[2].format = VK_FORMAT_R8G8B8A8_UINT;
		vertex_attribute_descriptions[2].offset = offsetof(struct nk_vertex, col);

		VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info;
		vertex_input_state_create_info.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_state_create_info.pNext = NULL;
		vertex_input_state_create_info.flags = 0;
		vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
		vertex_input_state_create_info.pVertexBindingDescriptions = vertex_binding_descriptions;
		vertex_input_state_create_info.vertexAttributeDescriptionCount = 3;
		vertex_input_state_create_info.pVertexAttributeDescriptions = vertex_attribute_descriptions;

		VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info;
		input_assembly_state_create_info.sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_state_create_info.pNext = NULL;
		input_assembly_state_create_info.flags = 0;
		input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewport_state_create_info;
		viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state_create_info.pNext = NULL;
		viewport_state_create_info.flags = 0;
		viewport_state_create_info.viewportCount = 1;
		viewport_state_create_info.pViewports = NULL;
		viewport_state_create_info.scissorCount = 1;
		viewport_state_create_info.pScissors = NULL;

		VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic_state_create_info;
		dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_create_info.pNext = NULL;
		dynamic_state_create_info.flags = 0;
		dynamic_state_create_info.dynamicStateCount = 2;
		dynamic_state_create_info.pDynamicStates = dynamic_states;

		VkPipelineRasterizationStateCreateInfo rasterization_state_create_info;
		rasterization_state_create_info.sType =
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_state_create_info.pNext = NULL;
		rasterization_state_create_info.flags = 0;
		rasterization_state_create_info.depthClampEnable = VK_FALSE;
		rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
		rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterization_state_create_info.depthBiasEnable = VK_FALSE;
		rasterization_state_create_info.depthBiasConstantFactor = 0.0;
		rasterization_state_create_info.depthBiasClamp = 0.0;
		rasterization_state_create_info.depthBiasSlopeFactor = 0.0;
		rasterization_state_create_info.lineWidth = 1.0;

		VkPipelineMultisampleStateCreateInfo multisample_state_create_info;
		multisample_state_create_info.sType =
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_state_create_info.pNext = NULL;
		multisample_state_create_info.flags = 0;
		multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_state_create_info.sampleShadingEnable = VK_FALSE;
		multisample_state_create_info.minSampleShading = 1.0;
		multisample_state_create_info.pSampleMask = NULL;
		multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
		multisample_state_create_info.alphaToOneEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState color_blend_attachment_state;
		color_blend_attachment_state.blendEnable = VK_TRUE;
		color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment_state
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
        };

		VkPipelineLayoutCreateInfo layout_create_info;
		layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_create_info.pNext = NULL;
		layout_create_info.flags = 0;
		layout_create_info.setLayoutCount = 1;
		layout_create_info.pSetLayouts = &nk_gui.descriptor_set_layout;
		layout_create_info.pushConstantRangeCount = 0;
		layout_create_info.pPushConstantRanges = NULL;

		result = vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL,
				&nk_gui.pipeline_layout);
		if(result != VK_SUCCESS) {
			printf("Could not create pipeline layout!\n");
			return false;
		}

		VkGraphicsPipelineCreateInfo pipeline_create_info;
		pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_create_info.pNext = NULL;
		pipeline_create_info.flags = 0;
		pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
		pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
		pipeline_create_info.pTessellationState = NULL;
		pipeline_create_info.pViewportState = &viewport_state_create_info;
		pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
		pipeline_create_info.pMultisampleState = &multisample_state_create_info;
		pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;
		pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
		pipeline_create_info.pDynamicState = &dynamic_state_create_info;
		pipeline_create_info.layout = nk_gui.pipeline_layout;
		pipeline_create_info.renderPass = vk_context.render_pass;
		pipeline_create_info.subpass = 0;
		pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_create_info.basePipelineIndex = -1;

		VkPipelineShaderStageCreateInfo shader_stages[2];
        shader_stages[0] = load_shader(vk_context.device.logical_device, "nk_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	    shader_stages[1] = load_shader(vk_context.device.logical_device, "nk_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		pipeline_create_info.stageCount = 2;
		pipeline_create_info.pStages = shader_stages;

		result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
				&pipeline_create_info, NULL, &nk_gui.graphics_pipeline);
		if(result != VK_SUCCESS) {
			printf("Could not create pipeline!\n");
			return false;
		}
        vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
        vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);

	}

    return &nk_gui.gui_context;
}

NK_API void   nk_font_stash_begin(struct nk_font_atlas** atlas) {
    nk_font_atlas_init_default(&nk_gui.atlas);
    nk_font_atlas_begin(&nk_gui.atlas);
    *atlas = &nk_gui.atlas;
}

NK_API void   nk_font_stash_end() {
    VkResult result;
    const void *image; int w, h;
    image = nk_font_atlas_bake(&nk_gui.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    printf("w: %d, h: %d\n", w, h);
    {
        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        nk_gui.font_image.format = VK_FORMAT_R8G8B8A8_UNORM;
        result = create_image(&vk_context.device, &nk_gui.font_image, w, h, usage, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		if(result != VK_SUCCESS) {
			printf("Could not create texture image!\n");
			return;
		}
        copy_image_staged(&nk_gui.font_image, &nk_gui.staging_buffer, VK_IMAGE_ASPECT_COLOR_BIT, (uint8_t*)image, w, h);
    }
    /* Sampler creation */
    {
		VkSamplerCreateInfo sampler_create_info;
		sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_create_info.pNext = NULL;
		sampler_create_info.flags = 0;
		sampler_create_info.magFilter = VK_FILTER_LINEAR;
		sampler_create_info.minFilter = VK_FILTER_LINEAR;
		sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_create_info.mipLodBias = 0.0;
		sampler_create_info.anisotropyEnable = VK_FALSE;
		sampler_create_info.maxAnisotropy = 1.0;
		sampler_create_info.compareEnable = VK_FALSE;
		sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
		sampler_create_info.minLod = 0.0;
		sampler_create_info.maxLod = 0.0;
		sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		sampler_create_info.unnormalizedCoordinates = VK_FALSE;

		result = vkCreateSampler(vk_context.device.logical_device, &sampler_create_info, NULL,
				&nk_gui.font_sampler);
		if(result != VK_SUCCESS) {
			printf("Could not submit command buffer!\n");
			return;
		}
    }


    nk_font_atlas_end(&nk_gui.atlas, nk_handle_ptr(nk_gui.font_sampler), &nk_gui.null);
    if (nk_gui.atlas.default_font) {
        nk_style_set_font(&nk_gui.gui_context, &nk_gui.atlas.default_font->handle);
    }

    /* Descriptor set update */
    {

		VkDescriptorImageInfo image_info;
		image_info.sampler = nk_gui.font_sampler;
		image_info.imageView = nk_gui.font_image.image_view;
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer = nk_gui.uniform_buffer.buffer;
        buffer_info.offset = 0;
        buffer_info.range = nk_gui.uniform_buffer.size;

		VkWriteDescriptorSet descriptor_writes[2];
		descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[0].pNext = NULL;
		descriptor_writes[0].dstSet = nk_gui.descriptor_set;
		descriptor_writes[0].dstBinding = 0;
		descriptor_writes[0].dstArrayElement = 0;
		descriptor_writes[0].descriptorCount = 1;
		descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_writes[0].pImageInfo = &image_info;
		descriptor_writes[0].pBufferInfo = NULL;
		descriptor_writes[0].pTexelBufferView = NULL;
		descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes[1].pNext = NULL;
		descriptor_writes[1].dstSet = nk_gui.descriptor_set;
		descriptor_writes[1].dstBinding = 1;
		descriptor_writes[1].dstArrayElement = 0;
		descriptor_writes[1].descriptorCount = 1;
		descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes[1].pImageInfo = NULL;
		descriptor_writes[1].pBufferInfo = &buffer_info;
		descriptor_writes[1].pTexelBufferView = NULL;

		vkUpdateDescriptorSets(vk_context.device.logical_device, 2, descriptor_writes, 0, NULL);
	}
}

bool update_gui_buffer() {
        float ortho[4][4] = {
            {2.0f, 0.0f, 0.0f, 0.0f},
            {0.0f,-2.0f, 0.0f, 0.0f},
            {0.0f, 0.0f,-1.0f, 0.0f},
            {-1.0f,1.0f, 0.0f, 1.0f},
        };
        ortho[0][0] /= vk_context.width;
        ortho[1][1] /= vk_context.height;
        static float r = 0;
        Ubo ubo;
        ubo.model = sx_mat4_ident();
        /*orth_proj_matrix(proj_matrix, -WIDTH/2, WIDTH/2, -HEIGHT/2, HEIGHT/2, 1000, -1000);*/
        /*ubo.projection = perspective_mat((Camera*)&device()->camera);*/
        /*ubo.view = sx_mat4_ident();*/
        /*ubo.projection = sx_mat4_ortho(vk_context.width, vk_context.height, 0, 1000, 0, false);*/
        memcpy(&ubo.projection, ortho, 16 * sizeof(float));
        ubo.view = sx_mat4_ident();
        /*memcpy(&ubo.view, &vk_context.camera->view, 16 * sizeof(float));*/
        /*mat4_transpose(res_matrix, m);*/
        copy_buffer_staged(&nk_gui.uniform_buffer, 
                &nk_gui.staging_buffer, &ubo, sizeof(ubo));
        return true;
}

uint8_t vertices[MAX_VERTEX_BUFFER];
uint8_t indices[MAX_INDEX_BUFFER];

NK_API void   nk_draw(VkCommandBuffer command_buffer) {
    struct nk_buffer vbuf, ebuf;
    update_gui_buffer();
	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = vk_context.width;
	viewport.height = vk_context.height;
	viewport.minDepth = 0.0;
	viewport.maxDepth = 1.0;

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = vk_context.width;
	scissor.extent.height = vk_context.height;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, nk_gui.graphics_pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, nk_gui.pipeline_layout,
            0, 1, &nk_gui.descriptor_set, 0, NULL);
    {
        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;

        /* load draw vertices & elements directly into vertex + element buffer */
        {
            /* fill convert configuration */
            struct nk_convert_config config;
            static const struct nk_draw_vertex_layout_element vertex_layout[] = {
                {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_vertex, position)},
                {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_vertex, uv)},
                {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_vertex, col)},
                {NK_VERTEX_LAYOUT_END}
            };
            sx_memset(&config, 0, sizeof(config));
            config.vertex_layout = vertex_layout;
            config.vertex_size = sizeof(struct nk_vertex);
            config.vertex_alignment = NK_ALIGNOF(struct nk_vertex);
            config.null = nk_gui.null;
            config.circle_segment_count = 22;
            config.curve_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.0f;
            config.shape_AA = NK_ANTI_ALIASING_ON;
            config.line_AA = NK_ANTI_ALIASING_ON;

            /* setup buffers to load vertices and elements */
            nk_buffer_init_fixed(&vbuf, vertices, (size_t)MAX_VERTEX_BUFFER);
            nk_buffer_init_fixed(&ebuf, indices, (size_t)MAX_INDEX_BUFFER);
            nk_convert(&nk_gui.gui_context, &nk_gui.cmds, &vbuf, &ebuf, &config);
            copy_buffer_staged(&nk_gui.vertex_buffer, &nk_gui.staging_buffer, vertices, sizeof(vertices));
            copy_buffer_staged(&nk_gui.index_buffer, &nk_gui.staging_buffer, indices, sizeof(indices));
        }

        /* iterate over and execute each draw command */
        VkDeviceSize doffset = 0;
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &nk_gui.vertex_buffer.buffer, &doffset);
        vkCmdBindIndexBuffer(command_buffer, nk_gui.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT16);
        
        uint32_t index_offset = 0;
        nk_draw_foreach(cmd, &nk_gui.gui_context, &nk_gui.cmds) {
            if (!cmd->elem_count) continue;

            VkRect2D scissor = {
                .extent = {
                    .width = cmd->clip_rect.w * nk_gui.fb_scale.x,
                    .height = cmd->clip_rect.h * nk_gui.fb_scale.y,
                },
                .offset = {
                    .x = sx_max(cmd->clip_rect.x * nk_gui.fb_scale.x, (float)0.0),
                    .y = sx_max(cmd->clip_rect.y * nk_gui.fb_scale.y, (float)0.0),
                }
            };
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            vkCmdDrawIndexed(command_buffer, cmd->elem_count, 1, index_offset, 0, 0);
            index_offset += cmd->elem_count;
        }
        nk_clear(&nk_gui.gui_context);
    }
}


VkPipelineShaderStageCreateInfo load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo shaderstage_create_info;
    shaderstage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderstage_create_info.pNext = NULL;
    shaderstage_create_info.flags = 0;
    shaderstage_create_info.stage = stage;
    shaderstage_create_info.pName = "main";
    shaderstage_create_info.pSpecializationInfo = NULL;
    FILE* shader = fopen(filnename, "rb");
    fseek(shader, 0, SEEK_END);
    uint64_t size = ftell(shader);
    uint8_t* data = (uint8_t*)aligned_alloc(sizeof(uint32_t), size * sizeof(uint8_t));
    rewind(shader);
    fread(data, sizeof(uint8_t), size, shader);
    VkShaderModuleCreateInfo module_create_info;
    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_create_info.pNext = NULL;
    module_create_info.flags = 0;
    module_create_info.codeSize = size;
    module_create_info.pCode = (uint32_t*)data;
    VkResult result = vkCreateShaderModule(logical_device, &module_create_info, NULL,
            &shaderstage_create_info.module);
    if(result != VK_SUCCESS) {
        printf("Could not create vert shader module!\n");
    }
    free(data);
    return shaderstage_create_info;
}

VkResult create_cubemap() {
    VkResult result;
    int total_size = 0;
    int w[6], h[6], channels[6];
    unsigned char* data[6];
    char* filenames[] = {"misc/skybox_space/right.png", "misc/skybox_space/left.png", "misc/skybox_space/top.png", "misc/skybox_space/bottom.png",
        "misc/skybox_space/front.png", "misc/skybox_space/back.png"};
    for (int i = 0; i < 6; i++) {
        data[i] = stbi_load(filenames[i], &w[i], &h[i], &channels[i], STBI_rgb_alpha);
        if (data[i] == NULL) 
            printf("failed\n");
        total_size += w[i] * h[i] * 4;
    }
    Buffer staging;
    //creating staging buffer
    {
        result = create_buffer(vk_context.device.physical_device, vk_context.device.logical_device, &staging, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, total_size);
		if(result != VK_SUCCESS) {
			printf("Could not bind memory for a vertex buffer!\n");
			return false;
		}

    }

    {
		void *staging_buffer_memory_pointer;
		VkResult result = vkMapMemory(vk_context.device.logical_device, staging.memory, 0,
				total_size, 0, &staging_buffer_memory_pointer);
		if(result != VK_SUCCESS) {
			printf("Could not map memory and upload data to a index buffer!\n");
			return false;
		}
        printf("size: %d\n", total_size);
        uint32_t offset = 0;
        for (uint32_t face = 0; face < 6; face++) {
            sx_memcpy(staging_buffer_memory_pointer + offset, data[face], w[face] * h[face] * 4);
            offset += w[face] * h[face] * 4;
        }
		vkUnmapMemory(vk_context.device.logical_device, staging.memory);
        VkBufferImageCopy buffer_copy_regions[6];

        offset = 0;
        for (uint32_t face = 0; face < 6; face++) {
            stbi_image_free(data[face]);
            VkImageSubresourceLayers image_subresource_layers = {};
            image_subresource_layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_subresource_layers.mipLevel = 0;
            image_subresource_layers.baseArrayLayer = face;
            image_subresource_layers.layerCount = 1;
            VkOffset3D image_offset;
            image_offset.x = 0;
            image_offset.y = 0;
            image_offset.z = 0;
            VkExtent3D image_extent;
            image_extent.width = w[face];
            image_extent.height = h[face];
            image_extent.depth = 1;
            VkBufferImageCopy buffer_image_copy_info;
            buffer_image_copy_info.bufferOffset = offset;
            buffer_image_copy_info.bufferRowLength = 0;
            buffer_image_copy_info.bufferImageHeight = 0;
            buffer_image_copy_info.imageSubresource = image_subresource_layers;
            buffer_image_copy_info.imageExtent = image_extent;
            buffer_image_copy_info.imageOffset = image_offset;
            buffer_copy_regions[face] = buffer_image_copy_info;
            offset += w[face] * h[face] * 4;
        }
        VkExtent3D extent;
		extent.width = w[0];
		extent.height = h[0];
		extent.depth = 1;
		VkImageCreateInfo image_create_info;
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.pNext = NULL;
		image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_create_info.extent = extent;
		image_create_info.mipLevels = 1;
		image_create_info.arrayLayers = 6;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.queueFamilyIndexCount = 0;
		image_create_info.pQueueFamilyIndices = NULL;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		result = vkCreateImage(vk_context.device.logical_device, &image_create_info, NULL,
				&vk_context.cubemap.image);
		if(result != VK_SUCCESS) {
			printf("Could not create depht image!\n");
			return result;
		}

		VkMemoryRequirements image_memory_requirements;
		vkGetImageMemoryRequirements(vk_context.device.logical_device, vk_context.cubemap.image,
				&image_memory_requirements);

        VkMemoryAllocateInfo memory_allocate_info;
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = NULL;
        memory_allocate_info.allocationSize = image_memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = get_memory_type(image_memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
                &vk_context.cubemap.memory);

        if(result != VK_SUCCESS) {
            printf("Could not allocate memory!\n");
            return result;
        }

		result = vkBindImageMemory(vk_context.device.logical_device, vk_context.cubemap.image,
				vk_context.cubemap.memory, 0);
		if(result != VK_SUCCESS) {
			printf("Could not bind memory image!\n");
			return result;
		}

		VkImageSubresourceRange image_subresource_range;
        image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_subresource_range.baseMipLevel = 0;
		image_subresource_range.levelCount = 1;
		image_subresource_range.baseArrayLayer = 0;
		image_subresource_range.layerCount = 6;
        VkCommandBuffer cmdbuffer = vk_context.copy_cmdbuffer;;

		VkCommandBufferBeginInfo command_buffer_begin_info;
		command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_begin_info.pNext = NULL;
		command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		command_buffer_begin_info.pInheritanceInfo = NULL;

		vkBeginCommandBuffer(cmdbuffer, &command_buffer_begin_info);

        {
            VkImageMemoryBarrier image_memory_barrier = {};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.pNext = NULL;
            image_memory_barrier.srcAccessMask = 0;
            image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_memory_barrier.image = vk_context.cubemap.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier(cmdbuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
                    NULL, 1, &image_memory_barrier);
        }

		vkCmdCopyBufferToImage(cmdbuffer, staging.buffer,
				vk_context.cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6,
				buffer_copy_regions);
        {
            VkImageMemoryBarrier image_memory_barrier = {};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.pNext = NULL;
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_memory_barrier.image = vk_context.cubemap.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier( cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
                    &image_memory_barrier);
        }

		vkEndCommandBuffer(cmdbuffer);

		VkSubmitInfo submit_info;
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = NULL;
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmdbuffer;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;

        VkFenceCreateInfo fence_create_info;
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = 0;
        fence_create_info.pNext = NULL;
        VkFence fence;
        vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL, &fence);

        result = vkQueueSubmit(vk_context.vk_graphic_queue, 1, &submit_info, fence);
        if(result != VK_SUCCESS) {
            printf("Could not submit command buffer!\n");
            return result;
        }
        result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
        if(result != VK_SUCCESS) {
            printf("Could not submit command buffer!\n");
            return result;
        }
        vkDestroyFence(vk_context.device.logical_device, fence, NULL);

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
        samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
        samplerCreateInfo.mipLodBias = 0.0;
        samplerCreateInfo.maxAnisotropy = 1.0;
        samplerCreateInfo.anisotropyEnable = VK_FALSE;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 0.0;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        result = vkCreateSampler(vk_context.device.logical_device, &samplerCreateInfo, NULL, &vk_context.cubemap_sampler);
        if(result != VK_SUCCESS) {
            printf("Could not submit command buffer!\n");
            return result;
        }

        // Create image view
        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.subresourceRange = image_subresource_range;
        viewCreateInfo.subresourceRange.layerCount = 6;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.image = vk_context.cubemap.image;
        result = vkCreateImageView(vk_context.device.logical_device, &viewCreateInfo, NULL, &vk_context.cubemap.image_view);
        if(result != VK_SUCCESS) {
            printf("Could not create image view!\n");
            return result;
        }

        // Clean up staging resources
        vkFreeMemory(vk_context.device.logical_device, staging.memory, NULL);
        vkDestroyBuffer(vk_context.device.logical_device, staging.buffer, NULL);


    }
    return result;
}

VkResult create_image(Device* device, ImageBuffer* image_buffer, 
                        uint32_t width, uint32_t height, 
                        VkImageUsageFlags usage, VkImageAspectFlags aspect, uint32_t mip_levels) {

    VkResult result;
    VkExtent3D extent;
    extent.width = width;
    extent.height = height;
    extent.depth = 1;
    VkImageCreateInfo image_create_info;
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = image_buffer->format;
    image_create_info.extent = extent;
    image_create_info.mipLevels = mip_levels;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = usage;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    result = vkCreateImage(device->logical_device, &image_create_info, NULL,
            &image_buffer->image);
    if(result != VK_SUCCESS) {
        printf("Could not create depht image!\n");
        return result;
    }

    VkMemoryRequirements image_memory_requirements;
    vkGetImageMemoryRequirements(device->logical_device, image_buffer->image,
            &image_memory_requirements);
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(device->physical_device, &memory_properties);

    for(uint32_t k = 0; k < memory_properties.memoryTypeCount; k++) {
        if((image_memory_requirements.memoryTypeBits & (1<<k)) &&
           (memory_properties.memoryTypes[k].propertyFlags &
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {

            VkMemoryAllocateInfo memory_allocate_info;
            memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memory_allocate_info.pNext = NULL;
            memory_allocate_info.allocationSize = image_memory_requirements.size;
            memory_allocate_info.memoryTypeIndex = k;

            result = vkAllocateMemory(device->logical_device, &memory_allocate_info, NULL,
                    &image_buffer->memory);
            if(result != VK_SUCCESS) {
                printf("Could not allocate memory!\n");
                return result;
            }
        }
    }
    result = vkBindImageMemory(device->logical_device, image_buffer->image,
            image_buffer->memory, 0);
    if(result != VK_SUCCESS) {
        printf("Could not bind memory image!\n");
        return result;
    }
    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = aspect;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 1;

    VkImageViewCreateInfo image_view_create_info;
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = NULL;
    image_view_create_info.flags = 0;
    image_view_create_info.image = image_buffer->image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = image_buffer->format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceRange = image_subresource_range;

    result = vkCreateImageView(device->logical_device, &image_view_create_info, NULL,
            &image_buffer->image_view);
    if(result != VK_SUCCESS) {
        printf("Could not depth image view!\n");
        return result;
    }
    return result;
}

VkResult create_buffer(VkPhysicalDevice physical_device, VkDevice device, Buffer* buffer, VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags memory_properties_flags, VkDeviceSize size) {
		buffer->size = size;
		VkBufferCreateInfo buffer_create_info;
		buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_create_info.pNext = NULL;
		buffer_create_info.flags = 0;
		buffer_create_info.size = buffer->size;
		buffer_create_info.usage = usage;
		buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer_create_info.queueFamilyIndexCount = 0;
		buffer_create_info.pQueueFamilyIndices = NULL;

		VkResult result = vkCreateBuffer(device, &buffer_create_info, NULL,
				&buffer->buffer);
		if(result != VK_SUCCESS) {
			printf("Could not create buffer!\n");
			return result;
		}

		VkMemoryRequirements buffer_memory_requirements;
		vkGetBufferMemoryRequirements(device, buffer->buffer,
				&buffer_memory_requirements);
        VkMemoryAllocateInfo memory_allocate_info;
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = NULL;
        memory_allocate_info.allocationSize = image_memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = get_memory_type(buffer_memory_requirements, memory_properties_flags);
        result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
                &buffer->memory);

		result = vkBindBufferMemory(device, buffer->buffer,
				buffer->memory, 0);
		if(result != VK_SUCCESS) {
			printf("Could not bind memory for a uniform buffer!\n");
		}
        return result;
}

VkResult create_swapchain( uint32_t width, uint32_t height) {
		VkSurfaceCapabilitiesKHR surface_capabilities;
        VkResult result;
		result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_context.device.physical_device,
				vk_context.device.presentation_surface, &surface_capabilities);
		if (result != VK_SUCCESS) {
			printf("Could not check presentation surface capabilities!\n");
			return result;
		}

		uint32_t formats_count;
		result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk_context.device.physical_device,
				vk_context.device.presentation_surface, &formats_count, NULL);
		VkSurfaceFormatKHR *surface_formats = (VkSurfaceFormatKHR *)malloc(formats_count *
				sizeof(VkSurfaceFormatKHR));
		result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk_context.device.physical_device,
				vk_context.device.presentation_surface, &formats_count, surface_formats);

		if (result != VK_SUCCESS) {
			printf("Error occurred during presentation surface formats enumeration!\n");
			return result;
		}

		uint32_t present_modes_count;
		result = vkGetPhysicalDeviceSurfacePresentModesKHR(vk_context.device.physical_device,
				vk_context.device.presentation_surface, &present_modes_count, NULL);
		VkPresentModeKHR *present_modes = (VkPresentModeKHR *)malloc(present_modes_count *
				sizeof(VkPresentModeKHR));
		result = vkGetPhysicalDeviceSurfacePresentModesKHR(vk_context.device.physical_device,
				vk_context.device.presentation_surface, &present_modes_count, present_modes);

		if (result != VK_SUCCESS) {
			printf("Error occurred during presentation surface present modes enumeration!\n");
			return result;
		}

		uint32_t image_count = surface_capabilities.minImageCount + 1;
		if( (surface_capabilities.maxImageCount > 0) && (image_count >
					surface_capabilities.maxImageCount) ) {
			image_count = surface_capabilities.maxImageCount;
		}

		VkSurfaceFormatKHR surface_format = surface_formats[0];
		if( (formats_count == 1) && (surface_formats[0].format = VK_FORMAT_UNDEFINED) ) {
			surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
			surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		}

		for (int i = 0; i < formats_count; i++) {
			if(surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
				surface_format = surface_formats[i];
				break;
			}
		}

		/* Clamp width and height */
		VkExtent2D swap_chain_extent = { width, height };
		if (surface_capabilities.currentExtent.width == -1) {
			if (swap_chain_extent.width < surface_capabilities.minImageExtent.width )
				swap_chain_extent.width = surface_capabilities.minImageExtent.width;
			if (swap_chain_extent.height < surface_capabilities.minImageExtent.height)
				swap_chain_extent.height = surface_capabilities.minImageExtent.height;
			if (swap_chain_extent.width > surface_capabilities.maxImageExtent.width)
				swap_chain_extent.width = surface_capabilities.maxImageExtent.width;
			if (swap_chain_extent.height > surface_capabilities.maxImageExtent.height)
				swap_chain_extent.height = surface_capabilities.maxImageExtent.height;
		} else {
			swap_chain_extent = surface_capabilities.currentExtent;
		}

		VkImageUsageFlagBits swap_chain_usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
			swap_chain_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if (surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
			swap_chain_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		VkSurfaceTransformFlagBitsKHR swap_chain_transform_flags;
		if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			swap_chain_transform_flags = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		} else {
			swap_chain_transform_flags = surface_capabilities.currentTransform;
		}

		VkPresentModeKHR present_mode;
		for(uint32_t i = 0; i < present_modes_count; i++) {
			if (present_modes[i] == VK_PRESENT_MODE_FIFO_KHR)
				present_mode = present_modes[i];
		}
		for(uint32_t i = 0; i < present_modes_count; i++) {
			if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				present_mode = present_modes[i];
		}
        free(present_modes);

		VkSwapchainCreateInfoKHR swap_chain_create_info;
		swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swap_chain_create_info.pNext = NULL;
		swap_chain_create_info.flags = 0;
		swap_chain_create_info.surface = vk_context.device.presentation_surface;
		swap_chain_create_info.minImageCount = image_count;
		swap_chain_create_info.imageFormat = surface_format.format;
		swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
		swap_chain_create_info.imageExtent = swap_chain_extent;
		swap_chain_create_info.imageArrayLayers = 1;
		swap_chain_create_info.imageUsage = swap_chain_usage_flags;
		swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swap_chain_create_info.queueFamilyIndexCount = 0;
		swap_chain_create_info.pQueueFamilyIndices = NULL;
		swap_chain_create_info.preTransform = swap_chain_transform_flags;
		swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swap_chain_create_info.presentMode = present_mode;
		swap_chain_create_info.clipped = VK_TRUE;
		swap_chain_create_info.oldSwapchain = vk_context.swapchain.swapchain;
		VkSwapchainKHR old_swapchain = vk_context.swapchain.swapchain;

		result = vkCreateSwapchainKHR(vk_context.device.logical_device, &swap_chain_create_info, NULL,
				&vk_context.swapchain.swapchain);
		if(result != VK_SUCCESS) {
			printf("Could not create swap chain!\n");
			return result;
		}
		if(old_swapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(vk_context.device.logical_device, old_swapchain, NULL);
		}

		vkGetSwapchainImagesKHR(vk_context.device.logical_device, vk_context.swapchain.swapchain, &image_count, NULL);
		vkGetSwapchainImagesKHR(vk_context.device.logical_device, vk_context.swapchain.swapchain, &image_count,
				vk_context.swapchain.images);

		VkImageSubresourceRange image_subresource_range;
		image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_subresource_range.baseMipLevel = 0;
		image_subresource_range.levelCount = 1;
		image_subresource_range.baseArrayLayer = 0;
		image_subresource_range.layerCount = 1;

		for (uint32_t i = 0; i < image_count; i++) {
			VkImageViewCreateInfo image_create_info;
			image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			image_create_info.pNext = NULL;
			image_create_info.flags = 0;
			image_create_info.image = vk_context.swapchain.images[i];
			image_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			image_create_info.format = surface_format.format;
			image_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			image_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			image_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			image_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			image_create_info.subresourceRange = image_subresource_range;

			result = vkCreateImageView(vk_context.device.logical_device, &image_create_info, NULL,
					&vk_context.swapchain.image_views[i]);
			if(result != VK_SUCCESS) {
				printf("Could not create swap chain!\n");
				return result;
			}
		}
		vk_context.swapchain.format = surface_format;
        return result;
}

VkResult copy_image_staged(ImageBuffer* dst_buffer, Buffer* staging_buffer, VkImageAspectFlags aspect,
                            void* data, uint32_t width, uint32_t height) {

        uint32_t size = width * height * 4;
		void *staging_buffer_memory_pointer;
		VkResult result = vkMapMemory(vk_context.device.logical_device, staging_buffer->memory, 0,
				size, 0, &staging_buffer_memory_pointer);
		if(result != VK_SUCCESS) {
			printf("Could not map memory and upload data to a index buffer!\n");
			return false;
		}
		memcpy(staging_buffer_memory_pointer, data, size);

		vkUnmapMemory(vk_context.device.logical_device, staging_buffer->memory);

		VkImageSubresourceRange image_subresource_range;
        image_subresource_range.aspectMask = aspect;
		image_subresource_range.baseMipLevel = 0;
		image_subresource_range.levelCount = 1;
		image_subresource_range.baseArrayLayer = 0;
		image_subresource_range.layerCount = 1;
        VkCommandBuffer cmdbuffer = vk_context.copy_cmdbuffer;;

		VkCommandBufferBeginInfo command_buffer_begin_info;
		command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_begin_info.pNext = NULL;
		command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		command_buffer_begin_info.pInheritanceInfo = NULL;

		vkBeginCommandBuffer(cmdbuffer, &command_buffer_begin_info);

		VkImageMemoryBarrier image_memory_barrier_from_undefined_to_transfer_dst;
		image_memory_barrier_from_undefined_to_transfer_dst.sType =
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		image_memory_barrier_from_undefined_to_transfer_dst.pNext = NULL;
		image_memory_barrier_from_undefined_to_transfer_dst.srcAccessMask = 0;
		image_memory_barrier_from_undefined_to_transfer_dst.dstAccessMask =
			VK_ACCESS_TRANSFER_WRITE_BIT;
		image_memory_barrier_from_undefined_to_transfer_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_memory_barrier_from_undefined_to_transfer_dst.newLayout =
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_memory_barrier_from_undefined_to_transfer_dst.srcQueueFamilyIndex =
			VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier_from_undefined_to_transfer_dst.dstQueueFamilyIndex =
			VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier_from_undefined_to_transfer_dst.image = dst_buffer->image;
		image_memory_barrier_from_undefined_to_transfer_dst.subresourceRange =
			image_subresource_range;
		vkCmdPipelineBarrier(cmdbuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
				NULL, 1, &image_memory_barrier_from_undefined_to_transfer_dst);

		VkImageSubresourceLayers image_subresource_layers;
		image_subresource_layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_subresource_layers.mipLevel = 0;
		image_subresource_layers.baseArrayLayer = 0;
		image_subresource_layers.layerCount = 1;
		VkOffset3D image_offset;
		image_offset.x = 0;
		image_offset.y = 0;
		image_offset.z = 0;
		VkExtent3D image_extent;
		image_extent.width = width;
		image_extent.height = height;
		image_extent.depth = 1;
		VkBufferImageCopy buffer_image_copy_info;
		buffer_image_copy_info.bufferOffset = 0;
		buffer_image_copy_info.bufferRowLength = 0;
		buffer_image_copy_info.bufferImageHeight = 0;
		buffer_image_copy_info.imageSubresource = image_subresource_layers;
		buffer_image_copy_info.imageOffset = image_offset;
		buffer_image_copy_info.imageExtent = image_extent;
		vkCmdCopyBufferToImage(cmdbuffer, staging_buffer->buffer,
				dst_buffer->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
				&buffer_image_copy_info);
		VkImageMemoryBarrier image_memory_barrier_from_transfer_to_shader_read;
		image_memory_barrier_from_transfer_to_shader_read.sType =
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		image_memory_barrier_from_transfer_to_shader_read.pNext = NULL;
		image_memory_barrier_from_transfer_to_shader_read.srcAccessMask =
			VK_ACCESS_TRANSFER_WRITE_BIT;
		image_memory_barrier_from_transfer_to_shader_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		image_memory_barrier_from_transfer_to_shader_read.oldLayout =
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		image_memory_barrier_from_transfer_to_shader_read.newLayout =
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_memory_barrier_from_transfer_to_shader_read.srcQueueFamilyIndex =
			VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier_from_transfer_to_shader_read.dstQueueFamilyIndex =
			VK_QUEUE_FAMILY_IGNORED;
		image_memory_barrier_from_transfer_to_shader_read.image = dst_buffer->image;
		image_memory_barrier_from_transfer_to_shader_read.subresourceRange =
			image_subresource_range;
		vkCmdPipelineBarrier( cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
				&image_memory_barrier_from_transfer_to_shader_read);
		vkEndCommandBuffer(cmdbuffer);

		VkSubmitInfo submit_info;
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = NULL;
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = NULL;
		submit_info.pWaitDstStageMask = NULL;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmdbuffer;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = NULL;

        VkFenceCreateInfo fence_create_info;
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = 0;
        fence_create_info.pNext = NULL;
        VkFence fence;
        vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL, &fence);

        result = vkQueueSubmit(vk_context.vk_graphic_queue, 1, &submit_info, fence);
        if(result != VK_SUCCESS) {
            printf("Could not submit command buffer!\n");
            return result;
        }
        result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
        if(result != VK_SUCCESS) {
            printf("Could not submit command buffer!\n");
            return result;
        }
        vkDestroyFence(vk_context.device.logical_device, fence, NULL);

        return result;
}

VkResult copy_buffer_staged(Buffer* dst_buffer, Buffer* staging_buffer, void* data, uint32_t size) {
    void *staging_buffer_memory_pointer;
    VkResult result;
    result = vkMapMemory(vk_context.device.logical_device, staging_buffer->memory, 0,
            staging_buffer->size, 0, &staging_buffer_memory_pointer);
    if(result != VK_SUCCESS) {
        printf("Could not map memory and upload data to a vertex buffer!\n");
        return result;
    }
    memcpy(staging_buffer_memory_pointer, data, size);
    
    vkUnmapMemory(vk_context.device.logical_device, staging_buffer->memory);
    VkCommandBuffer cmdbuffer = vk_context.copy_cmdbuffer;;

    VkCommandBufferBeginInfo command_buffer_begin_info;
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = NULL;
    command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    command_buffer_begin_info.pInheritanceInfo = NULL;

    vkBeginCommandBuffer(cmdbuffer, &command_buffer_begin_info);
    VkBufferCopy buffer_copy_info;
    buffer_copy_info.srcOffset = 0;
    buffer_copy_info.dstOffset = 0;
    buffer_copy_info.size = size;
    vkCmdCopyBuffer(cmdbuffer, staging_buffer->buffer,
            dst_buffer->buffer, 1, &buffer_copy_info);

    vkEndCommandBuffer(cmdbuffer);

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmdbuffer;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;

    VkFenceCreateInfo fence_create_info;
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = 0;
    fence_create_info.pNext = NULL;
    VkFence fence;
    vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL, &fence);

    result = vkQueueSubmit(vk_context.vk_graphic_queue, 1, &submit_info, fence);
    if(result != VK_SUCCESS) {
        printf("Could not submit command buffer!\n");
        return result;
    }
    result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
    if(result != VK_SUCCESS) {
        printf("Could not submit command buffer!\n");
        return result;
    }
    vkDestroyFence(vk_context.device.logical_device, fence, NULL);

    return result;

}

MeshData generate_vertex_data() {
    scene = scene_importer();
    MeshData planet;
    return scene.mesh_data[0];
    float radiusxy = 6378137;
    float radiusz =  6356752.314245;
    int sector_count = 50;
    int stack_count = 50;
    float x, y, z, xy;
    float nx, ny, nz;
    float length_invxy = 1.0 / (radiusxy * radiusxy);
    float length_invz = 1.0 / (radiusz * radiusz);
    float s, t;

    float sector_step = 2 * SX_PI / sector_count;
    float stack_step = SX_PI / stack_count;
    float sector_angle, stack_angle;
    planet.vertex_size = (sector_count + 1) * (stack_count + 1) * 8 * sizeof(float);
    planet.index_size = ((sector_count) * (stack_count + 1) - 1) * 6 * sizeof(uint32_t);
    planet.vertex_data = sx_malloc(sx_alloc_malloc(), planet.vertex_size);
    planet.index_data = sx_malloc(sx_alloc_malloc(), planet.index_size);
    int offset = 0;
    for (int i = 0; i <= stack_count; i++) {
        stack_angle = SX_PI / 2.0 - i * stack_step;
        xy = radiusxy * sx_cos(stack_angle);
        z = radiusz * sx_sin(stack_angle);
        for (int j = 0; j <= sector_count; j++) {
            sector_angle = j * sector_step;
            x = xy * sx_cos(sector_angle);
            y = xy * sx_sin(sector_angle);
            planet.vertex_data[offset + 0] = x;
            planet.vertex_data[offset + 1] = z;
            planet.vertex_data[offset + 2] = y;

            sx_vec3 ns; 
            ns.x = x * length_invxy;
            ns.y = y * length_invxy;
            ns.z = z * length_invz;
            ns = sx_vec3_norm(ns);
            nx = ns.x;
            ny = ns.y;
            nz = ns.z;
            planet.vertex_data[offset + 3] = nx;
            planet.vertex_data[offset + 4] = nz;
            planet.vertex_data[offset + 5] = ny;

            s = (float)j / sector_count;
            t = (float)i / stack_count;
            planet.vertex_data[offset + 6] = s;
            planet.vertex_data[offset + 7] = t;
            offset += 8;
        }
    }

    offset = 0;
    uint32_t k1, k2;
    for (int i = 0; i < stack_count; i++) {
        k1 = i * (sector_count + 1);
        k2 = k1 + sector_count + 1;
        for (int j = 0; j < sector_count; j++, k1++, k2++) {
            if (i != 0) {
                planet.index_data[offset + 0] = k1;
                planet.index_data[offset + 1] = k2;
                planet.index_data[offset + 2] = k1 + 1;
                offset += 3;
            }
            if (i != stack_count - 1) {
                planet.index_data[offset + 0] = k1 + 1;
                planet.index_data[offset + 1] = k2;
                planet.index_data[offset + 2] = k2 + 1;
                offset += 3;
            }
        }
    }

    return planet;
}

Texture create_texture(const char* filename, TextureSampler sampler, bool gen_mip) {
    Texture texture;
    int width, height, channels = 4;
    /*stbi_set_flip_vertically_on_load(true);*/
    uint8_t *img_data = stbi_load(filename, &width, &height, &channels, 4);
    /*uint8_t *img_data = stbi_load("intel-truck-1.png", &width, &height, &channels, 0);*/
    channels = 4;
    if(img_data == NULL) {
        printf("Could not load texture\n");
    }

    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    texture.image_buffer.format = VK_FORMAT_R8G8B8A8_UNORM;
    VkResult result = create_image(&vk_context.device, &texture.image_buffer, width, height, usage, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    if(result != VK_SUCCESS) {
        printf("Could not create texture image!\n");
    }

    copy_image_staged(&texture.image_buffer, &vk_context.staging_buffer, VK_IMAGE_ASPECT_COLOR_BIT, img_data, width, height);
    {
        VkSamplerCreateInfo sampler_create_info;
        sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_create_info.pNext = NULL;
        sampler_create_info.flags = 0;
        sampler_create_info.magFilter = sampler.mag_filter;
        sampler_create_info.minFilter = sampler.min_filter;
        sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_create_info.addressModeU = sampler.address_modeU;
        sampler_create_info.addressModeV = sampler.address_modeV;
        sampler_create_info.addressModeW = sampler.address_modeW;
        sampler_create_info.mipLodBias = 0.0;
        sampler_create_info.anisotropyEnable = VK_FALSE;
        sampler_create_info.maxAnisotropy = 1.0;
        sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_create_info.compareEnable = VK_FALSE;
        sampler_create_info.minLod = 0.0;
        sampler_create_info.maxLod = 0.0;
        sampler_create_info.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        sampler_create_info.unnormalizedCoordinates = VK_FALSE;

        result = vkCreateSampler(vk_context.device.logical_device, &sampler_create_info, NULL,
                &texture.sampler);
        if(result != VK_SUCCESS) {
            printf("Could not submit command buffer!\n");
        }
    }

    return texture;
}
