#version 450

layout(set=0, binding=0) uniform u_global_ubo {
    mat4 projection;
    mat4 view;
    vec4 light_position[4];
    vec4 camera_position;
    vec4 exposure_gama;
} global_ubo;

layout(set = 1, binding = 0) uniform UBO
{
	vec4 frustum_planes[6];
	vec2 viewport_dimensions;
	float displacement_factor;
	float tessellation_factor;
	float tessellated_edge_size;
    float pad;
} ubo;

layout (set = 1, binding = 1) uniform sampler2D displacement_map; 

layout(quads, equal_spacing, ccw) in;

layout (location = 0) in vec3 tsc_normal[];
layout (location = 1) in vec2 tsc_uv[];
 
layout (location = 0) out vec3 tes_normal;
layout (location = 1) out vec2 tes_uv;
layout (location = 2) out vec3 tes_viewvec;
layout (location = 3) out vec3 tes_lightvec;
layout (location = 4) out vec3 tes_eyepos;
layout (location = 5) out vec3 tes_worldpos;


void main()
{
	// Interpolate UV coordinates
	vec2 uv1 = mix(tsc_uv[0], tsc_uv[1], gl_TessCoord.x);
	vec2 uv2 = mix(tsc_uv[3], tsc_uv[2], gl_TessCoord.x);
	tes_uv = mix(uv1, uv2, gl_TessCoord.y);

	vec3 n1 = mix(tsc_normal[0], tsc_normal[1], gl_TessCoord.x);
	vec3 n2 = mix(tsc_normal[3], tsc_normal[2], gl_TessCoord.x);
	tes_normal = mix(n1, n2, gl_TessCoord.y);

	// Interpolate positions
	vec4 pos1 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);
	vec4 pos2 = mix(gl_in[3].gl_Position, gl_in[2].gl_Position, gl_TessCoord.x);
	vec4 pos = mix(pos1, pos2, gl_TessCoord.y);
	// Displace
	pos.y += textureLod(displacement_map, tes_uv, 0.0).r * ubo.displacement_factor;
	// Perspective projection
	gl_Position = global_ubo.projection * global_ubo.view * pos;

	// Calculate vectors for lighting based on tessellated position
	tes_viewvec = -pos.xyz;
	tes_lightvec = normalize(global_ubo.light_position[0].xyz + tes_viewvec);
	tes_worldpos = pos.xyz;
	tes_eyepos = vec3(global_ubo.view * pos);
}
