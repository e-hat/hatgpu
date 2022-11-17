#version 450

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform sampler2D albedoTexture;
layout (set = 1, binding = 1) uniform sampler2D metallicRoughnessTexture;

void main() {
    outColor = vec4(texture(metallicRoughnessTexture, inTexCoord).rrr * 20, 1.0);
}
