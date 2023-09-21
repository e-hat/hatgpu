#version 460

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform constants {
  mat4 viewproj;
} PushConstants;

void main() {
    gl_Position = PushConstants.viewproj * vec4(inPosition, 1.0);
}
