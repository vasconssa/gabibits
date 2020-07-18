#include "world/renderer.h"

#include <stdio.h>
/*#include "vulkan/vulkan_core.h"*/
#include "sx/os.h"
#include "sx/allocator.h"
#include "sx/math.h"
#include "sx/string.h"
#include "device/vk_device.h"
#include "camera.h"
#include "vulkan/vulkan_core.h"
#include "sx/io.h"
#include "resource/gltf_importer.h"
#include "world/terrain_system.h"
#include "world/gui.h"

#define VOLK_IMPLEMENTATION
#include "volk/volk.h"

#define MAX_NKTEXTURES 8

#define DDSKTX_IMPLEMENT
#define DDSKTX_API static
#define ddsktx_memcpy(_dst, _src, _size)    sx_memcpy((_dst), (_src), (_size))
#define ddsktx_memset(_dst, _v, _size)      sx_memset((_dst), (_v), (_size))
#define ddsktx_assert(_a)                   sx_assert((_a))
#define ddsktx_strcpy(_dst, _src)           sx_strcpy((_dst), sizeof(_dst), (_src))
SX_PRAGMA_DIAGNOSTIC_PUSH()
SX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-function")
#include "dds-ktx/dds-ktx.h"
SX_PRAGMA_DIAGNOSTIC_POP()

/*#define STB_IMAGE_IMPLEMENTATION*/
/*#include "stb/stb_image.h"*/
    


/*#define VK_IMPORT_FUNC(_func) PFN_##_func _func*/
/*#define VK_IMPORT_INSTANCE_FUNC VK_IMPORT_FUNC*/
/*#define VK_IMPORT_DEVICE_FUNC   VK_IMPORT_FUNC*/
/*VK_IMPORT*/
/*VK_IMPORT_INSTANCE*/
/*VK_IMPORT_DEVICE*/
/*#undef VK_IMPORT_DEVICE_FUNC*/
/*#undef VK_IMPORT_INSTANCE_FUNC*/
/*#undef VK_IMPORT_FUNC*/

int RENDERING_RESOURCES_SIZE = 2;
int swapchain_image_count = 0;

typedef struct GlobaUBO {
    sx_mat4 projection;
    sx_mat4 view;
    sx_vec4 light_position[4];
    sx_vec4 camera_position;
    sx_vec4 exposure_gama;
} GlobalUBO;


void *vulkan_library;
static RendererContext vk_context;


static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
        void* p_user_data) {
    printf("Validation layer: %s\n\n", p_callback_data->pMessage);

    return VK_FALSE;
}

uint32_t get_memory_type(VkMemoryRequirements image_memory_requirements, VkMemoryPropertyFlags properties);

VkResult create_swapchain( uint32_t width, uint32_t height);

VkResult create_image(Device* device, ImageBuffer* image, 
                      uint32_t width, uint32_t height, 
                      VkImageUsageFlags usage, VkImageAspectFlags aspect, uint32_t mip_levels);

void clear_image(Device* device, ImageBuffer* image);

VkResult create_attachments(RendererContext* context);


/*VkResult create_cubemap(Device* device, Texture* cubemap);*/

VkPipelineShaderStageCreateInfo load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage);

void update_composition_descriptors(RendererContext* context);

VkResult setup_framebuffer(RendererContext* context, bool update_attachments);

VkResult build_commandbuffers(RendererContext* context, Renderer* rd);

VkResult create_command_buffers();
void destroy_command_buffers();

bool vk_renderer_draw(RendererContext* context, Renderer* rd, float dt);

enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
/* bool update_uniform_buffer() {{{*/
bool update_uniform_buffer() {
        GlobalUBO ubo;
        /*orth_proj_matrix(proj_matrix, -WIDTH/2, WIDTH/2, -HEIGHT/2, HEIGHT/2, 1000, -1000);*/
        ubo.projection = perspective_mat((Camera*)&device()->camera);
        /*ubo.view = sx_mat4_ident();*/
        ubo.view = view_mat((Camera*)&device()->camera);
        sx_vec3 pos = device()->camera.cam.pos;
        ubo.camera_position = sx_vec4f(pos.x, pos.y, pos.z, 1.0);
        ubo.exposure_gama = sx_vec4f(0.5, 0.5, 0.5, 0.5);
        ubo.light_position[0] = sx_vec4f(2000.0, 2000.0, 2000.0, 0.0);
        ubo.light_position[1] = sx_vec4f(0.0, 10000.0, 0.0, 0.0);
        ubo.light_position[2] = sx_vec4f(0.0, 10000.0, 0.0, 0.0);
        ubo.light_position[3] = sx_vec4f(0.0, 10000.0, 0.0, 0.0);
        /*ubo.view.col4 = SX_VEC4_ZERO;*/
        /*memcpy(&ubo.view, &vk_context.camera->view, 16 * sizeof(float));*/
        /*mat4_transpose(res_matrix, m);*/
        copy_buffer(&vk_context.global_uniform_buffer,  &ubo, sizeof(ubo));

        return true;
}
/*}}}*/

/*bool vk_renderer_init(DeviceWindow win) {{{*/
bool vk_renderer_init(DeviceWindow win) {
    vk_context.width = win.width;
    vk_context.height = win.height;
    /*vulkan_library = sx_os_dlopen("libvulkan.so");*/
    /*sx_assert_rel(vulkan_library != NULL);*/
    volkInitialize();

    bool imported = true;

/*#define VK_IMPORT_FUNC( _func )                               \*/
    /*_func = (PFN_##_func)sx_os_dlsym(vulkan_library, #_func);       \*/
    /*imported &= NULL != _func;*/
    /*VK_IMPORT*/
/*#undef VK_IMPORT_FUNC*/

        sx_assert_rel(imported);

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
    const sx_alloc* alloc = sx_alloc_malloc();

    VkResult result;
    /* Instance Creation {{{ */
    {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);
        VkLayerProperties *layer_properties;
        layer_properties = sx_malloc(alloc, (layer_count * sizeof(*layer_properties)));
        vkEnumerateInstanceLayerProperties(&layer_count, layer_properties);
        for (uint32_t i = 0; i < num_layers; i++) {
            bool has_layer = false;
            for (uint32_t j = 0; j < layer_count; j++) {
                if (strcmp(validation_layers[i], layer_properties[j].layerName) == 0) {
                    has_layer = true;
                    break;
                }
            }
            if (!has_layer) {
                return false;
            }
        }
        sx_free(alloc, layer_properties);


        uint32_t extension_count = 0;
        vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
        VkExtensionProperties *extension_properties;
        extension_properties = sx_malloc(alloc, extension_count *
                sizeof(*extension_properties));
        vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extension_properties);

        for (uint32_t i = 0; i < num_extensions; i++)
        {
            bool has_extension = false;
            for(uint32_t j = 0; j < extension_count; j++)
            {
                if (strcmp(extensions[i], extension_properties[j].extensionName) == 0)
                {
                    has_extension = true;
                    break;
                }
            }
            if (!has_extension)
            {
                return false;
            }
        }
        sx_free(alloc, extension_properties);

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
        app_info.pApplicationName = "Gabibits Wingsuit";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "Gabibits Wingsuit";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo inst_create_info;
        inst_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&dbg_create_info;
        /*inst_create_info.pNext = NULL;*/
        inst_create_info.flags = 0;
        inst_create_info.pApplicationInfo = &app_info;
        inst_create_info.enabledLayerCount = num_layers;
        inst_create_info.ppEnabledLayerNames = validation_layers;
        inst_create_info.enabledExtensionCount = num_extensions;
        inst_create_info.ppEnabledExtensionNames = extensions;

        result = vkCreateInstance(&inst_create_info, NULL, &vk_context.vk_instance);
        sx_assert_rel(result == VK_SUCCESS && "Could not create instance");
        volkLoadInstance(vk_context.vk_instance);

/*#define VK_IMPORT_INSTANCE_FUNC( _func )                           \*/
        /*_func = (PFN_##_func)vkGetInstanceProcAddr(vk_context.vk_instance, #_func);       \*/
        /*imported &= NULL != _func;*/
        /*VK_IMPORT_INSTANCE*/
