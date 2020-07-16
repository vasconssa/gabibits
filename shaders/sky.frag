#version 450

layout (location = 0) in vec3 v_worldpos;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec3 v_tangent;
layout (location = 3) in vec3 v_uvw;

layout (set = 1, binding = 0) uniform samplerCube sampler_cubemap;

layout (location = 0) out vec4 out_color;
//layout (location = 1) out vec4 out_position;
//layout (location = 2) out vec4 out_normal;
//layout (location = 3) out vec4 out_albedo;
//layout (location = 4) out vec4 out_metallicroughness;

//layout (constant_id = 0) const float NEAR_PLANE = 0.1f;
//layout (constant_id = 1) const float FAR_PLANE = 256.0f;

//float linear_depth(float depth)
//{
	//float z = depth * 2.0f - 1.0f; 
	//return (2.0f * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));	
//}

void main() 
{
    //out_position = vec4(v_worldpos, 1.0);
	//out_position.a = linear_depth(gl_FragCoord.z);

    //vec3 N = normalize(v_normal);
    //out_normal = vec4(N, 1.0);
	//out_albedo = texture(sampler_cubemap, v_uvw);
	out_color = texture(sampler_cubemap, v_uvw);
	//out_metallicroughness = vec4(0.5);

	//out_color = out_albedo;
}

