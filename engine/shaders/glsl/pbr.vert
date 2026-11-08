#version 450 core

// Zypher standard mesh vertex shader (Vulkan / MoltenVK / OpenGL via SPIR-V).
// Compiled to SPIR-V with glslang; see shaders/shader_manifest.json.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform SceneUniforms {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 lightDirection;
    vec4 lightColor;
} scene;

layout(push_constant) uniform ObjectPush {
    mat4 model;
    vec4 baseColor;
    float roughness;
    float metallic;
} object;

layout(location = 0) out vec3 vWorldPosition;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vTexCoord;
layout(location = 3) out vec4 vBaseColor;

void main() {
    vec4 worldPosition = object.model * vec4(inPosition, 1.0);
    // Rotation only: the normal matrix is the inverse transpose of the upper
    // 3x3, which for the uniform scales used by the runtime is the same as the
    // model rotation. Non-uniform scale is handled on the CPU side.
    vec3 worldNormal = normalize(mat3(object.model) * inNormal);

    vWorldPosition = worldPosition.xyz;
    vNormal = worldNormal;
    vTexCoord = inTexCoord;
    vBaseColor = object.baseColor;

    gl_Position = scene.viewProjection * worldPosition;
}
