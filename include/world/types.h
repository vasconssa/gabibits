#pragma once
#include <sx/handle.h>


typedef struct Entity {
    sx_handle_t handle;
} Entity;

#define INSTANCE_ID(name)            \
	typedef struct name              \
	{                                \
		uint32_t i;                  \
	} name;                          \

INSTANCE_ID(TransformInstance);
INSTANCE_ID(MeshInstance);
INSTANCE_ID(TerrainInstance);
INSTANCE_ID(GuiInstance);