/*#undef VK_IMPORT_INSTANCE_FUNC*/

            sx_assert_rel(imported && "Could not import instance functions");


        result = vkCreateDebugUtilsMessengerEXT(vk_context.vk_instance, &dbg_create_info, NULL, &vk_context.vk_debugmessenger);
        sx_assert_rel(result == VK_SUCCESS && "Could not create Debug Utils Messenger");
    }
    /* }}} */

    /* Initiate swap chain {{{*/
    {
        VkXcbSurfaceCreateInfoKHR surface_create_info;
        surface_create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        surface_create_info.pNext = NULL;
        surface_create_info.flags = 0;
        surface_create_info.connection = win.connection;
        surface_create_info.window = win.window;

        result = vkCreateXcbSurfaceKHR( vk_context.vk_instance, &surface_create_info, NULL,
                &vk_context.device.presentation_surface);
        sx_assert_rel(result == VK_SUCCESS && "Could not create swap chain");
    }
    /*}}}*/

    /* Logical Device creation {{{*/
    {
        uint32_t physical_device_count;
        result = vkEnumeratePhysicalDevices(vk_context.vk_instance, &physical_device_count, NULL);
        sx_assert_rel(result == VK_SUCCESS && "Could not create physical device");

        VkPhysicalDevice *physical_devices = sx_malloc(alloc, physical_device_count *
                sizeof(*physical_devices));
        result = vkEnumeratePhysicalDevices(vk_context.vk_instance, 
                &physical_device_count, physical_devices);

        vk_context.device.physical_device = physical_devices[0];
        for (uint32_t i = 0; i < physical_device_count; i++)
        {
            VkPhysicalDeviceProperties device_props;
            vkGetPhysicalDeviceProperties(physical_devices[i], &device_props);
            if (device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                vk_context.device.physical_device = physical_devices[i];
            }
        }
        sx_free(alloc, physical_devices);

        uint32_t extension_count = 0;

        vkEnumerateDeviceExtensionProperties(vk_context.device.physical_device, NULL, &extension_count,
                NULL);
        VkExtensionProperties *available_extensions;
        available_extensions = sx_malloc(alloc, extension_count *
                sizeof(*available_extensions));
        vkEnumerateDeviceExtensionProperties(vk_context.device.physical_device, NULL, &extension_count,
                available_extensions);

        const char *device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        uint32_t num_device_extensions = 1;

        for (uint32_t i = 0; i < num_device_extensions; i++) {
            bool has_extension = false;
            for(uint32_t j = 0; j < extension_count; j++) {
                if (strcmp(device_extensions[i], available_extensions[j].extensionName) == 0) {
                    has_extension = true;
                    break;
                }
            }
            sx_assert_rel(has_extension);
        }
        sx_free(alloc, available_extensions);

        /* Choose queues */
        uint32_t queue_family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(vk_context.device.physical_device, &queue_family_count, NULL);
        VkQueueFamilyProperties *queue_family_properties = sx_malloc(alloc,
                queue_family_count * sizeof(*queue_family_properties));
        vkGetPhysicalDeviceQueueFamilyProperties(vk_context.device.physical_device, &queue_family_count,
                queue_family_properties);
        for (uint32_t i = 0; i < queue_family_count; i++) {
            VkBool32 queue_present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(vk_context.device.physical_device, i,
                    vk_context.device.presentation_surface, &queue_present_support);
            if(queue_present_support) {
                vk_context.present_queue_index = i;
            }
            if (queue_family_properties[i].queueCount >0 &&
                    queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                vk_context.graphic_queue_index = i;
                if(queue_present_support) break;
            }

        }
        for (uint32_t i = 0; i < queue_family_count; i++) {
            if (queue_family_count > 0 &&
                    queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
                vk_context.transfer_queue_index = i;
            }
            if (queue_family_count > 0 &&
                    !(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                break;
            }
        }
        sx_free(alloc, queue_family_properties);

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
        
        VkPhysicalDeviceFeatures device_features = {};
        device_features.tessellationShader = VK_TRUE;
        device_features.samplerAnisotropy = VK_TRUE;
        device_features.fillModeNonSolid = VK_TRUE;


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
        device_create_info.enabledLayerCount = 0;
        device_create_info.ppEnabledLayerNames = NULL;
        device_create_info.pEnabledFeatures = &device_features;

        result = vkCreateDevice(vk_context.device.physical_device, &device_create_info, NULL, &vk_context.device.logical_device);
        sx_assert_rel(result == VK_SUCCESS && "Could not create logical_device");
        volkLoadDevice(vk_context.device.logical_device);

/*#define VK_IMPORT_DEVICE_FUNC( _func )                                       \*/
        /*_func = (PFN_##_func)vkGetDeviceProcAddr(vk_context.device.logical_device, #_func);  \*/
        /*imported &= NULL != _func;*/
        /*VK_IMPORT_DEVICE*/
/*#undef VK_IMPORT_DEVICE_FUNC*/
            sx_assert_rel(imported && "Could not create device functions");

    }
    /*}}}*/

    /*Request queue handles {{{*/
    vkGetDeviceQueue(vk_context.device.logical_device, vk_context.graphic_queue_index, 0,
            &vk_context.vk_graphic_queue);
    vkGetDeviceQueue(vk_context.device.logical_device, vk_context.present_queue_index, 0,
            &vk_context.vk_present_queue);
    vkGetDeviceQueue(vk_context.device.logical_device, vk_context.transfer_queue_index, 0,
            &vk_context.vk_transfer_queue);
    /*}}}*/

    result = create_swapchain(win.width, win.height);
    result = create_attachments(&vk_context);

    /* Command pool creation and command buffer allocation {{{*/
	{
		VkCommandPoolCreateInfo cmd_pool_create_info;
		cmd_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmd_pool_create_info.pNext = NULL;
		cmd_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
									 VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		cmd_pool_create_info.queueFamilyIndex = vk_context.graphic_queue_index;
		result = vkCreateCommandPool(vk_context.device.logical_device, &cmd_pool_create_info, NULL,
				&vk_context.graphic_queue_cmdpool);
        sx_assert_rel(result == VK_SUCCESS && "Could not create command pool!");

        create_command_buffers();
    }
    /*}}}*/

    /* Global Buffer Creation {{{*/ 
    {
        result = create_buffer(&vk_context.global_uniform_buffer,
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(GlobalUBO));
        sx_assert_rel(result == VK_SUCCESS && "Could not create global uniform buffer!");

        create_texture(&vk_context.lut_brdf, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, "misc/empty.ktx");
        create_texture(&vk_context.irradiance_cube, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, "misc/empty.ktx");
        create_texture(&vk_context.prefiltered_cube, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, "misc/empty.ktx");
    }
    /*}}}*/

    /* SkyBox Vertex/Index/Cubemap creation {{{*/
    {
        /* vertex_data {{{*/
        float vertex_data[] = {
                    /*Position - Normal - uv*/
                 1.0,  1.0,  1.0,  0.0, 0.0, 1.0, 1.0, 0.0, /*0*/
                -1.0,  1.0,  1.0,  0.0, 0.0, 1.0, 0.0, 0.0, /*1*/
                -1.0, -1.0,  1.0,  0.0, 0.0, 1.0, 0.0, 1.0, /*2*/
                 1.0, -1.0,  1.0,  0.0, 0.0, 1.0, 1.0, 1.0, /*3*/

                 1.0,  1.0, -1.0,  0.0, 0.0, -1.0, 1.0, 0.0, /*4*/
                -1.0,  1.0, -1.0,  0.0, 0.0, -1.0, 0.0, 0.0, /*5*/
                -1.0, -1.0, -1.0,  0.0, 0.0, -1.0, 0.0, 1.0, /*6*/
                 1.0, -1.0, -1.0,  0.0, 0.0, -1.0, 1.0, 1.0, /*7*/

                 1.0,  1.0,  1.0,  1.0, 0.0, 0.0, 0.0, 0.0, /*8*/
                 1.0, -1.0,  1.0,  1.0, 0.0, 0.0, 0.0, 1.0, /*9*/
                 1.0,  1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 0.0, /*10*/
                 1.0, -1.0, -1.0,  1.0, 0.0, 0.0, 1.0, 1.0, /*11*/

                -1.0,  1.0,  1.0, -1.0, 0.0, 0.0, 0.0, 0.0, /*12*/
                -1.0, -1.0,  1.0, -1.0, 0.0, 0.0, 0.0, 1.0, /*13*/
                -1.0,  1.0, -1.0, -1.0, 0.0, 0.0, 1.0, 0.0, /*14*/
                -1.0, -1.0, -1.0, -1.0, 0.0, 0.0, 1.0, 1.0, /*15*/

                 1.0,  1.0,  1.0,  0.0, 1.0, 0.0, 1.0, 1.0, /*16*/
                 1.0,  1.0, -1.0,  0.0, 1.0, 0.0, 1.0, 0.0, /*17*/
                -1.0,  1.0, -1.0,  0.0, 1.0, 0.0, 0.0, 0.0, /*18*/
                -1.0,  1.0,  1.0,  0.0, 1.0, 0.0, 0.0, 1.0, /*19*/

                 1.0, -1.0,  1.0,  0.0, -1.0, 0.0, 1.0, 1.0, /*20*/
                 1.0, -1.0, -1.0,  0.0, -1.0, 0.0, 1.0, 0.0, /*22*/
                -1.0, -1.0, -1.0,  0.0, -1.0, 0.0, 0.0, 0.0, /*23*/
                -1.0, -1.0,  1.0,  0.0, -1.0, 0.0, 0.0, 1.0, /*24*/
        };
        /*}}}*/

        result = create_buffer(&vk_context.vertex_buffer_sky, 
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(vertex_data));

        sx_assert_rel(result == VK_SUCCESS && "Could not create vertex buffer!");
        result = copy_buffer_staged(&vk_context.vertex_buffer_sky, vertex_data, sizeof(vertex_data));
        sx_assert_rel(result == VK_SUCCESS && "Could not copy vertex buffer!");
	}

    {

        /* index_data {{{*/
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
        /*}}}*/

        result = create_buffer(&vk_context.index_buffer_sky, 
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(index_data));
        sx_assert_rel(result == VK_SUCCESS && "Could not create index buffer!");
        result = copy_buffer_staged(&vk_context.index_buffer_sky, index_data, sizeof(index_data));
        sx_assert_rel(result == VK_SUCCESS && "Could not copy index buffer!");
	}

	{
        /*create_cubemap(&vk_context.device, &vk_context.cubemap_texture);*/
        create_texture(&vk_context.cubemap_texture, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, alloc, "misc/cubemap/sky.ktx");
    }
    /*}}}*/
    

    /* Render pass creation {{{ */
    {
        VkAttachmentDescription attachment_descriptions[6];
        /* Color attchment */
		attachment_descriptions[0].flags = 0;
        attachment_descriptions[0].format = vk_context.swapchain.format.format;
		attachment_descriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        /* Deferred attachments */
        /* position */
		attachment_descriptions[1].flags = 0;
        attachment_descriptions[1].format = vk_context.position_image.format;
		attachment_descriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        /* normal */
		attachment_descriptions[2].flags = 0;
        attachment_descriptions[2].format = vk_context.normal_image.format;
		attachment_descriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        /* albedo */
		attachment_descriptions[3].flags = 0;
        attachment_descriptions[3].format = vk_context.albedo_image.format;
		attachment_descriptions[3].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[3].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        /* metallic roughness */
		attachment_descriptions[4].flags = 0;
        attachment_descriptions[4].format = vk_context.metallic_roughness_image.format;
		attachment_descriptions[4].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[4].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[4].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        /* Depth attachment */
		attachment_descriptions[5].flags = 0;
		attachment_descriptions[5].format = vk_context.depth_image.format;
		attachment_descriptions[5].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[5].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[5].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[5].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[5].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[5].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[5].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;;


        /* Three subpasses */
        VkSubpassDescription subpass_descriptions[3];

        /* First subpass: Fill G-Buffer components */
        VkAttachmentReference color_references[5];
        color_references[0].attachment = 0;
        color_references[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[1].attachment = 1;
        color_references[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[2].attachment = 2;
        color_references[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[3].attachment = 3;
        color_references[3].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[4].attachment = 4;
        color_references[4].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depth_reference[1];
        depth_reference[0].attachment = 5;
        depth_reference[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass_descriptions[0].flags = 0;
        subpass_descriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[0].inputAttachmentCount = 0;
        subpass_descriptions[0].pInputAttachments = NULL;
        subpass_descriptions[0].colorAttachmentCount = 5;
        subpass_descriptions[0].pColorAttachments = color_references;
        subpass_descriptions[0].pDepthStencilAttachment = depth_reference;
        subpass_descriptions[0].pResolveAttachments = NULL;
        subpass_descriptions[0].preserveAttachmentCount = 0;
        subpass_descriptions[0].pPreserveAttachments = NULL;

        /* Second subpass: Final composition (using G-Buffer components */
        VkAttachmentReference color_reference[1];
        color_reference[0].attachment = 0;
        color_reference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference input_references[4];
        input_references[0].attachment = 1;
        input_references[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input_references[1].attachment = 2;
        input_references[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input_references[2].attachment = 3;
        input_references[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        input_references[3].attachment = 4;
        input_references[3].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        uint32_t preserve_attachment_index = 1;
        subpass_descriptions[1].flags = 0;
        subpass_descriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[1].inputAttachmentCount = 4;
        subpass_descriptions[1].pInputAttachments = input_references;
        subpass_descriptions[1].colorAttachmentCount = 1;
        subpass_descriptions[1].pColorAttachments = color_reference;
        subpass_descriptions[1].pDepthStencilAttachment = depth_reference;
        subpass_descriptions[1].pResolveAttachments = NULL;
        subpass_descriptions[1].preserveAttachmentCount = 0;
        subpass_descriptions[1].pPreserveAttachments = NULL;
        
        /* Third subpass: Forward transparency */
        color_reference[0].attachment = 0;
        color_reference[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        input_references[0].attachment = 1;
        input_references[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        subpass_descriptions[2].flags = 0;
        subpass_descriptions[2].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_descriptions[2].inputAttachmentCount = 1;
        subpass_descriptions[2].pInputAttachments = input_references;
        subpass_descriptions[2].colorAttachmentCount = 1;
        subpass_descriptions[2].pColorAttachments = color_reference;
        subpass_descriptions[2].pDepthStencilAttachment = depth_reference;
        subpass_descriptions[2].pResolveAttachments = NULL;
        subpass_descriptions[2].preserveAttachmentCount = 0;
        subpass_descriptions[2].pPreserveAttachments = NULL;

        /* Subpass dependencies for layout transitions */
        VkSubpassDependency dependencies[4];
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT 
                                        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = 1;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[2].srcSubpass = 1;
        dependencies[2].dstSubpass = 2;
        dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[3].srcSubpass = 0;
        dependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[3].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT 
                                        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[3].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo render_pass_create_info;
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.pNext = NULL;
        render_pass_create_info.flags = 0;
        render_pass_create_info.attachmentCount = 6;
        render_pass_create_info.pAttachments = attachment_descriptions;
        render_pass_create_info.subpassCount = 3;
        render_pass_create_info.pSubpasses = subpass_descriptions;
        render_pass_create_info.dependencyCount = 4;
        render_pass_create_info.pDependencies = dependencies;

        result = vkCreateRenderPass(vk_context.device.logical_device,
                &render_pass_create_info, NULL, &vk_context.render_pass);
        sx_assert_rel(result == VK_SUCCESS && "Could not create render pass");
    }
    /* }}} */

    /* Descriptor Sets Creation {{{*/
    {
        /* Descriptor Pools creation {{{*/
        // global pool
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
            result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
                    &vk_context.global_descriptor_pool);
            sx_assert_rel(result == VK_SUCCESS && "Could not create global descriptor pool!");
        }
        // skybox pool
        {
            VkDescriptorPoolSize pool_sizes[1];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_sizes[0].descriptorCount = 1;

            VkDescriptorPoolCreateInfo descriptor_pool_create_info;
            descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool_create_info.pNext = NULL;
            descriptor_pool_create_info.flags = 0;
            descriptor_pool_create_info.maxSets = 1;
            descriptor_pool_create_info.poolSizeCount = 1;
            descriptor_pool_create_info.pPoolSizes = pool_sizes;
            result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
                    &vk_context.skybox_descriptor_pool);
            sx_assert_rel(result == VK_SUCCESS && "Could not create skybox descriptor pool!");
        }
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
            descriptor_pool_create_info.maxSets = 10;
            descriptor_pool_create_info.poolSizeCount = 2;
            descriptor_pool_create_info.pPoolSizes = pool_sizes;
            result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
                    &vk_context.terrain_descriptor_pool);
            sx_assert_rel(result == VK_SUCCESS && "Could not create terrain descriptor pool!");
        }
        // objects pool
        {
            VkDescriptorPoolSize pool_sizes[2];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            pool_sizes[0].descriptorCount = 1;
            pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_sizes[1].descriptorCount = 5;

            VkDescriptorPoolCreateInfo descriptor_pool_create_info;
            descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool_create_info.pNext = NULL;
            descriptor_pool_create_info.flags = 0;
            descriptor_pool_create_info.maxSets = 1000;
            descriptor_pool_create_info.poolSizeCount = 2;
            descriptor_pool_create_info.pPoolSizes = pool_sizes;
            result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
                    &vk_context.objects_descriptor_pool);
            sx_assert_rel(result == VK_SUCCESS && "Could not create objects descriptor pool!");
        }
        // composition pool
        {
            VkDescriptorPoolSize pool_sizes[1];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            pool_sizes[0].descriptorCount = 4;

            VkDescriptorPoolCreateInfo descriptor_pool_create_info;
            descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool_create_info.pNext = NULL;
            descriptor_pool_create_info.flags = 0;
            descriptor_pool_create_info.maxSets = 1;
            descriptor_pool_create_info.poolSizeCount = 1;
            descriptor_pool_create_info.pPoolSizes = pool_sizes;
            result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
                    &vk_context.composition_descriptor_pool);
            sx_assert_rel(result == VK_SUCCESS && "Could not create composition descriptor pool!");
        }
        // transparente pool
        {
            VkDescriptorPoolSize pool_sizes[2];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            pool_sizes[0].descriptorCount = 1;
            pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_sizes[1].descriptorCount = 1;

            VkDescriptorPoolCreateInfo descriptor_pool_create_info;
            descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool_create_info.pNext = NULL;
            descriptor_pool_create_info.flags = 0;
            descriptor_pool_create_info.maxSets = 1;
            descriptor_pool_create_info.poolSizeCount = 1;
            descriptor_pool_create_info.pPoolSizes = pool_sizes;
            result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
                    &vk_context.transparent_descriptor_pool);
            sx_assert_rel(result == VK_SUCCESS && "Could not create composition descriptor pool!");
        }
        // nk_gui pool
        {
            VkDescriptorPoolSize pool_sizes[1];
            pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            pool_sizes[0].descriptorCount = MAX_NKTEXTURES;

            VkDescriptorPoolCreateInfo descriptor_pool_create_info;
            descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptor_pool_create_info.pNext = NULL;
            descriptor_pool_create_info.flags = 0;
            descriptor_pool_create_info.maxSets = 1;
            descriptor_pool_create_info.poolSizeCount = 1;
            descriptor_pool_create_info.pPoolSizes = pool_sizes;
            result = vkCreateDescriptorPool(vk_context.device.logical_device, &descriptor_pool_create_info, NULL,
                    &vk_context.nk_gui_descriptor_pool);
            sx_assert_rel(result == VK_SUCCESS && "Could not create nk_gui descriptor pool!");
        }
        /* }}} */

        /* Descriptor Set Layout Creation {{{*/
        // Global layout
        {
            VkDescriptorSetLayoutBinding layout_bindings[4];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT 
                                            | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;
            layout_bindings[1].binding = 1;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[1].descriptorCount = 1;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[1].pImmutableSamplers = NULL;
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

            result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
                    &descriptor_set_layout_create_info, NULL, &vk_context.global_descriptor_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create global descriptor layout!");
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

            result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
                    &descriptor_set_layout_create_info, NULL, &vk_context.terrain_descriptor_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create terrain descriptor layout!");
        }
        // Skybox layout
        {
            VkDescriptorSetLayoutBinding layout_bindings[1];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;

            VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
            descriptor_set_layout_create_info.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptor_set_layout_create_info.pNext = NULL;
            descriptor_set_layout_create_info.flags = 0;
            descriptor_set_layout_create_info.bindingCount = 1;
            descriptor_set_layout_create_info.pBindings = layout_bindings;

            result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
                    &descriptor_set_layout_create_info, NULL, &vk_context.skybox_descriptor_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create skybox descriptor layout!");
        }
        // Objects layout
        {
            VkDescriptorSetLayoutBinding layout_bindings[6];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;
            layout_bindings[1].binding = 1;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[1].descriptorCount = 1;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[1].pImmutableSamplers = NULL;
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
            layout_bindings[4].binding = 4;
            layout_bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[4].descriptorCount = 1;
            layout_bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[4].pImmutableSamplers = NULL;
            layout_bindings[5].binding = 5;
            layout_bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[5].descriptorCount = 1;
            layout_bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[5].pImmutableSamplers = NULL;

            VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
            descriptor_set_layout_create_info.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptor_set_layout_create_info.pNext = NULL;
            descriptor_set_layout_create_info.flags = 0;
            descriptor_set_layout_create_info.bindingCount = 6;
            descriptor_set_layout_create_info.pBindings = layout_bindings;

            result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
                    &descriptor_set_layout_create_info, NULL, &vk_context.objects_descriptor_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create objects descriptor layout!");
        }
        // Composition layout
        {
            VkDescriptorSetLayoutBinding layout_bindings[4];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;
            layout_bindings[1].binding = 1;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            layout_bindings[1].descriptorCount = 1;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[1].pImmutableSamplers = NULL;
            layout_bindings[2].binding = 2;
            layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            layout_bindings[2].descriptorCount = 1;
            layout_bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[2].pImmutableSamplers = NULL;
            layout_bindings[3].binding = 3;
            layout_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
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

            result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
                    &descriptor_set_layout_create_info, NULL, &vk_context.composition_descriptor_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create composition descriptor layout!");
        }
        // Transparent layout
        {
            VkDescriptorSetLayoutBinding layout_bindings[2];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            layout_bindings[0].descriptorCount = 1;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;
            layout_bindings[1].binding = 1;
            layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[1].descriptorCount = 1;
            layout_bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[1].pImmutableSamplers = NULL;

            VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
            descriptor_set_layout_create_info.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptor_set_layout_create_info.pNext = NULL;
            descriptor_set_layout_create_info.flags = 0;
            descriptor_set_layout_create_info.bindingCount = 2;
            descriptor_set_layout_create_info.pBindings = layout_bindings;

            result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
                    &descriptor_set_layout_create_info, NULL, &vk_context.transparent_descriptor_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create transparent descriptor layout!");
        }
        // Nk gui
        {
            VkDescriptorSetLayoutBinding layout_bindings[1];
            layout_bindings[0].binding = 0;
            layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            layout_bindings[0].descriptorCount = MAX_NKTEXTURES;
            layout_bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            layout_bindings[0].pImmutableSamplers = NULL;

            VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
            descriptor_set_layout_create_info.sType =
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptor_set_layout_create_info.pNext = NULL;
            descriptor_set_layout_create_info.flags = 0;
            descriptor_set_layout_create_info.bindingCount = 1;
            descriptor_set_layout_create_info.pBindings = layout_bindings;

            result = vkCreateDescriptorSetLayout(vk_context.device.logical_device,
                    &descriptor_set_layout_create_info, NULL, &vk_context.nk_gui_descriptor_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create transparent descriptor layout!");
        }
        /* }}}*/

        /* Descriptor set creation {{{*/
        // Global descriptor set
        {
            VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
            descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptor_set_allocate_info.pNext = NULL;
            descriptor_set_allocate_info.descriptorPool = vk_context.global_descriptor_pool;
            descriptor_set_allocate_info.descriptorSetCount = 1;
            descriptor_set_allocate_info.pSetLayouts = &vk_context.global_descriptor_layout;

            result = vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info,
                    &vk_context.global_descriptor_set);
            sx_assert_rel("Could not create global descriptor set!");
        }
        // Skybox descriptor set
        {
            VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
            descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptor_set_allocate_info.pNext = NULL;
            descriptor_set_allocate_info.descriptorPool = vk_context.skybox_descriptor_pool;
            descriptor_set_allocate_info.descriptorSetCount = 1;
            descriptor_set_allocate_info.pSetLayouts = &vk_context.skybox_descriptor_layout;

            result = vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info,
                    &vk_context.skybox_descriptor_set);
            sx_assert_rel("Could not create skybox descriptor set!");
        }
        // Composition descriptor set
        {
            VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
            descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptor_set_allocate_info.pNext = NULL;
            descriptor_set_allocate_info.descriptorPool = vk_context.composition_descriptor_pool;
            descriptor_set_allocate_info.descriptorSetCount = 1;
            descriptor_set_allocate_info.pSetLayouts = &vk_context.composition_descriptor_layout;

            result = vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info,
                    &vk_context.composition_descriptor_set);
            sx_assert_rel("Could not create composition descriptor set!");
        }
        // Transparent descriptor set
        {
            VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
            descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptor_set_allocate_info.pNext = NULL;
            descriptor_set_allocate_info.descriptorPool = vk_context.transparent_descriptor_pool;
            descriptor_set_allocate_info.descriptorSetCount = 1;
            descriptor_set_allocate_info.pSetLayouts = &vk_context.transparent_descriptor_layout;

            result = vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info,
                    &vk_context.transparent_descriptor_set);
            sx_assert_rel("Could not create transparent descriptor set!");
        }
        /*}}}*/
    }
    /* }}}*/

    /*Update Descriptor Sets{{{*/
    {
        /*Global Descriptor update{{{*/
        {
            VkDescriptorBufferInfo buffer_info;
            buffer_info.buffer = vk_context.global_uniform_buffer.buffer;
            buffer_info.offset = 0;
            buffer_info.range = vk_context.global_uniform_buffer.size;

            VkDescriptorImageInfo image_info[3];
            image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info[0].imageView = vk_context.lut_brdf.image_buffer.image_view;
            image_info[0].sampler = vk_context.lut_brdf.sampler;
            image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info[1].imageView = vk_context.irradiance_cube.image_buffer.image_view;
            image_info[1].sampler = vk_context.irradiance_cube.sampler;
            image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info[2].imageView = vk_context.prefiltered_cube.image_buffer.image_view;
            image_info[2].sampler = vk_context.prefiltered_cube.sampler;

            VkWriteDescriptorSet descriptor_writes[4];
            descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].pNext = NULL;
            descriptor_writes[0].dstSet = vk_context.global_descriptor_set;
            descriptor_writes[0].dstBinding = 0;
            descriptor_writes[0].dstArrayElement = 0;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;;
            descriptor_writes[0].pImageInfo = NULL;
            descriptor_writes[0].pBufferInfo = &buffer_info;
            descriptor_writes[0].pTexelBufferView = NULL;

            descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[1].pNext = NULL;
            descriptor_writes[1].dstSet = vk_context.global_descriptor_set;
            descriptor_writes[1].dstBinding = 1;
            descriptor_writes[1].dstArrayElement = 0;
            descriptor_writes[1].descriptorCount = 1;
            descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[1].pImageInfo = &image_info[0];
            descriptor_writes[1].pBufferInfo = NULL;
            descriptor_writes[1].pTexelBufferView = NULL;

            descriptor_writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[2].pNext = NULL;
            descriptor_writes[2].dstSet = vk_context.global_descriptor_set;
            descriptor_writes[2].dstBinding = 2;
            descriptor_writes[2].dstArrayElement = 0;
            descriptor_writes[2].descriptorCount = 1;
            descriptor_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[2].pImageInfo = &image_info[1];
            descriptor_writes[2].pBufferInfo = NULL;
            descriptor_writes[2].pTexelBufferView = NULL;

            descriptor_writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[3].pNext = NULL;
            descriptor_writes[3].dstSet = vk_context.global_descriptor_set;
            descriptor_writes[3].dstBinding = 3;
            descriptor_writes[3].dstArrayElement = 0;
            descriptor_writes[3].descriptorCount = 1;
            descriptor_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[3].pImageInfo = &image_info[2];
            descriptor_writes[3].pBufferInfo = NULL;
            descriptor_writes[3].pTexelBufferView = NULL;

            vkUpdateDescriptorSets(vk_context.device.logical_device, 4, descriptor_writes, 0, NULL);
        }
        /*}}}*/
        /* Sky Descriptor update {{{*/
        {
            VkDescriptorImageInfo image_info[1];
            image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_info[0].imageView = vk_context.cubemap_texture.image_buffer.image_view;
            image_info[0].sampler = vk_context.cubemap_texture.sampler;

            VkWriteDescriptorSet descriptor_writes[1];
            descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor_writes[0].pNext = NULL;
            descriptor_writes[0].dstSet = vk_context.skybox_descriptor_set;
            descriptor_writes[0].dstBinding = 0;
            descriptor_writes[0].dstArrayElement = 0;
            descriptor_writes[0].descriptorCount = 1;
            descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptor_writes[0].pImageInfo = &image_info[0];
            descriptor_writes[0].pBufferInfo = NULL;

            vkUpdateDescriptorSets(vk_context.device.logical_device, 1, descriptor_writes, 0, NULL);
        }
        /*}}}*/
        update_composition_descriptors(&vk_context);
    }
    /*}}}*/

    /* Pipelines creation {{{*/
    {
        /* Common pipeline state {{{*/ 
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

		VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
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

        /*}}}*/
        /* Nk gui pipeline {{{*/
        {
            VkVertexInputBindingDescription vertex_binding_descriptions[1] = {
                {
                    .binding = 0,
                    .stride  = sizeof(nk_vertex),
                    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
                }
            };
            //Position, Normal, uv
            VkVertexInputAttributeDescription vertex_attribute_descriptions[3] = {
                {
                    .location = 0,
                    .binding = vertex_binding_descriptions[0].binding,
                    .format = VK_FORMAT_R32G32_SFLOAT,
                    .offset = offsetof(nk_vertex, position)
                },
                {
                    .location = 1,
                    .binding = vertex_binding_descriptions[0].binding,
                    .format = VK_FORMAT_R32G32_SFLOAT,
                    .offset = offsetof(nk_vertex, uv)
                },
                {
                    .location = 2,
                    .binding = vertex_binding_descriptions[0].binding,
                    .format = VK_FORMAT_R8G8B8A8_UINT,
                    .offset = offsetof(nk_vertex, col)
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

            VkPipelineRasterizationStateCreateInfo rasterization_state_create_info  = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .depthBiasConstantFactor = 0.0,
                .depthBiasClamp = 0.0,
                .depthBiasSlopeFactor = 0.0,
                .lineWidth = 1.0
            };

            VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            };
            VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .attachmentCount = 1,
                .pAttachments = &color_blend_attachment_state
            };

            VkGraphicsPipelineCreateInfo pipeline_create_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .pVertexInputState = &vertex_input_state_create_info,
                .pInputAssemblyState = &input_assembly_state_create_info,
                .pTessellationState = NULL,
                .pViewportState = &viewport_state_create_info,
                .pRasterizationState = &rasterization_state_create_info,
                .pMultisampleState = &multisample_state_create_info,
                .pDepthStencilState = &depth_stencil_state_create_info,
                .pColorBlendState = &color_blend_state_create_info,
                .pDynamicState = &dynamic_state_create_info,
                .renderPass = vk_context.render_pass,
                .subpass = 2,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1
            };

            VkDescriptorSetLayout descriptor_set_layouts[2] = {
                vk_context.global_descriptor_layout, 
                vk_context.nk_gui_descriptor_layout
            };

            VkPushConstantRange push_constants_range = {
                .offset = 0,
                .size = sizeof(GuiPushConstantBlock),
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            };

            VkPipelineLayoutCreateInfo layout_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .setLayoutCount = 2,
                .pSetLayouts = descriptor_set_layouts,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_constants_range
            };

            result = vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL,
                    &vk_context.nk_gui_pipeline_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create nk_gui pipeline layout!");

            pipeline_create_info.layout = vk_context.nk_gui_pipeline_layout;

            VkPipelineShaderStageCreateInfo shader_stages[2];
            shader_stages[0] = load_shader(vk_context.device.logical_device, "shaders/nk_gui.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            shader_stages[1] = load_shader(vk_context.device.logical_device, "shaders/nk_gui.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

            pipeline_create_info.stageCount = 2;
            pipeline_create_info.pStages = shader_stages;

            result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
                    &pipeline_create_info, NULL, &vk_context.nk_gui_pipeline);
            sx_assert_rel(result == VK_SUCCESS && "Could not create nk_gui pipeline!");
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);
        }
        /*}}}*/
        /* Skybox pipeline {{{*/
        {
            VkDescriptorSetLayout descriptor_set_layouts[2] = {
                vk_context.global_descriptor_layout, 
                vk_context.skybox_descriptor_layout
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

            VkPipelineRasterizationStateCreateInfo rasterization_state_create_info  = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .depthBiasConstantFactor = 0.0,
                .depthBiasClamp = 0.0,
                .depthBiasSlopeFactor = 0.0,
                .lineWidth = 1.0
            };

            VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .attachmentCount = 1,
                .pAttachments = color_blend_attachment_state
            };

            VkGraphicsPipelineCreateInfo pipeline_create_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .pVertexInputState = &vertex_input_state_create_info,
                .pInputAssemblyState = &input_assembly_state_create_info,
                .pTessellationState = NULL,
                .pViewportState = &viewport_state_create_info,
                .pRasterizationState = &rasterization_state_create_info,
                .pMultisampleState = &multisample_state_create_info,
                .pDepthStencilState = &depth_stencil_state_create_info,
                .pColorBlendState = &color_blend_state_create_info,
                .pDynamicState = &dynamic_state_create_info,
                .renderPass = vk_context.render_pass,
                .subpass = 2,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1
            };

            result = vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL,
                    &vk_context.skybox_pipeline_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create skybox pipeline layout!");

            pipeline_create_info.layout = vk_context.skybox_pipeline_layout;

            VkPipelineShaderStageCreateInfo shader_stages[2];
            shader_stages[0] = load_shader(vk_context.device.logical_device, "shaders/sky.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            shader_stages[1] = load_shader(vk_context.device.logical_device, "shaders/sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

            pipeline_create_info.stageCount = 2;
            pipeline_create_info.pStages = shader_stages;

            result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
                    &pipeline_create_info, NULL, &vk_context.skybox_pipeline);
            sx_assert_rel(result == VK_SUCCESS && "Could not create skybox pipeline!");
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);
        }
        /*}}}*/
        /* Terrain pipeline {{{*/
        {
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
                .renderPass = vk_context.render_pass,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1
            };
            pipeline_create_info.pTessellationState = &tessellation_create_info;
            pipeline_create_info.pInputAssemblyState = &terrain_assembly_state_create_info;

            VkDescriptorSetLayout descriptor_set_layouts[2] = {
                vk_context.global_descriptor_layout, 
                vk_context.terrain_descriptor_layout
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

            result = vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL,
                    &vk_context.terrain_pipeline_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create terrain pipeline layout!");

            pipeline_create_info.layout = vk_context.terrain_pipeline_layout;

            VkPipelineShaderStageCreateInfo shader_stages[4];
            shader_stages[0] = load_shader(vk_context.device.logical_device, "shaders/terrain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            shader_stages[1] = load_shader(vk_context.device.logical_device, "shaders/terrain.tesc.spv", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
            shader_stages[2] = load_shader(vk_context.device.logical_device, "shaders/terrain.tese.spv", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
            shader_stages[3] = load_shader(vk_context.device.logical_device, "shaders/terrain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

            pipeline_create_info.stageCount = 4;
            pipeline_create_info.pStages = shader_stages;

            result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
                    &pipeline_create_info, NULL, &vk_context.terrain_pipeline);
            sx_assert_rel(result == VK_SUCCESS && "Could not create terrain pipeline!");
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[2].module, NULL);
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[3].module, NULL);
        }
        /*}}}*/
        /* Objects pipeline {{{*/
        {
            VkDescriptorSetLayout descriptor_set_layouts[2] = {
                vk_context.global_descriptor_layout, 
                vk_context.objects_descriptor_layout
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

            VkGraphicsPipelineCreateInfo pipeline_create_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .pVertexInputState = &vertex_input_state_create_info,
                .pInputAssemblyState = &input_assembly_state_create_info,
                .pTessellationState = NULL,
                .pViewportState = &viewport_state_create_info,
                .pRasterizationState = &rasterization_state_create_info,
                .pMultisampleState = &multisample_state_create_info,
                .pDepthStencilState = &depth_stencil_state_create_info,
                .pColorBlendState = &color_blend_state_create_info,
                .pDynamicState = &dynamic_state_create_info,
                .renderPass = vk_context.render_pass,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1
            };
            result = vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL,
                    &vk_context.objects_pipeline_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create objects pipeline layout!");

            pipeline_create_info.layout = vk_context.objects_pipeline_layout;

            VkPipelineShaderStageCreateInfo shader_stages[2];
            shader_stages[0] = load_shader(vk_context.device.logical_device, "shaders/objects.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            shader_stages[1] = load_shader(vk_context.device.logical_device, "shaders/objects.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

            pipeline_create_info.stageCount = 2;
            pipeline_create_info.pStages = shader_stages;

            result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
                    &pipeline_create_info, NULL, &vk_context.objects_pipeline);
            sx_assert_rel(result == VK_SUCCESS && "Could not create objects pipeline!");
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);
        }
        /*}}}*/
        /* Composition pipeline {{{*/
        {
            VkPipelineVertexInputStateCreateInfo empty_vertex_input_state_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .vertexBindingDescriptionCount = 0,
                .pVertexBindingDescriptions = NULL,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions = NULL
            };
            
            VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .depthTestEnable = VK_FALSE,
                .depthWriteEnable = VK_FALSE,
                .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                .depthBoundsTestEnable = VK_FALSE,
                .minDepthBounds = 0.0,
                .maxDepthBounds = 1.0
            };

            VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .attachmentCount = 1,
                .pAttachments = color_blend_attachment_state
            };

            VkPipelineRasterizationStateCreateInfo rasterization_state_create_info  = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
                .depthBiasConstantFactor = 0.0,
                .depthBiasClamp = 0.0,
                .depthBiasSlopeFactor = 0.0,
                .lineWidth = 1.0
            };
            VkGraphicsPipelineCreateInfo pipeline_create_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .pVertexInputState = &vertex_input_state_create_info,
                .pInputAssemblyState = &input_assembly_state_create_info,
                .pTessellationState = NULL,
                .pViewportState = &viewport_state_create_info,
                .pRasterizationState = &rasterization_state_create_info,
                .pMultisampleState = &multisample_state_create_info,
                .pDepthStencilState = &depth_stencil_state_create_info,
                .pColorBlendState = &color_blend_state_create_info,
                .pDynamicState = &dynamic_state_create_info,
                .renderPass = vk_context.render_pass,
                .subpass = 1,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1
            };
            pipeline_create_info.pVertexInputState = &empty_vertex_input_state_create_info;
            pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
            pipeline_create_info.subpass = 1;

            VkDescriptorSetLayout descriptor_set_layouts[2] = {
                vk_context.global_descriptor_layout, 
                vk_context.composition_descriptor_layout
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

            result = vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL,
                    &vk_context.composition_pipeline_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create composition pipeline layout!");

            pipeline_create_info.layout = vk_context.composition_pipeline_layout;

            VkPipelineShaderStageCreateInfo shader_stages[2];
            shader_stages[0] = load_shader(vk_context.device.logical_device, "shaders/composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            shader_stages[1] = load_shader(vk_context.device.logical_device, "shaders/composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

            pipeline_create_info.stageCount = 2;
            pipeline_create_info.pStages = shader_stages;

            result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
                    &pipeline_create_info, NULL, &vk_context.composition_pipeline);
            sx_assert_rel(result == VK_SUCCESS && "Could not create composition pipeline!");
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);
        }
        /*}}}*/
        /* Transparent pipeline {{{*/
        {
            VkPipelineVertexInputStateCreateInfo empty_vertex_input_state_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .vertexBindingDescriptionCount = 0,
                .pVertexBindingDescriptions = NULL,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions = NULL
            };
            VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            };
            VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .attachmentCount = 1,
                .pAttachments = &color_blend_attachment_state
            };

            VkGraphicsPipelineCreateInfo pipeline_create_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .pVertexInputState = &empty_vertex_input_state_create_info,
                .pInputAssemblyState = &input_assembly_state_create_info,
                .pTessellationState = NULL,
                .pViewportState = &viewport_state_create_info,
                .pRasterizationState = &rasterization_state_create_info,
                .pMultisampleState = &multisample_state_create_info,
                .pDepthStencilState = &depth_stencil_state_create_info,
                .pColorBlendState = &color_blend_state_create_info,
                .pDynamicState = &dynamic_state_create_info,
                .renderPass = vk_context.render_pass,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1
            };
            pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
            pipeline_create_info.subpass = 2;

            VkDescriptorSetLayout descriptor_set_layouts[2] = {
                vk_context.global_descriptor_layout, 
                vk_context.transparent_descriptor_layout
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

            result = vkCreatePipelineLayout(vk_context.device.logical_device, &layout_create_info, NULL,
                    &vk_context.transparent_pipeline_layout);
            sx_assert_rel(result == VK_SUCCESS && "Could not create transparent pipeline layout!");

            pipeline_create_info.layout = vk_context.transparent_pipeline_layout;

            VkPipelineShaderStageCreateInfo shader_stages[2];
            shader_stages[0] = load_shader(vk_context.device.logical_device, "shaders/transparent.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
            shader_stages[1] = load_shader(vk_context.device.logical_device, "shaders/transparent.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

            pipeline_create_info.stageCount = 2;
            pipeline_create_info.pStages = shader_stages;

            result = vkCreateGraphicsPipelines(vk_context.device.logical_device, VK_NULL_HANDLE, 1,
                    &pipeline_create_info, NULL, &vk_context.transparent_pipeline);
            sx_assert_rel(result == VK_SUCCESS && "Could not create transparent pipeline!");
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[0].module, NULL);
            vkDestroyShaderModule(vk_context.device.logical_device, shader_stages[1].module, NULL);
        }
        /*}}}*/

    }
    /*}}}*/

	/* Semaphore creation {{{*/
	{
		VkSemaphoreCreateInfo semaphore_create_info;
		semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphore_create_info.pNext = NULL;
		semaphore_create_info.flags = 0;
		vk_context.image_available_semaphore = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
				sizeof(*(vk_context.image_available_semaphore)));
		vk_context.rendering_finished_semaphore = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
				sizeof(*(vk_context.rendering_finished_semaphore)));

		for(uint32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
			result = vkCreateSemaphore(vk_context.device.logical_device, &semaphore_create_info, NULL,
					&vk_context.image_available_semaphore[i]);
            sx_assert_rel(result == VK_SUCCESS && "Could not create semaphores!");
			result = vkCreateSemaphore(vk_context.device.logical_device, &semaphore_create_info, NULL,
					&vk_context.rendering_finished_semaphore[i]);
            sx_assert_rel(result == VK_SUCCESS && "Could not create semaphores!");
		}
	}
    /*}}}*/

	/* fences creation {{{*/
	{
		VkFenceCreateInfo fence_create_info;
		fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_create_info.pNext = NULL;
		fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vk_context.fences = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
				sizeof(*(vk_context.fences)));

		for(uint32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {

			result = vkCreateFence(vk_context.device.logical_device, &fence_create_info, NULL,
					&vk_context.fences[i]);
            sx_assert_rel(result == VK_SUCCESS && "Could not create fences!");
		}
	}
    /*}}}*/

	vk_context.framebuffer = sx_malloc(alloc, RENDERING_RESOURCES_SIZE *
			sizeof(*(vk_context.framebuffer)));
	for (uint32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
		vk_context.framebuffer[i] = VK_NULL_HANDLE;
	}

    return true;
}
/*}}}*/

