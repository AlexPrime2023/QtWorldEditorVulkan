#version 450

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

layout(push_constant) uniform PushModel {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 vFragTexCoord;
layout(location = 1) out vec3 vWorldNormal;

void main()
{
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);

    mat3 normalMat = transpose(inverse(mat3(pc.model)));
    vWorldNormal = normalize(normalMat * inNormal);

    vFragTexCoord = inTexCoord;

    gl_Position = cam.proj * cam.view * worldPos;
}
