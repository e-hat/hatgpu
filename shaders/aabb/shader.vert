#version 460

layout(location = 0) in vec4 inPosition;

layout(push_constant) uniform constants {
  mat4 viewproj;
} PushConstants;

void main() {
    gl_Position = PushConstants.viewproj * inPosition;
}