/*update_composition_descriptors(RendererContext* context){{{*/
void update_composition_descriptors(RendererContext* context) {
    VkDescriptorImageInfo image_info[4];
    image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[0].imageView = context->position_image.image_view;
    image_info[0].sampler = VK_NULL_HANDLE;
    image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[1].imageView = context->normal_image.image_view;
    image_info[1].sampler = VK_NULL_HANDLE;
    image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[2].imageView = context->albedo_image.image_view;
    image_info[2].sampler = VK_NULL_HANDLE;
    image_info[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[3].imageView = context->metallic_roughness_image.image_view;
    image_info[3].sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet descriptor_writes[4];

    descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[0].pNext = NULL;
    descriptor_writes[0].dstSet = context->composition_descriptor_set;
    descriptor_writes[0].dstBinding = 0;
    descriptor_writes[0].dstArrayElement = 0;
    descriptor_writes[0].descriptorCount = 1;
    descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;;
    descriptor_writes[0].pImageInfo = &image_info[0];
    descriptor_writes[0].pBufferInfo = NULL;
    descriptor_writes[0].pTexelBufferView = NULL;
    
    descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[1].pNext = NULL;
    descriptor_writes[1].dstSet = context->composition_descriptor_set;
    descriptor_writes[1].dstBinding = 1;
    descriptor_writes[1].dstArrayElement = 0;
    descriptor_writes[1].descriptorCount = 1;
    descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;;
    descriptor_writes[1].pImageInfo = &image_info[1];
    descriptor_writes[1].pBufferInfo = NULL;
    descriptor_writes[1].pTexelBufferView = NULL;

    descriptor_writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[2].pNext = NULL;
    descriptor_writes[2].dstSet = context->composition_descriptor_set;
    descriptor_writes[2].dstBinding = 2;
    descriptor_writes[2].dstArrayElement = 0;
    descriptor_writes[2].descriptorCount = 1;
    descriptor_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;;
    descriptor_writes[2].pImageInfo = &image_info[2];
    descriptor_writes[2].pBufferInfo = NULL;
    descriptor_writes[2].pTexelBufferView = NULL;

    descriptor_writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[3].pNext = NULL;
    descriptor_writes[3].dstSet = context->composition_descriptor_set;
    descriptor_writes[3].dstBinding = 3;
    descriptor_writes[3].dstArrayElement = 0;
    descriptor_writes[3].descriptorCount = 1;
    descriptor_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;;
    descriptor_writes[3].pImageInfo = &image_info[3];
    descriptor_writes[3].pBufferInfo = NULL;
    descriptor_writes[3].pTexelBufferView = NULL;


    vkUpdateDescriptorSets(context->device.logical_device, 4, descriptor_writes, 0, NULL);
}
/*}}}*/

/* setup_framebuffer(RendererContext* context, bool update_attachments) {{{*/
VkResult setup_framebuffer(RendererContext* context, bool update_attachments) {
    VkResult result;
    for (int32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
        if (vk_context.framebuffer[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context->device.logical_device, vk_context.framebuffer[i], NULL);
        }
    }
    if (update_attachments) {
        create_attachments(context);
        update_composition_descriptors(context);
    }

    /*VkImageView attachments[6];*/
	/*VkFramebufferCreateInfo framebuffer_create_info;*/
	/*framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;*/
	/*framebuffer_create_info.pNext = NULL;*/
	/*framebuffer_create_info.flags = 0;*/
	/*framebuffer_create_info.renderPass = vk_context.render_pass;*/
	/*framebuffer_create_info.attachmentCount = 6;*/
	/*framebuffer_create_info.pAttachments = attachments;*/
	/*framebuffer_create_info.width = vk_context.width;*/
	/*framebuffer_create_info.height = vk_context.height;*/
	/*framebuffer_create_info.layers = 1;*/
    
    /*for (int32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {*/
        /*attachments[0] = vk_context.swapchain.image_views[i];*/
        /*attachments[1] = vk_context.position_image.image_view;*/
        /*attachments[2] = vk_context.normal_image.image_view;*/
        /*attachments[3] = vk_context.albedo_image.image_view;*/
        /*attachments[4] = vk_context.metallic_roughness_image.image_view;*/
        /*attachments[5] = vk_context.depth_image.image_view;*/
		/*result = vkCreateFramebuffer(context->device.logical_device, &framebuffer_create_info, NULL,*/
			/*&vk_context.framebuffer[i]);*/
        /*sx_assert_rel(result == VK_SUCCESS && "Could not create framebuffer!");*/
    /*}*/

    return result;
}
/*}}}*/

VkResult frame(Renderer* rd, uint32_t resource_index, uint32_t image_index) {
    VkResult result;
    RendererContext* context = rd->context;
    if (vk_context.framebuffer[resource_index] != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(context->device.logical_device, vk_context.framebuffer[resource_index], NULL);
    }

    VkImageView attachments[6];
	VkFramebufferCreateInfo framebuffer_create_info;
	framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_create_info.pNext = NULL;
	framebuffer_create_info.flags = 0;
	framebuffer_create_info.renderPass = vk_context.render_pass;
	framebuffer_create_info.attachmentCount = 6;
	framebuffer_create_info.pAttachments = attachments;
	framebuffer_create_info.width = vk_context.width;
	framebuffer_create_info.height = vk_context.height;
	framebuffer_create_info.layers = 1;

    attachments[0] = vk_context.swapchain.image_views[image_index];
    attachments[1] = vk_context.position_image.image_view;
    attachments[2] = vk_context.normal_image.image_view;
    attachments[3] = vk_context.albedo_image.image_view;
    attachments[4] = vk_context.metallic_roughness_image.image_view;
    attachments[5] = vk_context.depth_image.image_view;
    result = vkCreateFramebuffer(context->device.logical_device, &framebuffer_create_info, NULL,
        &vk_context.framebuffer[resource_index]);
    sx_assert_rel(result == VK_SUCCESS && "Could not create framebuffer!");

	VkCommandBufferBeginInfo command_buffer_begin_info;
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.pNext = NULL;
	command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	command_buffer_begin_info.pInheritanceInfo = NULL;

    VkClearValue clear_value[6];
    VkClearColorValue color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    VkClearDepthStencilValue depth = {1.0, 0.0};
    clear_value[0].color = color;
    clear_value[1].color = color;
    clear_value[2].color = color;
    clear_value[3].color = color;
    clear_value[4].color = color;
    clear_value[5].depthStencil = depth;

    VkRect2D render_area;
    render_area.offset.x = 0;
    render_area.offset.y = 0;
    render_area.extent.width = context->width;
    render_area.extent.height = context->height;

    VkRenderPassBeginInfo render_pass_begin_info;
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.pNext = NULL;
    render_pass_begin_info.renderPass = context->render_pass;
    render_pass_begin_info.renderArea = render_area;
    render_pass_begin_info.clearValueCount = 6;
    render_pass_begin_info.pClearValues = clear_value;

    render_pass_begin_info.framebuffer = context->framebuffer[resource_index];
    vkBeginCommandBuffer(context->graphic_queue_cmdbuffer[resource_index],
            &command_buffer_begin_info);
    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_subresource_range.baseMipLevel = 0;
    image_subresource_range.levelCount = 1;
    image_subresource_range.baseArrayLayer = 0;
    image_subresource_range.layerCount = 1;


    if (context->vk_graphic_queue != context->vk_present_queue) {
        VkImageMemoryBarrier barrier_from_present_to_draw;
        barrier_from_present_to_draw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier_from_present_to_draw.pNext = NULL;
        barrier_from_present_to_draw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier_from_present_to_draw.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier_from_present_to_draw.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier_from_present_to_draw.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier_from_present_to_draw.srcQueueFamilyIndex = context->present_queue_index;
        barrier_from_present_to_draw.dstQueueFamilyIndex = context->graphic_queue_index;
        barrier_from_present_to_draw.image = context->swapchain.images[image_index];
        barrier_from_present_to_draw.subresourceRange = image_subresource_range;
        vkCmdPipelineBarrier(context->graphic_queue_cmdbuffer[resource_index],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                &barrier_from_present_to_draw);
    }


    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = context->width;
    viewport.height = context->height;
    viewport.minDepth = 0.0;
    viewport.maxDepth = 1.0;

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = context->width;
    scissor.extent.height = context->height;

    vkCmdBeginRenderPass(context->graphic_queue_cmdbuffer[resource_index], &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(context->graphic_queue_cmdbuffer[resource_index], 0, 1, &viewport);
    vkCmdSetScissor(context->graphic_queue_cmdbuffer[resource_index], 0, 1, &scissor);
    VkDeviceSize offset = 0;

    VkDescriptorSet descriptor_sets[2];
    descriptor_sets[0] = context->global_descriptor_set;

    vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[resource_index], 
            VK_PIPELINE_BIND_POINT_GRAPHICS, context->skybox_pipeline_layout,
            0, 1, &context->global_descriptor_set, 0, NULL);
    // First Sub pass
    {

        // draw terrain
        vkCmdBindPipeline(context->graphic_queue_cmdbuffer[resource_index],
                VK_PIPELINE_BIND_POINT_GRAPHICS, context->terrain_pipeline);
        for (uint32_t idx = 0; idx < rd->terrain_system->data.size; idx++) {
            vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[resource_index],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, context->terrain_pipeline_layout,
                    1, 1, &rd->terrain_system->data.descriptor_set[idx], 0, NULL);
            vkCmdBindVertexBuffers(context->graphic_queue_cmdbuffer[resource_index], 0, 1,
                    &rd->terrain_system->data.vertex_buffer[idx].buffer, &offset);
            vkCmdBindIndexBuffer(context->graphic_queue_cmdbuffer[resource_index], 
                    rd->terrain_system->data.index_buffer[idx].buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(context->graphic_queue_cmdbuffer[resource_index], 
                    rd->terrain_system->data.index_buffer[idx].size/(sizeof(uint32_t)), 1, 0, 0, 0);
        }

        //draw objects
        vkCmdBindPipeline(context->graphic_queue_cmdbuffer[resource_index],
                VK_PIPELINE_BIND_POINT_GRAPHICS, context->objects_pipeline);
        for (uint32_t obj = 0; obj < rd->data.size; obj++) {
            vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[resource_index], 
                    VK_PIPELINE_BIND_POINT_GRAPHICS, context->objects_pipeline_layout,
                    1, 1, &rd->data.descriptor_set[obj], 0, NULL);
            vkCmdBindVertexBuffers(context->graphic_queue_cmdbuffer[resource_index], 0, 1,
                    &rd->data.vertex_buffer[obj].buffer, &offset);
            vkCmdBindIndexBuffer(context->graphic_queue_cmdbuffer[resource_index], 
                    rd->data.index_buffer[obj].buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(context->graphic_queue_cmdbuffer[resource_index], 
                    rd->data.index_buffer[obj].size/(sizeof(uint32_t)), 1, 0, 0, 0);
        }

    }
    // Second subpass
    {
        vkCmdNextSubpass(context->graphic_queue_cmdbuffer[resource_index], VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[resource_index], 
                VK_PIPELINE_BIND_POINT_GRAPHICS, context->composition_pipeline_layout,
                1, 1, &context->composition_descriptor_set, 0, NULL);
        vkCmdBindPipeline(context->graphic_queue_cmdbuffer[resource_index], 
                VK_PIPELINE_BIND_POINT_GRAPHICS, context->composition_pipeline);
        vkCmdDraw(context->graphic_queue_cmdbuffer[resource_index], 4, 1, 0, 0);
    }
    //Third subpass
    {
        vkCmdNextSubpass(context->graphic_queue_cmdbuffer[resource_index], VK_SUBPASS_CONTENTS_INLINE);
        // draw skybox
        vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[resource_index], 
                VK_PIPELINE_BIND_POINT_GRAPHICS, context->skybox_pipeline_layout,
                1, 1, &context->skybox_descriptor_set, 0, NULL);
        vkCmdBindPipeline(context->graphic_queue_cmdbuffer[resource_index],
                VK_PIPELINE_BIND_POINT_GRAPHICS, context->skybox_pipeline);
        vkCmdBindVertexBuffers(context->graphic_queue_cmdbuffer[resource_index], 0, 1,
                &context->vertex_buffer_sky.buffer, &offset);
        vkCmdBindIndexBuffer(context->graphic_queue_cmdbuffer[resource_index], 
                context->index_buffer_sky.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(context->graphic_queue_cmdbuffer[resource_index], 
                context->index_buffer_sky.size/(sizeof(uint32_t)), 1, 0, 0, 0);

        vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[resource_index], 
                VK_PIPELINE_BIND_POINT_GRAPHICS, context->transparent_pipeline_layout,
                1, 1, &context->transparent_descriptor_set, 0, NULL);
        vkCmdBindPipeline(context->graphic_queue_cmdbuffer[resource_index], 
                VK_PIPELINE_BIND_POINT_GRAPHICS, context->transparent_pipeline);
        vkCmdDraw(context->graphic_queue_cmdbuffer[resource_index], 4, 1, 0, 0);

        if (rd->gui) {
            gui_draw(rd->gui, resource_index);
        }
    }

    vkCmdEndRenderPass(context->graphic_queue_cmdbuffer[resource_index]);

    if (context->vk_graphic_queue != context->vk_present_queue) {
        VkImageMemoryBarrier barrier_from_draw_to_present;
        barrier_from_draw_to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier_from_draw_to_present.pNext = NULL;
        barrier_from_draw_to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier_from_draw_to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier_from_draw_to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier_from_draw_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier_from_draw_to_present.srcQueueFamilyIndex = context->graphic_queue_index;
        barrier_from_draw_to_present.dstQueueFamilyIndex = context->present_queue_index;
        barrier_from_draw_to_present.image = context->swapchain.images[image_index];
        barrier_from_draw_to_present.subresourceRange = image_subresource_range;
        vkCmdPipelineBarrier(context->graphic_queue_cmdbuffer[resource_index],
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
                &barrier_from_draw_to_present);
    }

     result = vkEndCommandBuffer(context->graphic_queue_cmdbuffer[resource_index]);
     sx_assert_rel(result == VK_SUCCESS && "Could not record command buffers!");
    return result;
}

