#version 450

out gl_PerVertex {
    vec4 gl_Position;
};

layout(set=0, binding=1) uniform u_UniformBuffer {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in uvec4 color;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUv;

void main() {
    gl_Position = ubo.projection * vec4(position, 0.0, 1.0);
    gl_Position.y = -gl_Position.y;
    fragColor = vec4(color[0]/255.0, color[1]/255.0, color[2]/255.0, color[3]/255.0 - 0.2);
    fragUv = uv;
}
