#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

layout ( push_constant ) uniform constants
{
    vec4 data;
    mat4 renderMatrix;
} PushConstants;

void main() {
    gl_Position = PushConstants.renderMatrix * vec4(inPosition, 1.0);
    fragColor = inColor;
}