/* build_commandbuffers(RendererContext* context) {{{*/
VkResult build_commandbuffers(RendererContext* context, Renderer* rd) {
    VkResult result;
	VkCommandBufferBeginInfo command_buffer_begin_info;
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.pNext = NULL;
	command_buffer_begin_info.flags = 0;
	command_buffer_begin_info.pInheritanceInfo = NULL;

    VkClearValue clear_value[6];
    VkClearColorValue color = { {0.0f, 0.0f, 0.0f, 0.0f} };
    VkClearDepthStencilValue depth = {1.0, 0.0};
    clear_value[0].color = color;
    clear_value[1].color = color;
    clear_value[2].color = color;
    clear_value[3].color = color;
    clear_value[4].color = color;
    clear_value[5].depthStencil = depth;

    VkRect2D render_area;
    render_area.offset.x = 0;
    render_area.offset.y = 0;
    render_area.extent.width = context->width;
    render_area.extent.height = context->height;

    VkRenderPassBeginInfo render_pass_begin_info;
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.pNext = NULL;
    render_pass_begin_info.renderPass = context->render_pass;
    render_pass_begin_info.renderArea = render_area;
    render_pass_begin_info.clearValueCount = 6;
    render_pass_begin_info.pClearValues = clear_value;

    for (int32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
        render_pass_begin_info.framebuffer = context->framebuffer[i];
        vkBeginCommandBuffer(context->graphic_queue_cmdbuffer[i],
                &command_buffer_begin_info);
        VkImageSubresourceRange image_subresource_range;
        image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_subresource_range.baseMipLevel = 0;
        image_subresource_range.levelCount = 1;
        image_subresource_range.baseArrayLayer = 0;
        image_subresource_range.layerCount = 1;


        if (context->vk_graphic_queue != context->vk_present_queue) {
            VkImageMemoryBarrier barrier_from_present_to_draw;
            barrier_from_present_to_draw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier_from_present_to_draw.pNext = NULL;
            barrier_from_present_to_draw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrier_from_present_to_draw.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier_from_present_to_draw.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier_from_present_to_draw.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier_from_present_to_draw.srcQueueFamilyIndex = context->present_queue_index;
            barrier_from_present_to_draw.dstQueueFamilyIndex = context->graphic_queue_index;
            barrier_from_present_to_draw.image = context->swapchain.images[i];
            barrier_from_present_to_draw.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier(context->graphic_queue_cmdbuffer[i],
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1,
                    &barrier_from_present_to_draw);
        }


        VkViewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = context->width;
        viewport.height = context->height;
        viewport.minDepth = 0.0;
        viewport.maxDepth = 1.0;

        VkRect2D scissor;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = context->width;
        scissor.extent.height = context->height;

        vkCmdBeginRenderPass(context->graphic_queue_cmdbuffer[i], &render_pass_begin_info,
                VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(context->graphic_queue_cmdbuffer[i], 0, 1, &viewport);
        vkCmdSetScissor(context->graphic_queue_cmdbuffer[i], 0, 1, &scissor);
        VkDeviceSize offset = 0;

        VkDescriptorSet descriptor_sets[2];
        descriptor_sets[0] = context->global_descriptor_set;
        descriptor_sets[0] = context->global_descriptor_set;

        vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context->skybox_pipeline_layout,
                0, 1, &context->global_descriptor_set, 0, NULL);
        // First Sub pass
        {

            // draw terrain
            vkCmdBindPipeline(context->graphic_queue_cmdbuffer[i],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, context->terrain_pipeline);
            for (uint32_t idx = 0; idx < rd->terrain_system->data.size; idx++) {
                vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context->terrain_pipeline_layout,
                        1, 1, &rd->terrain_system->data.descriptor_set[idx], 0, NULL);
                vkCmdBindVertexBuffers(context->graphic_queue_cmdbuffer[i], 0, 1,
                        &rd->terrain_system->data.vertex_buffer[idx].buffer, &offset);
                vkCmdBindIndexBuffer(context->graphic_queue_cmdbuffer[i], rd->terrain_system->data.index_buffer[idx].buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(context->graphic_queue_cmdbuffer[i], rd->terrain_system->data.index_buffer[idx].size/(sizeof(uint32_t)), 1, 0, 0, 0);
            }

            //draw objects
            vkCmdBindPipeline(context->graphic_queue_cmdbuffer[i],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, context->objects_pipeline);
            for (uint32_t obj = 0; obj < rd->data.size; obj++) {
                vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context->objects_pipeline_layout,
                        1, 1, &rd->data.descriptor_set[obj], 0, NULL);
                vkCmdBindVertexBuffers(context->graphic_queue_cmdbuffer[i], 0, 1,
                        &rd->data.vertex_buffer[obj].buffer, &offset);
                vkCmdBindIndexBuffer(context->graphic_queue_cmdbuffer[i], rd->data.index_buffer[obj].buffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(context->graphic_queue_cmdbuffer[i], rd->data.index_buffer[obj].size/(sizeof(uint32_t)), 1, 0, 0, 0);
            }

        }
        // Second subpass
        {
            vkCmdNextSubpass(context->graphic_queue_cmdbuffer[i], VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context->composition_pipeline_layout,
                    1, 1, &context->composition_descriptor_set, 0, NULL);
            vkCmdBindPipeline(context->graphic_queue_cmdbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context->composition_pipeline);
            vkCmdDraw(context->graphic_queue_cmdbuffer[i], 4, 1, 0, 0);
        }
        //Third subpass
        {
            vkCmdNextSubpass(context->graphic_queue_cmdbuffer[i], VK_SUBPASS_CONTENTS_INLINE);
            // draw skybox
            vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context->skybox_pipeline_layout,
                    1, 1, &context->skybox_descriptor_set, 0, NULL);
            vkCmdBindPipeline(context->graphic_queue_cmdbuffer[i],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, context->skybox_pipeline);
            vkCmdBindVertexBuffers(context->graphic_queue_cmdbuffer[i], 0, 1,
                    &context->vertex_buffer_sky.buffer, &offset);
            vkCmdBindIndexBuffer(context->graphic_queue_cmdbuffer[i], context->index_buffer_sky.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(context->graphic_queue_cmdbuffer[i], context->index_buffer_sky.size/(sizeof(uint32_t)), 1, 0, 0, 0);

            vkCmdBindDescriptorSets(context->graphic_queue_cmdbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context->transparent_pipeline_layout,
                    1, 1, &context->transparent_descriptor_set, 0, NULL);
            vkCmdBindPipeline(context->graphic_queue_cmdbuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context->transparent_pipeline);
            vkCmdDraw(context->graphic_queue_cmdbuffer[i], 4, 1, 0, 0);

            if (rd->gui) {
                gui_draw(rd->gui, i);
            }
        }

        vkCmdEndRenderPass(context->graphic_queue_cmdbuffer[i]);

        if (context->vk_graphic_queue != context->vk_present_queue) {
            VkImageMemoryBarrier barrier_from_draw_to_present;
            barrier_from_draw_to_present.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier_from_draw_to_present.pNext = NULL;
            barrier_from_draw_to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier_from_draw_to_present.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrier_from_draw_to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier_from_draw_to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier_from_draw_to_present.srcQueueFamilyIndex = context->graphic_queue_index;
            barrier_from_draw_to_present.dstQueueFamilyIndex = context->present_queue_index;
            barrier_from_draw_to_present.image = context->swapchain.images[i];
            barrier_from_draw_to_present.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier(context->graphic_queue_cmdbuffer[i],
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
                    &barrier_from_draw_to_present);
        }

         result = vkEndCommandBuffer(context->graphic_queue_cmdbuffer[i]);
         sx_assert_rel(result == VK_SUCCESS && "Could not record command buffers!");
    }
    return result;
}
/*}}}*/

