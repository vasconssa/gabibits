#version 450

layout (location = 0) in vec3 v_worldpos;
layout (location = 1) in vec3 v_normal;
layout (location = 2) in vec3 v_tangent;
layout (location = 3) in vec2 v_uv;

layout (set = 1, binding = 1) uniform sampler2D albedo;
layout (set = 1, binding = 2) uniform sampler2D normal;
layout (set = 1, binding = 3) uniform sampler2D ambient_occlusion;
layout (set = 1, binding = 4) uniform sampler2D metallic_roughness;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_position;
layout (location = 2) out vec4 out_normal;
layout (location = 3) out vec4 out_albedo;
layout (location = 4) out vec4 out_metallicroughness;

layout (constant_id = 0) const float NEAR_PLANE = 0.1f;
layout (constant_id = 1) const float FAR_PLANE = 256.0f;

float linear_depth(float depth)
{
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));	
}

// See http://www.thetenthplanet.de/archives/1180
vec3 perturb_normal()
{
    vec2 uv = v_uv;
    uv.y = 1.0 * uv.y;
	vec3 tangent_normal = texture(normal,  uv).xyz * 2.0 - 1.0;

	vec3 q1 = dFdx(v_worldpos);
	vec3 q2 = dFdy(v_worldpos);
	vec2 st1 = dFdx(v_uv);
	vec2 st2 = dFdy(v_uv);

	vec3 N = normalize(v_normal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangent_normal);
}
void main() 
{
	out_position = vec4(v_worldpos, 1.0);

	vec3 N = perturb_normal();
	//N.y = -N.y;
	out_normal = vec4(N, 1.0);

    out_albedo = texture(albedo, v_uv);

	// Store linearized depth in alpha component
	out_position.a = linear_depth(gl_FragCoord.z);

	// Write color attachments to avoid undefined behaviour (validation error)
	out_color = vec4(0.0);
	out_metallicroughness = vec4(0.5);
}
