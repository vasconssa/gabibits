#version 450

layout (location = 0) in vec3 v_uvw;

layout (location = 0) out vec4 out_fragcolor;

void main() 
{
	out_fragcolor = vec4(1.0, 0.0, 0.0, 1.0);
}