/* vk_renderer_draw(RendererContext* context, Renderer* rd, float dt) {{{*/
bool vk_renderer_draw(RendererContext* context, Renderer* rd, float dt) {
    /*fly_camera_update(context->camera, dt);*/
	static uint32_t resource_index = 0;
	uint32_t next_resource_index = (resource_index + 1) % RENDERING_RESOURCES_SIZE;
	uint32_t image_index;
	VkResult result = vkWaitForFences(context->device.logical_device, 1,
			&context->fences[resource_index],
			VK_FALSE, 1000000000);
    sx_assert_rel(result == VK_SUCCESS && "Waiting for fence takes too long!");
	vkResetFences(context->device.logical_device, 1, &context->fences[resource_index]);

	result = vkAcquireNextImageKHR(context->device.logical_device, context->swapchain.swapchain,
			UINT64_MAX, context->image_available_semaphore[resource_index], VK_NULL_HANDLE, &image_index);
	switch(result) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
            printf("acquire subotimo\n");
			break;
            /*renderer_resize(rd, vk_context.width, vk_context.height);*/
		default:
			printf("Problem occurred during swap chain image acquisition!\n");
			return false;
	}
    frame(rd, resource_index, image_index);

	VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = NULL;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &context->image_available_semaphore[resource_index];
	submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &context->graphic_queue_cmdbuffer[resource_index];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &context->rendering_finished_semaphore[resource_index];
	result = vkQueueSubmit(context->vk_graphic_queue, 1, &submit_info,
			context->fences[resource_index]);
    sx_assert_rel(result == VK_SUCCESS && "Fail to submit command!");

	VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &context->rendering_finished_semaphore[resource_index];
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &context->swapchain.swapchain;
	present_info.pImageIndices = &image_index;
	present_info.pResults = NULL;

	result = vkQueuePresentKHR(context->vk_present_queue, &present_info);
	switch(result) {
		case VK_SUCCESS:
			break;
		case VK_SUBOPTIMAL_KHR:
		case VK_ERROR_OUT_OF_DATE_KHR:
            printf("present subotimo\n");
            break;
            /*renderer_resize(rd, vk_context.width, vk_context.height);*/
		default:
			printf("Problem occurred during image presentation!\n");
			return false;
	}
	resource_index = next_resource_index;

    /*vkDeviceWaitIdle(context->device.logical_device);*/
	return true;
}
/*}}}*/

/* create_swapchain( uint32_t width, uint32_t height) {{{*/
VkResult create_swapchain( uint32_t width, uint32_t height) {
    const sx_alloc* alloc = sx_alloc_malloc();
    VkSurfaceCapabilitiesKHR surface_capabilities;
    VkResult result;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &surface_capabilities);
    sx_assert_rel(result == VK_SUCCESS);

    uint32_t formats_count;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &formats_count, NULL);
    VkSurfaceFormatKHR *surface_formats = sx_malloc(alloc, formats_count *
            sizeof(*surface_formats));
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &formats_count, surface_formats);
    sx_assert_rel(result == VK_SUCCESS);

    uint32_t present_modes_count;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &present_modes_count, NULL);
    VkPresentModeKHR *present_modes = sx_malloc(alloc, present_modes_count *
            sizeof(*present_modes));
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(vk_context.device.physical_device,
            vk_context.device.presentation_surface, &present_modes_count, present_modes);
    sx_assert_rel(result == VK_SUCCESS);


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

    for (uint32_t i = 0; i < formats_count; i++) {
        if(surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            surface_format = surface_formats[i];
            break;
        }
    }
    sx_free(alloc, surface_formats);

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
    present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    sx_free(alloc, present_modes);

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
    sx_assert_rel(result == VK_SUCCESS);

    if(old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(vk_context.device.logical_device, old_swapchain, NULL);
    }

    vkGetSwapchainImagesKHR(vk_context.device.logical_device, vk_context.swapchain.swapchain, &image_count, NULL);
    vkGetSwapchainImagesKHR(vk_context.device.logical_device, vk_context.swapchain.swapchain, &image_count,
            vk_context.swapchain.images);
    swapchain_image_count = image_count;

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
        sx_assert_rel(result == VK_SUCCESS);
    }
    vk_context.swapchain.format = surface_format;
    /*RENDERING_RESOURCES_SIZE = image_count;*/
    return result;
}
/*}}}*/

