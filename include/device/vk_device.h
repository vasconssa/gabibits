#pragma once

#include <stdint.h>

#define VK_NO_STDINT_H
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#include "device/input_manager.h"
#include "camera.h"

typedef struct {
    InputManager* input_manager;
    FpsCamera camera;
    sx_rect viewport;
} GameDevice;

int device_run(void* vk_context, void* data);

GameDevice* device();
