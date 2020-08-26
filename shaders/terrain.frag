#version 450

layout (set = 1, binding = 1) uniform sampler2D sampler_height; 
layout (set = 1, binding = 2) uniform sampler2D sampler_terrain;
layout (set = 1, binding = 3) uniform sampler2D sampler_normal;

layout (location = 0) in vec3 tes_normal;
layout (location = 1) in vec2 tes_uv;
layout (location = 2) in vec3 tes_viewvec;
layout (location = 3) in vec3 tes_lightvec;
layout (location = 4) in vec3 tes_eyepos;
layout (location = 5) in vec3 tes_worldpos;

layout (location = 0) out vec4 out_color;
layout (location = 1) out vec4 out_position;
layout (location = 2) out vec4 out_normal;
layout (location = 3) out vec4 out_albedo;
layout (location = 4) out vec4 out_metallicroughness;

layout (constant_id = 0) const float NEAR_PLANE = 0.1f;
layout (constant_id = 1) const float FAR_PLANE = 2500.0f;

float linear_depth(float depth)
{
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));	
}
vec4 hash4( vec2 p ) { return fract(sin(vec4( 1.0+dot(p,vec2(37.0,17.0)), 
                                              2.0+dot(p,vec2(11.0,47.0)),
                                              3.0+dot(p,vec2(41.0,29.0)),
                                              4.0+dot(p,vec2(23.0,31.0))))*103.0); }

vec4 textureNoTile( sampler2D samp, in vec2 uv ) {
    vec2 p = floor( uv );
    vec2 f = fract( uv );
	
    // derivatives (for correct mipmapping)
    vec2 ddx = dFdx( uv );
    vec2 ddy = dFdy( uv );
    
    // voronoi contribution
    vec4 va = vec4( 0.0 );
    float wt = 0.0;
    for( int j=-1; j<=1; j++ )
    for( int i=-1; i<=1; i++ )
    {
        vec2 g = vec2( float(i), float(j) );
        vec4 o = hash4( p + g );
        vec2 r = g - f + o.xy;
        float d = dot(r,r);
        float w = exp(-5.0*d );
        vec4 c = textureGrad( samp, uv + o.zw, ddx, ddy );
        va += w*c;
        wt += w;
    }
	
    // normalization
    return va/wt;
}

//vec3 sample_terrain_layer()
//{
    //// Define some layer ranges for sampling depending on terrain height
    //vec2 layers[6];
    //layers[0] = vec2(-10.0, 10.0);
    //layers[1] = vec2(5.0, 45.0);
    //layers[2] = vec2(45.0, 80.0);
    //layers[3] = vec2(75.0, 100.0);
    //layers[4] = vec2(95.0, 140.0);
    //layers[5] = vec2(140.0, 190.0);

    //vec3 color = vec3(0.0);
    
    //// Get height from displacement map
    //float height = textureLod(sampler_height, tes_uv, 0.0).r * 255.0;
    
    //for (int i = 0; i < 6; i++)
    //{
        //float range = layers[i].y - layers[i].x;
        //float weight = (range - abs(height - layers[i].y)) / range;
        //weight = max(0.0, weight);
        //float mul = 32.0;
        //vec3 uvw = vec3(mul * tes_uv.x, -mul * tes_uv.y, i);
        //color += weight * textureNoTile(sampler_terrain, uvw).rgb;
    //}

    //return color;
//}

float fog(float density)
{
	const float LOG2 = -1.442695;
	float dist = gl_FragCoord.z / gl_FragCoord.w;
	float d = density * dist;
	return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

// See http://www.thetenthplanet.de/archives/1180
vec3 perturb_normal()
{
    vec2 uv = tes_uv * 8.0;
    uv.y = 1.0 * uv.y;
	vec3 tangent_normal = texture(sampler_normal,  uv).xyz * 2.0 - 1.0;

	vec3 q1 = dFdx(tes_worldpos);
	vec3 q2 = dFdy(tes_worldpos);
	vec2 st1 = dFdx(tes_uv);
	vec2 st2 = dFdy(tes_uv);

	vec3 N = normalize(tes_normal);
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangent_normal);
}

void main()
{
    vec3 N = perturb_normal();
    //vec3 N = normalize(cross(dFdy(tes_worldpos), dFdx(tes_worldpos)));
	vec3 L = normalize(tes_lightvec);
    L = normalize(vec3(0.0, 1.0, 1.0));
	vec3 ambient = vec3(1.0);
	vec3 diffuse = max(dot(N, L), 0.0) * vec3(1.0);

    //vec4 color = vec4(ambient * diffuse * sample_terrain_layer(), 1.0);
    float mul = 1.0;
    vec3 uvw = vec3(mul * tes_uv.x,  mul * tes_uv.y, 0.0);
    vec4 color = vec4(texture(sampler_terrain, mul * tes_uv).xyz, 1.0);
    //vec4 color = vec4(ambient * diffuse * vec3(1.0, 1.0, 0.0), 1.0);

	const vec4 fogColor = vec4(0.47, 0.5, 0.67, 0.0);
    out_position = vec4(tes_worldpos, 1.0);
	out_position.a = linear_depth(gl_FragCoord.z);
	out_albedo  = mix(color, fogColor, fog(0.00));	
    //out_albedo = vec4(texture(sampler_terrain, tes_uv).xyz, 1.0);
    out_normal = vec4(N, 1.0);
	out_metallicroughness = vec4(0.5);
	out_color = vec4(0.0);;
}