/* get_memory_type(VkMemoryRequirements image_memory_requirements, VkMemoryPropertyFlags properties) {{{*/
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
/*}}}*/

/* create_image(Device* device, ImageBuffer* image_buffer, {{{*/
VkResult create_image(Device* device, ImageBuffer* image_buffer, 
        uint32_t width, uint32_t height, 
        VkImageUsageFlags usage, VkImageAspectFlags aspect, uint32_t mip_levels) {
    if (image_buffer->image != VK_NULL_HANDLE) {
        clear_image(device, image_buffer);
    }
    image_buffer->image = VK_NULL_HANDLE;
    image_buffer->image_view = VK_NULL_HANDLE;
    image_buffer->memory = VK_NULL_HANDLE;

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
/*}}}*/

/* clear_image(Device* device, ImageBuffer* image) {{{*/
void clear_image(Device* device, ImageBuffer* image) {
    vkDestroyImageView(device->logical_device, image->image_view, NULL);
    vkFreeMemory(device->logical_device, image->memory, NULL);
    vkDestroyImage(device->logical_device, image->image, NULL);
}
/*}}}*/

void clear_buffer(Device* device, Buffer* buffer) {
    vkFreeMemory(device->logical_device, buffer->memory, NULL);
    vkDestroyBuffer(device->logical_device, buffer->buffer, NULL);
}

void clear_texture(Device* device, Texture* texture) {
    vkDestroySampler(device->logical_device, texture->sampler, NULL);
    clear_image(device, &texture->image_buffer);
}

VkResult create_texture_from_data(Texture* texture, VkSamplerAddressMode sampler_address_mode, const sx_alloc* alloc, 
        const void* data, uint32_t width, uint32_t height) {
    clear_texture(&vk_context.device, texture);
    VkResult result;
    Buffer staging;
    uint32_t size = width * height * 4;
    result = create_buffer(&staging, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size);
    sx_assert_rel(result == VK_SUCCESS && "Could not create staging buffer!");

    void *staging_buffer_memory_pointer;
    result = vkMapMemory(vk_context.device.logical_device, staging.memory, 0,
            size, 0, &staging_buffer_memory_pointer);
    sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to staging buffer!");
    sx_memcpy(staging_buffer_memory_pointer, data, size);
    vkUnmapMemory(vk_context.device.logical_device, staging.memory);
    uint32_t offset = 0;
    VkExtent3D extent;
    extent.width = width;
    extent.height = height;
    extent.depth = 1;
    VkImageCreateInfo image_create_info;
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_create_info.extent = extent;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = vkCreateImage(vk_context.device.logical_device, &image_create_info, NULL,
            &texture->image_buffer.image);
    sx_assert_rel(result == VK_SUCCESS && "Could not create texture image!");

    VkMemoryRequirements image_memory_requirements;
    vkGetImageMemoryRequirements(vk_context.device.logical_device, texture->image_buffer.image, &image_memory_requirements);

    VkMemoryAllocateInfo memory_allocate_info;
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = image_memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = get_memory_type(image_memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
            &texture->image_buffer.memory);

    sx_assert_rel(result == VK_SUCCESS && "Could not allocate memory for texture!");

    result = vkBindImageMemory(vk_context.device.logical_device, texture->image_buffer.image, texture->image_buffer.memory, 0);
    sx_assert_rel(result == VK_SUCCESS && "Could not bind memory image!");

    VkImageSubresourceRange image_subresource_range;
    image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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

    VkBufferImageCopy buffer_image_copy_info = {};
    buffer_image_copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_image_copy_info.imageSubresource.mipLevel = 0;
    buffer_image_copy_info.imageSubresource.baseArrayLayer = 0;
    buffer_image_copy_info.imageSubresource.layerCount = 1;
    buffer_image_copy_info.imageExtent.width = width;
    buffer_image_copy_info.imageExtent.height = height;
    buffer_image_copy_info.imageExtent.depth = 1;
    buffer_image_copy_info.bufferOffset = offset;

    vkBeginCommandBuffer(cmdbuffer, &command_buffer_begin_info);

    {
        VkImageMemoryBarrier image_memory_barrier = {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = NULL;
        image_memory_barrier.srcAccessMask = 0;
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_memory_barrier.image = texture->image_buffer.image;
        image_memory_barrier.subresourceRange = image_subresource_range;
        vkCmdPipelineBarrier(cmdbuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
                NULL, 1, &image_memory_barrier);
    }

    vkCmdCopyBufferToImage(cmdbuffer, staging.buffer,
            texture->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
            &buffer_image_copy_info);
    {
        VkImageMemoryBarrier image_memory_barrier = {};
        image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        image_memory_barrier.pNext = NULL;
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_memory_barrier.image = texture->image_buffer.image;
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
    sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
    result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
    sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
    vkDestroyFence(vk_context.device.logical_device, fence, NULL);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = sampler_address_mode;
    samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
    samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
    samplerCreateInfo.mipLodBias = 0.0;
    samplerCreateInfo.maxAnisotropy = 1.0;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.minLod = 0.0;
    samplerCreateInfo.maxLod = 0.0;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    result = vkCreateSampler(vk_context.device.logical_device, &samplerCreateInfo, NULL, &texture->sampler);
    sx_assert_rel(result == VK_SUCCESS && "Could not create texture sampler!");

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewCreateInfo.subresourceRange = image_subresource_range;
    viewCreateInfo.subresourceRange.layerCount = 1;
    viewCreateInfo.image = texture->image_buffer.image;
    result = vkCreateImageView(vk_context.device.logical_device, &viewCreateInfo, NULL, &texture->image_buffer.image_view);
    sx_assert_rel(result == VK_SUCCESS && "Could not create texture image view!");

    // Clean up staging resources
    vkFreeMemory(vk_context.device.logical_device, staging.memory, NULL);
    vkDestroyBuffer(vk_context.device.logical_device, staging.buffer, NULL);

    return result;
}

/* create_texture(Device* device, Texture* texture, const sx_alloc* alloc, const char* filepath) {{{*/
VkResult create_texture(Texture* texture, VkSamplerAddressMode sampler_address_mode, const sx_alloc* alloc, const char* filepath) {
    texture->sampler = VK_NULL_HANDLE;
    texture->image_buffer.image = VK_NULL_HANDLE;
    texture->image_buffer.image_view = VK_NULL_HANDLE;
    texture->image_buffer.memory = VK_NULL_HANDLE;

    VkResult result;
    sx_mem_block* mem = sx_file_load_bin(alloc, filepath);
    ddsktx_texture_info tc = {0};
    ddsktx_error err;
    bool parse = ddsktx_parse(&tc, mem->data, mem->size, &err);
    /*printf("%s\n", filepath);*/
    sx_assert(parse == true && "Could not parse ktx file");
    if(parse) {
        Buffer staging;
        result = create_buffer(&staging, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tc.size_bytes);
        sx_assert_rel(result == VK_SUCCESS && "Could not create staging buffer!");

		void *staging_buffer_memory_pointer;
		VkResult result = vkMapMemory(vk_context.device.logical_device, staging.memory, 0,
				tc.size_bytes, 0, &staging_buffer_memory_pointer);
        sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to staging buffer!");
        uint32_t offset = 0;
        int num_faces = 1;
        if (tc.flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) {
            num_faces = DDSKTX_CUBE_FACE_COUNT;
        }
        ddsktx_sub_data sub_data;
        int num_regions = tc.num_mips * tc.num_layers * num_faces;
        VkBufferImageCopy* buffer_copy_regions = sx_malloc(alloc, num_regions * sizeof(*buffer_copy_regions));
        int idx = 0;
        int layer_face = 0;
        for (int mip = 0; mip < tc.num_mips; mip++) {
            layer_face = 0;
            for (int layer = 0; layer < tc.num_layers; layer++) {
                for (int face = 0; face < num_faces; face++) {
                    ddsktx_get_sub(&tc, &sub_data, mem->data, mem->size, layer, face, mip);
                    sx_memcpy(staging_buffer_memory_pointer + offset, sub_data.buff, sub_data.size_bytes);
                    VkBufferImageCopy buffer_image_copy_info = {};
                    buffer_image_copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    buffer_image_copy_info.imageSubresource.mipLevel = mip;
                    buffer_image_copy_info.imageSubresource.baseArrayLayer = layer_face;
                    buffer_image_copy_info.imageSubresource.layerCount = 1;
                    buffer_image_copy_info.imageExtent.width = sub_data.width;
                    buffer_image_copy_info.imageExtent.height = sub_data.height;
                    buffer_image_copy_info.imageExtent.depth = 1;
                    buffer_image_copy_info.bufferOffset = offset;
                    buffer_copy_regions[idx] = buffer_image_copy_info;

                    offset += sub_data.size_bytes;
                    idx++;
                    layer_face++;
                }
            }
        }
		vkUnmapMemory(vk_context.device.logical_device, staging.memory);
        VkExtent3D extent;
		extent.width = tc.width;
		extent.height = tc.height;
		extent.depth = 1;
		VkImageCreateInfo image_create_info;
		image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_create_info.pNext = NULL;
        image_create_info.flags = 0;
        if (tc.flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) {
            image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }
		image_create_info.imageType = VK_IMAGE_TYPE_2D;
		image_create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
		image_create_info.extent = extent;
		image_create_info.mipLevels = tc.num_mips;
		image_create_info.arrayLayers = layer_face;
		image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
		image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_create_info.queueFamilyIndexCount = 0;
		image_create_info.pQueueFamilyIndices = NULL;
		image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		result = vkCreateImage(vk_context.device.logical_device, &image_create_info, NULL,
				&texture->image_buffer.image);
        sx_assert_rel(result == VK_SUCCESS && "Could not create texture image!");

		VkMemoryRequirements image_memory_requirements;
		vkGetImageMemoryRequirements(vk_context.device.logical_device, texture->image_buffer.image, &image_memory_requirements);

        VkMemoryAllocateInfo memory_allocate_info;
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = NULL;
        memory_allocate_info.allocationSize = image_memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = get_memory_type(image_memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
                &texture->image_buffer.memory);

        sx_assert_rel(result == VK_SUCCESS && "Could not allocate memory for texture!");

		result = vkBindImageMemory(vk_context.device.logical_device, texture->image_buffer.image, texture->image_buffer.memory, 0);
        sx_assert_rel(result == VK_SUCCESS && "Could not bind memory image!");

		VkImageSubresourceRange image_subresource_range;
        image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_subresource_range.baseMipLevel = 0;
		image_subresource_range.levelCount = tc.num_mips;
		image_subresource_range.baseArrayLayer = 0;
		image_subresource_range.layerCount = layer_face;
        /*printf("num_layers: %d\n\n", layer_face);*/
        /*printf("num_mips: %d\n\n", tc.num_mips);*/
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
            image_memory_barrier.image = texture->image_buffer.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier(cmdbuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
                    NULL, 1, &image_memory_barrier);
        }

		vkCmdCopyBufferToImage(cmdbuffer, staging.buffer,
				texture->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions,
				buffer_copy_regions);
        {
            VkImageMemoryBarrier image_memory_barrier = {};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.pNext = NULL;
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_memory_barrier.image = texture->image_buffer.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier( cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1,
                    &image_memory_barrier);
        }
        sx_free(alloc, buffer_copy_regions);

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
        sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
        result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
        sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
        vkDestroyFence(vk_context.device.logical_device, fence, NULL);

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = sampler_address_mode;
        samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
        samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
        samplerCreateInfo.mipLodBias = 0.0;
        samplerCreateInfo.maxAnisotropy = 4.0;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = (float)tc.num_mips;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        result = vkCreateSampler(vk_context.device.logical_device, &samplerCreateInfo, NULL, &texture->sampler);
        sx_assert_rel(result == VK_SUCCESS && "Could not create texture sampler!");

        // Create image view
        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        if (tc.num_layers > 1) {
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        } else if (tc.flags & DDSKTX_TEXTURE_FLAG_CUBEMAP) {
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        }
        viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.subresourceRange = image_subresource_range;
        viewCreateInfo.subresourceRange.layerCount = layer_face;
        viewCreateInfo.image = texture->image_buffer.image;
        result = vkCreateImageView(vk_context.device.logical_device, &viewCreateInfo, NULL, &texture->image_buffer.image_view);
        sx_assert_rel(result == VK_SUCCESS && "Could not create texture image view!");

        // Clean up staging resources
        vkFreeMemory(vk_context.device.logical_device, staging.memory, NULL);
        vkDestroyBuffer(vk_context.device.logical_device, staging.buffer, NULL);


    }
    sx_mem_destroy_block(mem);

    return result;
}
/*}}}*/

/* create_cubemap(Device* device, Texture* cubemap){{{*/
/*
VkResult create_cubemap(Device* device, Texture* cubemap) {
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
        result = create_buffer(&staging, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, total_size);
        sx_assert_rel(result == VK_SUCCESS && "Could not create staging buffer!");

    }

    {
		void *staging_buffer_memory_pointer;
		VkResult result = vkMapMemory(device->logical_device, staging.memory, 0,
				total_size, 0, &staging_buffer_memory_pointer);
        sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to staging buffer!");
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

		result = vkCreateImage(device->logical_device, &image_create_info, NULL,
				&cubemap->image_buffer.image);
        sx_assert_rel(result == VK_SUCCESS && "Could not create cubemap image!");

		VkMemoryRequirements image_memory_requirements;
		vkGetImageMemoryRequirements(device->logical_device, cubemap->image_buffer.image, &image_memory_requirements);

        VkMemoryAllocateInfo memory_allocate_info;
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = NULL;
        memory_allocate_info.allocationSize = image_memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = get_memory_type(image_memory_requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
                &cubemap->image_buffer.memory);

        sx_assert_rel(result == VK_SUCCESS && "Could not allocate memory for cubemap!");

		result = vkBindImageMemory(device->logical_device, cubemap->image_buffer.image, cubemap->image_buffer.memory, 0);
        sx_assert_rel(result == VK_SUCCESS && "Could not bind memory image!");

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
            image_memory_barrier.image = cubemap->image_buffer.image;
            image_memory_barrier.subresourceRange = image_subresource_range;
            vkCmdPipelineBarrier(cmdbuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0,
                    NULL, 1, &image_memory_barrier);
        }

		vkCmdCopyBufferToImage(cmdbuffer, staging.buffer,
				cubemap->image_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6,
				buffer_copy_regions);
        {
            VkImageMemoryBarrier image_memory_barrier = {};
            image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            image_memory_barrier.pNext = NULL;
            image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_memory_barrier.image = cubemap->image_buffer.image;
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
        vkCreateFence(device->logical_device, &fence_create_info, NULL, &fence);

        result = vkQueueSubmit(vk_context.vk_graphic_queue, 1, &submit_info, fence);
        sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
        result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
        sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
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
        result = vkCreateSampler(device->logical_device, &samplerCreateInfo, NULL, &cubemap->sampler);
        sx_assert_rel(result == VK_SUCCESS && "Could not create cubemap sampler!");

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
        viewCreateInfo.image = cubemap->image_buffer.image;
        result = vkCreateImageView(device->logical_device, &viewCreateInfo, NULL, &cubemap->image_buffer.image_view);
        sx_assert_rel(result == VK_SUCCESS && "Could not create cubemap image view!");

        // Clean up staging resources
        vkFreeMemory(device->logical_device, staging.memory, NULL);
        vkDestroyBuffer(device->logical_device, staging.buffer, NULL);

    }
    return result;
}
*/
/*}}}*/


/* create_attachments(RendererContext* context){{{*/
VkResult create_attachments(RendererContext* context) {
    uint32_t width = context->width;
    uint32_t height = context->height;
    VkResult result;
    /* GBuffer creation {{{ */
    {
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        context->position_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        context->normal_image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        context->albedo_image.format = VK_FORMAT_R8G8B8A8_UNORM;
        context->metallic_roughness_image.format = VK_FORMAT_R8G8B8A8_UNORM;
        result = create_image(&vk_context.device, &vk_context.position_image,
                width, height, usage, aspect, 1);
        sx_assert_rel(result == VK_SUCCESS && "Could not create position image");
        result = create_image(&vk_context.device, &vk_context.normal_image,
                width, height, usage, aspect, 1);
        sx_assert_rel(result == VK_SUCCESS && "Could not create normal image");
        result = create_image(&vk_context.device, &vk_context.albedo_image,
                width, height, usage, aspect, 1);
        sx_assert_rel(result == VK_SUCCESS && "Could not create albedo image");
        result = create_image(&vk_context.device, &vk_context.metallic_roughness_image,
                width, height, usage, aspect, 1);
        sx_assert_rel(result == VK_SUCCESS && "Could not create metallic roughness image");
    }
    /* }}} */

    /* Depth stencil creation {{{ */
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
            vkGetPhysicalDeviceFormatProperties(vk_context.device.physical_device,
                    depth_formats[i], &format_props);
            if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                vk_context.depth_image.format = depth_formats[i];
            }
        }
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (vk_context.depth_image.format >= VK_FORMAT_D16_UNORM_S8_UINT) {
            aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        result = create_image(&vk_context.device, &vk_context.depth_image,
                width, height, usage, aspect, 1);
        sx_assert_rel(result == VK_SUCCESS && "Could not create depth image");

    }
    /* }}} */

    return result;
}
/*}}}*/

