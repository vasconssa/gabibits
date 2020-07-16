#version 450

//layout (location = 0) in vec3 a_position;
//layout (location = 1) in vec3 a_normal;
//layout (location = 2) in vec2 a_texcoord;


layout(set=0, binding=0) uniform u_global_ubo {
    mat4 projection;
    mat4 view;
    vec4 light_position[4];
    vec4 camera_position;
    vec4 exposure_gama;
} global_ubo;

layout (location = 0) out vec3 v_uvw;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	v_uvw = vec3((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2, 1.0);
	//gl_Position = global_ubo.projection * global_ubo.view * vec4(a_position.xyz, 1.0);
	gl_Position = vec4(v_uvw.xy * 2.0f - 1.0f, 1.0f, 1.0f);
}
