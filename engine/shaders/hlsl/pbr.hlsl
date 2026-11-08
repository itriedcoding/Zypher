// Zypher standard mesh shaders for Direct3D 12 (compiled with DXC).
// Both entry points live in one file so the runtime can create the pipeline
// from a single bytecode blob.

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
    float2 padding;
};

ConstantBuffer<SceneConstants> gScene : register(b0);
ConstantBuffer<ObjectConstants> gObject : register(b1);
Texture2D gAlbedo : register(t0);
SamplerState gAlbedoSampler : register(s0);

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float4 baseColor : TEXCOORD3;
};

VertexOutput VSMain(VertexInput input) {
    VertexOutput output;
    float4 worldPosition = mul(gObject.model, float4(input.position, 1.0));
    output.position = mul(gScene.viewProjection, worldPosition);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul((float3x3)gObject.model, input.normal));
    output.texCoord = input.texCoord;
    output.baseColor = gObject.baseColor;
    return output;
}

float3 LinearToSrgb(float3 color) {
    return pow(saturate(color), 1.0 / 2.2);
}

float4 PSMain(VertexOutput input) : SV_Target {
    float3 albedo = input.baseColor.rgb * gAlbedo.Sample(gAlbedoSampler, input.texCoord).rgb;
    float3 normal = normalize(input.normal);
    float3 lightDir = normalize(-gScene.lightDirection.xyz);
    float diffuse = saturate(dot(normal, lightDir)) * gScene.lightColor.a;
    float3 lit = albedo * (0.20 + diffuse) * gScene.lightColor.rgb;
    return float4(LinearToSrgb(lit), input.baseColor.a);
}
