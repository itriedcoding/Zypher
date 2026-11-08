#version 450 core

// Zypher standard material shader. Kept close to the software rasteriser's
// lighting model (Lambert + ambient) so frames rendered by either backend match
// closely, which makes the headless render tests meaningful.

layout(location = 0) in vec3 vWorldPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;
layout(location = 3) in vec4 vBaseColor;

layout(set = 0, binding = 0) uniform SceneUniforms {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 lightDirection;
    vec4 lightColor;
} scene;

layout(set = 0, binding = 1) uniform sampler2D albedoTexture;

layout(location = 0) out vec4 outColor;

const float kAmbient = 0.20;

vec3 linearToSrgb(vec3 color) {
    return pow(clamp(color, vec3(0.0), vec3(1.0)), vec3(1.0 / 2.2));
}

void main() {
    vec3 albedo = vBaseColor.rgb * texture(albedoTexture, vTexCoord).rgb;
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(-scene.lightDirection.xyz);
    float diffuse = max(dot(normal, lightDir), 0.0) * scene.lightColor.a;

    vec3 lit = albedo * (kAmbient + diffuse) * scene.lightColor.rgb;
    outColor = vec4(linearToSrgb(lit), vBaseColor.a);
}
