#version 460

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

struct ObjectData
{
    mat4 modelTransform;
};

layout (std140, set = 0, binding = 1) readonly buffer ObjectBuffer {
    ObjectData objects[];
} objectBuffer;

void main() {
    mat4 modelTransform = objectBuffer.objects[gl_BaseInstance].modelTransform;
    mat4 transformMatrix = cameraData.viewproj * modelTransform;

    gl_Position = transformMatrix * vec4(inPosition, 1.0);

    outWorldPos = vec3(modelTransform * vec4(inPosition, 1.0));
    outTexCoord = inTexCoord;
    outNormal = inNormal;
    outCameraPos = cameraData.position;
}
