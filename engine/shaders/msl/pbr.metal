// Zypher standard mesh shaders for the Metal backend (macOS).
// Compiled at build time with `xcrun metal`; the runtime loads the resulting
// .metallib.

#include <metal_stdlib>
using namespace metal;

struct SceneConstants {
    float4x4 viewProjection;
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
};

struct ObjectConstants {
    float4x4 model;
    float4 baseColor;
    float roughness;
    float metallic;
};

struct VertexInput {
    float3 position [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float2 texCoord [[attribute(2)]];
};

struct VertexOutput {
    float4 position [[position]];
    float3 worldPosition;
    float3 normal;
    float2 texCoord;
    float4 baseColor;
};

vertex VertexOutput pbr_vertex(VertexInput input [[stage_in]],
                               constant SceneConstants& scene [[buffer(1)]],
                               constant ObjectConstants& object [[buffer(2)]]) {
    VertexOutput output;
    float4 worldPosition = object.model * float4(input.position, 1.0);
    output.position = scene.viewProjection * worldPosition;
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize((object.model * float4(input.normal, 0.0)).xyz);
    output.texCoord = input.texCoord;
    output.baseColor = object.baseColor;
    return output;
}

fragment float4 pbr_fragment(VertexOutput input [[stage_in]],
                             constant SceneConstants& scene [[buffer(1)]],
                             texture2d<float> albedoTexture [[texture(0)]],
                             sampler albedoSampler [[sampler(0)]]) {
    float3 albedo = input.baseColor.rgb * albedoTexture.sample(albedoSampler, input.texCoord).rgb;
    float3 normal = normalize(input.normal);
    float3 lightDirection = normalize(-scene.lightDirection.xyz);
    float diffuse = saturate(dot(normal, lightDirection)) * scene.lightColor.a;
    float3 lit = albedo * (0.20 + diffuse) * scene.lightColor.rgb;
    return float4(pow(saturate(lit), float3(1.0 / 2.2)), input.baseColor.a);
}