/* VkResult create_command_buffers() {{{*/
VkResult create_command_buffers() {
    VkCommandBufferAllocateInfo cmd_buffer_allocate_info;
    cmd_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_allocate_info.pNext = NULL;
    cmd_buffer_allocate_info.commandPool = vk_context.graphic_queue_cmdpool;
    cmd_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_allocate_info.commandBufferCount = RENDERING_RESOURCES_SIZE;

    vk_context.graphic_queue_cmdbuffer = sx_malloc(sx_alloc_malloc(), RENDERING_RESOURCES_SIZE *
            sizeof(*(vk_context.graphic_queue_cmdbuffer)));

    VkResult result;
    result = vkAllocateCommandBuffers(vk_context.device.logical_device, &cmd_buffer_allocate_info,
            vk_context.graphic_queue_cmdbuffer);
    sx_assert_rel(result == VK_SUCCESS && "Could no create command buffer!");

    cmd_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_allocate_info.pNext = NULL;
    cmd_buffer_allocate_info.commandPool = vk_context.graphic_queue_cmdpool;
    cmd_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_allocate_info.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(vk_context.device.logical_device, &cmd_buffer_allocate_info,
            &vk_context.copy_cmdbuffer);
    sx_assert_rel(result == VK_SUCCESS && "Could no create copy command buffer!");
    return result;
}
/*}}}*/

/* void destroy_command_buffers() {{{*/
void destroy_command_buffers() {
    vkFreeCommandBuffers(vk_context.device.logical_device, vk_context.graphic_queue_cmdpool, 
            RENDERING_RESOURCES_SIZE, vk_context.graphic_queue_cmdbuffer);
    sx_free(sx_alloc_malloc(), vk_context.graphic_queue_cmdbuffer);
    vkFreeCommandBuffers(vk_context.device.logical_device, vk_context.graphic_queue_cmdpool, 
            1, &vk_context.copy_cmdbuffer);
}
/*}}}*/

/* create_buffer(Device* device, Buffer* buffer, VkBufferUsageFlags usage...) {{{*/
VkResult create_buffer(Buffer* buffer, VkBufferUsageFlags usage, 
                       VkMemoryPropertyFlags memory_properties_flags, VkDeviceSize size) {
        buffer->buffer = VK_NULL_HANDLE;
        buffer->memory = VK_NULL_HANDLE;
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

		VkResult result = vkCreateBuffer(vk_context.device.logical_device, &buffer_create_info, NULL,
				&buffer->buffer);
        sx_assert_rel(result == VK_SUCCESS && "Could not create buffer");

		VkMemoryRequirements buffer_memory_requirements;
		vkGetBufferMemoryRequirements(vk_context.device.logical_device, buffer->buffer,
				&buffer_memory_requirements);
        VkMemoryAllocateInfo memory_allocate_info;
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = NULL;
        memory_allocate_info.allocationSize = buffer_memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = get_memory_type(buffer_memory_requirements, memory_properties_flags);
        result = vkAllocateMemory(vk_context.device.logical_device, &memory_allocate_info, NULL,
                &buffer->memory);

		result = vkBindBufferMemory(vk_context.device.logical_device, buffer->buffer,
				buffer->memory, 0);
        sx_assert_rel(result == VK_SUCCESS && "Could not bind memory for buffer");
        return result;
}
/*}}}*/

/*VkResult copy_buffer(Device* device, Buffer* dst_buffer, void* data, VkDeviceSize size) {{{*/
VkResult copy_buffer(Buffer* dst_buffer, void* data, VkDeviceSize size) {
    void *buffer_memory_pointer;
    VkResult result;
    result = vkMapMemory(vk_context.device.logical_device, dst_buffer->memory, 0, size, 0, &buffer_memory_pointer);
    sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to buffer!");
    sx_memcpy(buffer_memory_pointer, data, size);
    
    vkUnmapMemory(vk_context.device.logical_device, dst_buffer->memory);

    return result;
}
/*}}}*/

