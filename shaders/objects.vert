#version 450

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_uv;

layout(set=0, binding=0) uniform u_global_ubo {
    mat4 projection;
    mat4 view;
    vec4 light_position[4];
    vec4 camera_position;
    vec4 exposure_gama;
} global_ubo;

layout(set = 1, binding=0) uniform u_object_ubo {
    mat4 model;
    vec4 base_color_factor;
    vec4 metallic_roughness;
} object_ubo;

layout (location = 0) out vec3 v_worldpos;
layout (location = 1) out vec3 v_normal;
layout (location = 2) out vec3 v_tangent;
layout (location = 3) out vec2 v_uv;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = global_ubo.projection * global_ubo.view * object_ubo.model * vec4(a_position, 1.0);

    v_worldpos = (object_ubo.model * vec4(a_position, 1.0)).xyz;

    v_normal = normalize(a_normal);
    v_uv = a_uv;
}
