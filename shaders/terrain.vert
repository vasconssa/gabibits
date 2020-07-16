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

layout (location = 0) out vec3 v_normal;
layout (location = 1) out vec2 v_uv;

void main(void)
{
    vec3 pos = a_position;
    //pos.y += global_ubo.camera_position.y;
    gl_Position = vec4(pos, 1.0);
	v_uv = a_uv;
	v_normal = a_normal;
}