/* VkResult copy_buffer_staged(Buffer* dst_buffer, void* data, VkDeviceSize size) {{{*/
VkResult copy_buffer_staged(Buffer* dst_buffer, void* data, VkDeviceSize size) {
    Buffer staging;
    VkResult result = create_buffer(&staging, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size);
    sx_assert_rel(result == VK_SUCCESS && "Could not create staging buffer!");

    void *staging_buffer_memory_pointer;
    result = vkMapMemory(vk_context.device.logical_device, staging.memory, 0,
            staging.size, 0, &staging_buffer_memory_pointer);
    sx_assert_rel(result == VK_SUCCESS && "Could not map memory and upload data to buffer!");
    sx_memcpy(staging_buffer_memory_pointer, data, size);
    
    vkUnmapMemory(vk_context.device.logical_device, staging.memory);
    VkCommandBuffer cmdbuffer = vk_context.copy_cmdbuffer;

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
    vkCmdCopyBuffer(cmdbuffer, staging.buffer,
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
    sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
    result = vkWaitForFences(vk_context.device.logical_device, 1, &fence, VK_TRUE, 1000000000);
    sx_assert_rel(result == VK_SUCCESS && "Could not submit command buffer!");
    vkDestroyFence(vk_context.device.logical_device, fence, NULL);

    // Clean up staging resources
    vkFreeMemory(vk_context.device.logical_device, staging.memory, NULL);
    vkDestroyBuffer(vk_context.device.logical_device, staging.buffer, NULL);
    return result;
}
/*}}}*/

/* load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage) {{{*/
VkPipelineShaderStageCreateInfo load_shader(VkDevice logical_device, const char* filnename, VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo shaderstage_create_info;
    shaderstage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderstage_create_info.pNext = NULL;
    shaderstage_create_info.flags = 0;
    shaderstage_create_info.stage = stage;
    shaderstage_create_info.pName = "main";
    shaderstage_create_info.pSpecializationInfo = NULL;
    sx_mem_block* mem = sx_file_load_bin(sx_alloc_malloc(), filnename);
    /*FILE* shader = fopen(filnename, "rb");*/
    /*fseek(shader, 0, SEEK_END);*/
    /*uint64_t size = ftell(shader);*/
    /*uint8_t* data = (uint8_t*)aligned_alloc(sizeof(uint32_t), size * sizeof(uint8_t));*/
    /*rewind(shader);*/
    /*fread(data, sizeof(uint8_t), size, shader);*/
    VkShaderModuleCreateInfo module_create_info;
    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_create_info.pNext = NULL;
    module_create_info.flags = 0;
    module_create_info.codeSize = mem->size;
    module_create_info.pCode = (uint32_t*)mem->data;
    VkResult result = vkCreateShaderModule(logical_device, &module_create_info, NULL,
            &shaderstage_create_info.module);
    if(result != VK_SUCCESS) {
        printf("Could not create vert shader module!\n");
    }
    sx_mem_destroy_block(mem);
    /*free(data);*/
    return shaderstage_create_info;
}
/*}}}*/

/*Renderer* create_renderer(const sx_alloc* alloc, EntityManager* em) {{{*/
Renderer* create_renderer(const sx_alloc* alloc, EntityManager* em) {
    Renderer* renderer = sx_malloc(alloc, sizeof(*renderer));
    renderer->alloc = alloc;
    renderer->entity_manager = em;
    renderer->table = sx_hashtbl_create(alloc, 1000);
    renderer->data.buffer = NULL;
    renderer->data.size = 0;
    renderer->data.capacity = 0;
    renderer->materials.buffer = NULL;
    renderer->materials.size = 0;
    renderer->materials.capacity = 0;
    renderer->context = &vk_context;

    return renderer;
}
/*}}}*/

/*void pbrmaterial_allocate(Renderer* rd, uint32_t size) {{{*/
void pbrmaterial_allocate(Renderer* rd, uint32_t size) {
    sx_assert_rel(size > rd->materials.size);

    PbrMaterials new_data;
    uint32_t bytes = size * (  sizeof(Texture) + alignof(Texture) 
                             + sizeof(Texture) + alignof(Texture)
                             + sizeof(Texture) + alignof(Texture)
                             + sizeof(Texture) + alignof(Texture)
                             + sizeof(Texture) + alignof(Texture)
                             + sizeof(Buffer) + alignof(Buffer) );

    new_data.buffer = sx_malloc(rd->alloc, bytes);
    new_data.size = size;
    new_data.capacity = size;

    new_data.base_color_texture = (Texture*)sx_align_ptr((void*)(new_data.buffer), 0, alignof(Texture));
    new_data.metallic_roughness_texture = (Texture*)sx_align_ptr((void*)(new_data.base_color_texture + size), 0, alignof(Texture));
    new_data.normal_texture = (Texture*)sx_align_ptr((void*)(new_data.metallic_roughness_texture + size), 0, alignof(Texture));
    new_data.occlusion_texture = (Texture*)sx_align_ptr((void*)(new_data.normal_texture + size), 0, alignof(Texture));
    new_data.emissive_texture = (Texture*)sx_align_ptr((void*)(new_data.occlusion_texture + size), 0, alignof(Texture));
    new_data.object_ubos = (Buffer*)sx_align_ptr((void*)(new_data.emissive_texture + size), 0, alignof(Buffer));

    if (rd->materials.buffer) {
        sx_memcpy(new_data.base_color_texture, rd->materials.base_color_texture, rd->data.size * sizeof(Texture));
        sx_memcpy(new_data.metallic_roughness_texture, rd->materials.metallic_roughness_texture, rd->data.size * sizeof(Texture));
        sx_memcpy(new_data.normal_texture, rd->materials.normal_texture, rd->data.size * sizeof(Texture));
        sx_memcpy(new_data.occlusion_texture, rd->materials.occlusion_texture, rd->data.size * sizeof(Texture));
        sx_memcpy(new_data.emissive_texture, rd->materials.emissive_texture, rd->data.size * sizeof(Texture));
        sx_memcpy(new_data.object_ubos, rd->materials.object_ubos, rd->data.size * sizeof(Buffer));

        sx_free(rd->alloc, rd->materials.buffer);
    }

    rd->materials = new_data;
}
/*}}}*/

/*void init_pbr_materials(Renderer* rd, MaterialsData* material_data, const char* path) {{{*/
void init_pbr_materials(Renderer* rd, MaterialsData* material_data, const char* path) {
    pbrmaterial_allocate(rd, material_data->num_materials);
    char total_path[200];

    for (int i = 0; i < material_data->num_materials; i++) {
        sx_strcpy(total_path, 200, path);
        sx_strcat(total_path, 200, material_data->base_color_texture_names[i].names);
        create_texture(&rd->materials.base_color_texture[i], 
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, rd->alloc, total_path);

        sx_strcpy(total_path, 200, path);
        sx_strcat(total_path, 200, material_data->metallic_roughness_texture_names[i].names);
        create_texture(&rd->materials.metallic_roughness_texture[i], 
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, rd->alloc, total_path);

        sx_strcpy(total_path, 200, path);
        sx_strcat(total_path, 200, material_data->normal_texture_names[i].names);
        create_texture(&rd->materials.normal_texture[i], 
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, rd->alloc, total_path);

        sx_strcpy(total_path, 200, path);
        sx_strcat(total_path, 200, material_data->occlusion_texture_names[i].names);
        create_texture(&rd->materials.occlusion_texture[i], 
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, rd->alloc, total_path);

        sx_strcpy(total_path, 200, path);
        sx_strcat(total_path, 200, material_data->emissive_texture_names[i].names);
        create_texture(&rd->materials.emissive_texture[i], 
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, rd->alloc, total_path);
        ObjectsUBO ubo;
        ubo.base_color = material_data->base_color_factors[i];
        ubo.metallic_roughness.x = material_data->metallic_factors[i];
        ubo.metallic_roughness.y = material_data->roughness_factors[i];

        VkResult result;
        result = create_buffer(&rd->materials.object_ubos[i],
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(ObjectsUBO));
        sx_assert_rel(result == VK_SUCCESS && "Could not create object uniform buffer!");
        copy_buffer(&rd->materials.object_ubos[i],  &ubo, sizeof(ubo));
    }
}
/*}}}*/

/*void renderer_allocate(Renderer* rd, uint32_t size) {{{*/
void renderer_allocate(Renderer* rd, uint32_t size) {
    sx_assert_rel(size > rd->data.size);

    MeshInstanceData new_data;
    uint32_t bytes = size * (  sizeof(Entity) + alignof(Entity)
                             + sizeof(Buffer) + alignof(Buffer)
                             + sizeof(Buffer) + alignof(Buffer)
                             + sizeof(VkDescriptorSet) + alignof(VkDescriptorSet) );
    new_data.buffer = sx_malloc(rd->alloc, bytes);
    new_data.size = rd->data.size;
    new_data.capacity = size;

    new_data.entity = (Entity*)sx_align_ptr((void*)(new_data.buffer), 0, alignof(Entity));
    new_data.vertex_buffer = (Buffer*)sx_align_ptr((void*)(new_data.entity + size), 0, alignof(Buffer));
    new_data.index_buffer = (Buffer*)sx_align_ptr((void*)(new_data.vertex_buffer + size), 0, alignof(Buffer));
    new_data.descriptor_set = (VkDescriptorSet*)sx_align_ptr((void*)(new_data.index_buffer + size), 0, alignof(VkDescriptorSet));
    /*new_data.vertex_offsets = (uint32_t*)sx_align_ptr((void*)(new_data.index_buffer + size), 0, alignof(uint32_t));*/
    /*new_data.index_offsets = (uint32_t*)sx_align_ptr((void*)(new_data.vertex_offsets + size), 0, alignof(uint32_t));*/

    if (rd->data.buffer) {
        sx_memcpy(new_data.entity, rd->data.entity, rd->data.size * sizeof(Entity));
        sx_memcpy(new_data.vertex_buffer, rd->data.vertex_buffer, rd->data.size * sizeof(Buffer));
        sx_memcpy(new_data.index_buffer, rd->data.index_buffer, rd->data.size * sizeof(Buffer));
        sx_memcpy(new_data.descriptor_set, rd->data.descriptor_set, rd->data.size * sizeof(VkDescriptorSet));
        /*sx_memcpy(new_data.index_offsets, rd->data.index_offsets, rd->data.size * sizeof(uint32_t));*/

        sx_free(rd->alloc, rd->data.buffer);
    }
    rd->data = new_data;
}
/*}}}*/

void renderer_grow(Renderer* rd) {
    renderer_allocate(rd, rd->data.capacity * 2 + 1);
}

/*MeshInstance create_mesh_instance(Renderer* rd, Entity e, MeshData* mesh_data) {{{*/
MeshInstance create_mesh_instance(Renderer* rd, Entity e, MeshData* mesh_data) {
    if (rd->data.capacity == rd->data.size) {
        renderer_grow(rd);
    }

    uint32_t last = rd->data.size;
    rd->data.size++;

    rd->data.entity[last] = e;
    VkResult result;
    // Vertex buffer creation;
    result = create_buffer(&rd->data.vertex_buffer[last],
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh_data->vertex_size * sizeof(float));

    sx_assert_rel(result == VK_SUCCESS && "Could not create vertex buffer!");
    result = copy_buffer_staged(&rd->data.vertex_buffer[last], mesh_data->vertex_data, mesh_data->vertex_size * sizeof(float));
    sx_assert_rel(result == VK_SUCCESS && "Could not copy vertex buffer!");
    // Index buffer creation;
    result = create_buffer(&rd->data.index_buffer[last],
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh_data->index_size * sizeof(uint32_t));

    sx_assert_rel(result == VK_SUCCESS && "Could not create index buffer!");
    result = copy_buffer_staged(&rd->data.index_buffer[last], mesh_data->index_data, mesh_data->index_size * sizeof(uint32_t));
    sx_assert_rel(result == VK_SUCCESS && "Could not copy index buffer!");
    
    // Objects descriptor set
    {
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.pNext = NULL;
        descriptor_set_allocate_info.descriptorPool = vk_context.objects_descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &vk_context.objects_descriptor_layout;

        result = vkAllocateDescriptorSets(vk_context.device.logical_device, &descriptor_set_allocate_info,
                &rd->data.descriptor_set[last]);
        sx_assert_rel("Could not create objects descriptor set!");
    }
    /*Objects Descriptor update{{{*/
    {
        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer = rd->materials.object_ubos[last].buffer;
        buffer_info.offset = 0;
        buffer_info.range = rd->materials.object_ubos[last].size;

        VkDescriptorImageInfo image_info[5];
        image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[0].imageView = rd->materials.base_color_texture[last].image_buffer.image_view;
        image_info[0].sampler = rd->materials.base_color_texture[last].sampler;
        image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[1].imageView = rd->materials.normal_texture[last].image_buffer.image_view;
        image_info[1].sampler = rd->materials.normal_texture[last].sampler;
        image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[2].imageView = rd->materials.metallic_roughness_texture[last].image_buffer.image_view;
        image_info[2].sampler = rd->materials.metallic_roughness_texture[last].sampler;
        image_info[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[3].imageView = rd->materials.occlusion_texture[last].image_buffer.image_view;
        image_info[3].sampler = rd->materials.occlusion_texture[last].sampler;
        image_info[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info[4].imageView = rd->materials.emissive_texture[last].image_buffer.image_view;
        image_info[4].sampler = rd->materials.emissive_texture[last].sampler;

        VkWriteDescriptorSet descriptor_writes[6];
        descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[0].pNext = NULL;
        descriptor_writes[0].dstSet = rd->data.descriptor_set[last];
        descriptor_writes[0].dstBinding = 0;
        descriptor_writes[0].dstArrayElement = 0;
        descriptor_writes[0].descriptorCount = 1;
        descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;;
        descriptor_writes[0].pImageInfo = NULL;
        descriptor_writes[0].pBufferInfo = &buffer_info;
        descriptor_writes[0].pTexelBufferView = NULL;

        descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[1].pNext = NULL;
        descriptor_writes[1].dstSet = rd->data.descriptor_set[last];
        descriptor_writes[1].dstBinding = 1;
        descriptor_writes[1].dstArrayElement = 0;
        descriptor_writes[1].descriptorCount = 1;
        descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[1].pImageInfo = &image_info[0];
        descriptor_writes[1].pBufferInfo = NULL;
        descriptor_writes[1].pTexelBufferView = NULL;

        descriptor_writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[2].pNext = NULL;
        descriptor_writes[2].dstSet = rd->data.descriptor_set[last];
        descriptor_writes[2].dstBinding = 2;
        descriptor_writes[2].dstArrayElement = 0;
        descriptor_writes[2].descriptorCount = 1;
        descriptor_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[2].pImageInfo = &image_info[1];
        descriptor_writes[2].pBufferInfo = NULL;
        descriptor_writes[2].pTexelBufferView = NULL;

        descriptor_writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[3].pNext = NULL;
        descriptor_writes[3].dstSet = rd->data.descriptor_set[last];
        descriptor_writes[3].dstBinding = 3;
        descriptor_writes[3].dstArrayElement = 0;
        descriptor_writes[3].descriptorCount = 1;
        descriptor_writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[3].pImageInfo = &image_info[2];
        descriptor_writes[3].pBufferInfo = NULL;
        descriptor_writes[3].pTexelBufferView = NULL;

        descriptor_writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[4].pNext = NULL;
        descriptor_writes[4].dstSet = rd->data.descriptor_set[last];
        descriptor_writes[4].dstBinding = 4;
        descriptor_writes[4].dstArrayElement = 0;
        descriptor_writes[4].descriptorCount = 1;
        descriptor_writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[4].pImageInfo = &image_info[3];
        descriptor_writes[4].pBufferInfo = NULL;
        descriptor_writes[4].pTexelBufferView = NULL;

        descriptor_writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_writes[5].pNext = NULL;
        descriptor_writes[5].dstSet = rd->data.descriptor_set[last];
        descriptor_writes[5].dstBinding = 5;
        descriptor_writes[5].dstArrayElement = 0;
        descriptor_writes[5].descriptorCount = 1;
        descriptor_writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_writes[5].pImageInfo = &image_info[4];
        descriptor_writes[5].pBufferInfo = NULL;
        descriptor_writes[5].pTexelBufferView = NULL;



        vkUpdateDescriptorSets(vk_context.device.logical_device, 6, descriptor_writes, 0, NULL);
    }
    /*}}}*/

    sx_hashtbl_add(rd->table, sx_hash_u32(e.handle), last);

    MeshInstance inst;
    inst.i = last;
    return inst;
}
/*}}}*/

void renderer_prepare(Renderer* rd) {
    /*setup_framebuffer(&vk_context, false);*/
    /*build_commandbuffers(&vk_context, rd);*/
    update_uniform_buffer();
}

void update_transform(Renderer* rd, Entity e, sx_mat4* mat) {
    int i = sx_hashtbl_find_get(rd->table, sx_hash_u32(e.handle), -1);
    ObjectsUBO ubo;
    sx_memcpy(&ubo.model, mat, sizeof(sx_mat4));
    copy_buffer(&rd->materials.object_ubos[i],  &ubo, sizeof(sx_mat4));
}

void renderer_render(Renderer* rd) {
    update_uniform_buffer();
    vk_renderer_draw(&vk_context, rd, 0);
    /*vkDeviceWaitIdle(vk_context.device.logical_device);*/
    /*build_commandbuffers(&vk_context, rd);*/
}

void renderer_resize(Renderer* rd, uint32_t width, uint32_t height) {
    vk_context.width = width;
    vk_context.height = height;
    /*vkDeviceWaitIdle(vk_context.device.logical_device);*/
    create_swapchain(width, height);
    /*setup_framebuffer(&vk_context, true);*/
    create_attachments(rd->context);
    update_composition_descriptors(rd->context);
    /*destroy_command_buffers();*/
    /*create_command_buffers();*/
    /*build_commandbuffers(&vk_context, rd);*/
    /*vkDeviceWaitIdle(vk_context.device.logical_device);*/
}

void destroy_mesh_instance(Renderer* rd, Entity e, MeshInstance i) {
}

bool renderer_lookup(Renderer* rd, Entity e) {
    return sx_hashtbl_find(rd->table, sx_hash_u32(e.handle)) != -1;
}

void destroy_renderer(Renderer* renderer) {
    vkDeviceWaitIdle(vk_context.device.logical_device);
    for (int32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
        if (vk_context.framebuffer[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(vk_context.device.logical_device, vk_context.framebuffer[i], NULL);
        }
    }
    sx_free(sx_alloc_malloc(), vk_context.framebuffer);
    vkDestroyDescriptorPool(vk_context.device.logical_device, vk_context.global_descriptor_pool, NULL);
    vkDestroyDescriptorPool(vk_context.device.logical_device, vk_context.skybox_descriptor_pool, NULL);
    vkDestroyDescriptorPool(vk_context.device.logical_device, vk_context.terrain_descriptor_pool, NULL);
    vkDestroyDescriptorPool(vk_context.device.logical_device, vk_context.objects_descriptor_pool, NULL);
    vkDestroyDescriptorPool(vk_context.device.logical_device, vk_context.composition_descriptor_pool, NULL);
    vkDestroyDescriptorPool(vk_context.device.logical_device, vk_context.transparent_descriptor_pool, NULL);
    vkDestroyDescriptorPool(vk_context.device.logical_device, vk_context.nk_gui_descriptor_pool, NULL);

    vkDestroyDescriptorSetLayout(vk_context.device.logical_device, vk_context.global_descriptor_layout, NULL);
    vkDestroyDescriptorSetLayout(vk_context.device.logical_device, vk_context.skybox_descriptor_layout, NULL);
    vkDestroyDescriptorSetLayout(vk_context.device.logical_device, vk_context.terrain_descriptor_layout, NULL);
    vkDestroyDescriptorSetLayout(vk_context.device.logical_device, vk_context.objects_descriptor_layout, NULL);
    vkDestroyDescriptorSetLayout(vk_context.device.logical_device, vk_context.composition_descriptor_layout, NULL);
    vkDestroyDescriptorSetLayout(vk_context.device.logical_device, vk_context.transparent_descriptor_layout, NULL);
    vkDestroyDescriptorSetLayout(vk_context.device.logical_device, vk_context.nk_gui_descriptor_layout, NULL);

    destroy_command_buffers();
    vkDestroyPipeline(vk_context.device.logical_device, vk_context.terrain_pipeline, NULL);
    vkDestroyPipeline(vk_context.device.logical_device, vk_context.skybox_pipeline, NULL);
    vkDestroyPipeline(vk_context.device.logical_device, vk_context.objects_pipeline, NULL);
    vkDestroyPipeline(vk_context.device.logical_device, vk_context.composition_pipeline, NULL);
    vkDestroyPipeline(vk_context.device.logical_device, vk_context.transparent_pipeline, NULL);
    vkDestroyPipeline(vk_context.device.logical_device, vk_context.nk_gui_pipeline, NULL);
    vkDestroyPipelineLayout(vk_context.device.logical_device, vk_context.terrain_pipeline_layout, NULL);
    vkDestroyPipelineLayout(vk_context.device.logical_device, vk_context.skybox_pipeline_layout, NULL);
    vkDestroyPipelineLayout(vk_context.device.logical_device, vk_context.objects_pipeline_layout, NULL);
    vkDestroyPipelineLayout(vk_context.device.logical_device, vk_context.composition_pipeline_layout, NULL);
    vkDestroyPipelineLayout(vk_context.device.logical_device, vk_context.transparent_pipeline_layout, NULL);
    vkDestroyPipelineLayout(vk_context.device.logical_device, vk_context.nk_gui_pipeline_layout, NULL);

    vkDestroyRenderPass(vk_context.device.logical_device, vk_context.render_pass, NULL);

    clear_image(&vk_context.device, &vk_context.depth_image);
    clear_image(&vk_context.device, &vk_context.position_image);
    clear_image(&vk_context.device, &vk_context.normal_image);
    clear_image(&vk_context.device, &vk_context.albedo_image);
    clear_image(&vk_context.device, &vk_context.metallic_roughness_image);

    clear_buffer(&vk_context.device, &vk_context.vertex_buffer_sky);
    clear_buffer(&vk_context.device, &vk_context.index_buffer_sky);
    clear_buffer(&vk_context.device, &vk_context.global_uniform_buffer);
    clear_buffer(&vk_context.device, &vk_context.staging_buffer);

    clear_texture(&vk_context.device, &vk_context.cubemap_texture);
    clear_texture(&vk_context.device, &vk_context.lut_brdf);
    clear_texture(&vk_context.device, &vk_context.irradiance_cube);
    clear_texture(&vk_context.device, &vk_context.prefiltered_cube);

    for (int32_t i = 0; i < renderer->data.size; i++) {
        clear_buffer(&vk_context.device, &renderer->data.vertex_buffer[i]);
        clear_buffer(&vk_context.device, &renderer->data.index_buffer[i]);
    }
    for (int32_t i = 0; i < renderer->materials.size; i++) {
        clear_texture(&vk_context.device, &renderer->materials.base_color_texture[i]);
        clear_texture(&vk_context.device, &renderer->materials.metallic_roughness_texture[i]);
        clear_texture(&vk_context.device, &renderer->materials.normal_texture[i]);
        clear_texture(&vk_context.device, &renderer->materials.occlusion_texture[i]);
        clear_texture(&vk_context.device, &renderer->materials.emissive_texture[i]);
        clear_buffer(&vk_context.device, &renderer->materials.object_ubos[i]);
    }

    for (int32_t i = 0; i < swapchain_image_count; i++) {
        vkDestroyImageView(vk_context.device.logical_device, vk_context.swapchain.image_views[i], NULL);
    }

    vkDestroySwapchainKHR(vk_context.device.logical_device, vk_context.swapchain.swapchain, NULL);
    for (int32_t i = 0; i < RENDERING_RESOURCES_SIZE; i++) {
        vkDestroySemaphore(vk_context.device.logical_device, vk_context.image_available_semaphore[i], NULL);
        vkDestroySemaphore(vk_context.device.logical_device, vk_context.rendering_finished_semaphore[i], NULL);
        vkDestroyFence(vk_context.device.logical_device, vk_context.fences[i], NULL);
    }
    sx_free(sx_alloc_malloc(), vk_context.image_available_semaphore);
    sx_free(sx_alloc_malloc(), vk_context.rendering_finished_semaphore);
    sx_free(sx_alloc_malloc(), vk_context.fences);

    vkDestroyCommandPool(vk_context.device.logical_device, vk_context.graphic_queue_cmdpool, NULL);

    vkDestroyDevice(vk_context.device.logical_device, NULL);

    vkDestroySurfaceKHR(vk_context.vk_instance, vk_context.device.presentation_surface, NULL);
    vkDestroyDebugUtilsMessengerEXT(vk_context.vk_instance, vk_context.vk_debugmessenger, NULL);
    vkDestroyInstance(vk_context.vk_instance, NULL);
    sx_free(sx_alloc_malloc(), vk_context.present_queue_cmdbuffer);
    sx_free(renderer->alloc, renderer->data.buffer);
    sx_free(renderer->alloc, renderer->materials.buffer);
    sx_hashtbl_destroy(renderer->table, renderer->alloc);
    sx_free(renderer->alloc, renderer);
    /*sx_os_dlclose(vulkan_library);*/
}
