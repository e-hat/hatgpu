#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec3 outCameraPos;

layout (set = 0, binding = 0) uniform CameraBuffer{
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec3 position;
} cameraData;

layout ( push_constant ) uniform constants
{
    mat4 renderMatrix;
} PushConstants;

void main() {
    outWorldPos = vec3(PushConstants.renderMatrix * vec4(inPosition, 1.0));
    mat4 transformMatrix = cameraData.viewproj * PushConstants.renderMatrix;
    gl_Position = transformMatrix * vec4(inPosition, 1.0);
    outTexCoord = inTexCoord;
    outNormal = inNormal;
    outCameraPos = cameraData.position;
}
