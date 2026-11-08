// Zypher standard shaders authored once in Slang and compiled to SPIR-V,
// DXIL and Metal with the Slang compiler (see docs/building.md).

struct SceneConstants
{
    float4x4 viewProjection;
    float4 cameraPosition;
    float4 lightDirection;
    float4 lightColor;
};

struct ObjectConstants
{
    float4x4 model;
    float4 baseColor;
    float roughness;
    float metallic;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    float4 baseColor : TEXCOORD3;
};

ParameterBlock<SceneConstants> scene;
ParameterBlock<ObjectConstants> object;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input)
{
    VertexOutput output;
    float4 worldPosition = mul(object.model, float4(input.position, 1.0));
    output.position = mul(scene.viewProjection, worldPosition);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul((float3x3)object.model, input.normal));
    output.texCoord = input.texCoord;
    output.baseColor = object.baseColor;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target
{
    float3 albedo = input.baseColor.rgb;
    float3 normal = normalize(input.normal);
    float3 lightDirection = normalize(-scene.lightDirection.xyz);
    float diffuse = saturate(dot(normal, lightDirection)) * scene.lightColor.a;
    float3 lit = albedo * (0.20 + diffuse) * scene.lightColor.rgb;
    return float4(pow(saturate(lit), 1.0 / 2.2), input.baseColor.a);
}
