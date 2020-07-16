#version 450

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord;


layout(set=0, binding=0) uniform u_global_ubo {
    mat4 projection;
    mat4 view;
    vec4 light_position[4];
    vec4 camera_position;
    vec4 exposure_gama;
} global_ubo;

layout (location = 0) out vec3 v_worldpos;
layout (location = 1) out vec3 v_normal;
layout (location = 2) out vec3 v_tangent;
layout (location = 3) out vec3 v_uvw;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
    mat4 view = global_ubo.view;
    view[0][3] = 0.0;
    view[1][3] = 0.0;
    view[2][3] = 0.0;
    view[3][3] = 0.0;
    vec3 pos = a_position;
    //pos.y += global_ubo.camera_position.y;
	gl_Position = (global_ubo.projection * view * vec4(pos, 0.0)).xyzz;
    v_worldpos = a_position;
    v_normal = normalize(a_normal);
	v_uvw = a_position;
    //v_uvw.y *= -1.0;
}
