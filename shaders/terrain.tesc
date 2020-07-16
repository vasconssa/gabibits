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
    mat4 model;
	vec4 frustum_planes[6];
	vec2 viewport_dimensions;
	float displacement_factor;
	float tessellation_factor;
	float tessellated_edge_size;
    float pad;
} ubo;

layout(set = 1, binding = 1) uniform sampler2D sampler_height;

layout (vertices = 4) out;
 
layout (location = 0) in vec3 v_normal[];
layout (location = 1) in vec2 v_uv[];
 
layout (location = 0) out vec3 tsc_normal[4];
layout (location = 1) out vec2 tsc_uv[4];
 
// Calculate the tessellation factor based on screen space
// dimensions of the edge
float screen_space_tess_factor(vec4 p0, vec4 p1)
{
	// Calculate edge mid point
	vec4 mid_point = 0.5 * (p0 + p1);
	// Sphere radius as distance between the control points
	float radius = distance(p0, p1) / 2.0;

	// View space
	vec4 v0 = global_ubo.view  * mid_point;

	// Project into clip space
	vec4 clip0 = (global_ubo.projection * (v0 - vec4(radius, vec3(0.0))));
	vec4 clip1 = (global_ubo.projection * (v0 + vec4(radius, vec3(0.0))));

	// Get normalized device coordinates
	clip0 /= clip0.w;
	clip1 /= clip1.w;

	// Convert to viewport coordinates
	clip0.xy *= ubo.viewport_dimensions;
	clip1.xy *= ubo.viewport_dimensions;
	
	// Return the tessellation factor based on the screen size 
	// given by the distance of the two edge control points in screen space
	// and a reference (min.) tessellation size for the edge set by the application
    return clamp(distance(clip0, clip1) / ubo.tessellated_edge_size * ubo.tessellation_factor, 1.0, 64.0);
    return 4.0;
}

// Checks the current's patch visibility against the frustum using a sphere check
// Sphere radius is given by the patch size
bool frustum_check()
{
    return true;

	//// Fixed radius (increase if patch size is increased in example)
	//const float radius = 8.0f;
	//vec4 pos = gl_in[gl_InvocationID].gl_Position;
	//pos.y += textureLod(sampler_height, v_uv[0], 0.0).r * ubo.displacement_factor;

	//// Check sphere against frustum planes
	//for (int i = 0; i < 6; i++) {
		//if (dot(pos, ubo.frustum_planes[i]) + radius < 0.0)
		//{
			//return false;
		//}
	//}
	//return true;
}

void main()
{
	if (gl_InvocationID == 0)
	{
		if (!frustum_check())
		{
			gl_TessLevelInner[0] = 0.0;
			gl_TessLevelInner[1] = 0.0;
			gl_TessLevelOuter[0] = 0.0;
			gl_TessLevelOuter[1] = 0.0;
			gl_TessLevelOuter[2] = 0.0;
			gl_TessLevelOuter[3] = 0.0;
		}
		else
		{
			if (ubo.tessellation_factor > 0.0)
			{
				gl_TessLevelOuter[0] = screen_space_tess_factor(gl_in[0].gl_Position, gl_in[1].gl_Position);
				gl_TessLevelOuter[1] = screen_space_tess_factor(gl_in[3].gl_Position, gl_in[0].gl_Position);
				gl_TessLevelOuter[2] = screen_space_tess_factor(gl_in[2].gl_Position, gl_in[3].gl_Position);
				gl_TessLevelOuter[3] = screen_space_tess_factor(gl_in[1].gl_Position, gl_in[2].gl_Position);
				gl_TessLevelInner[0] = mix(gl_TessLevelOuter[0], gl_TessLevelOuter[3], 0.5);
				gl_TessLevelInner[1] = mix(gl_TessLevelOuter[2], gl_TessLevelOuter[1], 0.5);
			}
			else
			{
				// Tessellation factor can be set to zero by example
				// to demonstrate a simple passthrough
				gl_TessLevelInner[0] = 1.0;
				gl_TessLevelInner[1] = 1.0;
				gl_TessLevelOuter[0] = 1.0;
				gl_TessLevelOuter[1] = 1.0;
				gl_TessLevelOuter[2] = 1.0;
				gl_TessLevelOuter[3] = 1.0;
			}
		}

	}

	gl_out[gl_InvocationID].gl_Position =  gl_in[gl_InvocationID].gl_Position;
	tsc_normal[gl_InvocationID] = v_normal[gl_InvocationID];
	tsc_uv[gl_InvocationID] = v_uv[gl_InvocationID];
} 
