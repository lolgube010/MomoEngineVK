// input_structures.hlsl

struct SceneDataStr // NOTE- THIS MIGHT BE BROKEN! DEBUG!
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewproj;
    float4 ambientColor;
    float4 sunlightDirection; // w for sun power
    float4 sunlightColor;
};

struct GLTFMaterialDataStr
{
    float4 colorFactors;
    float4 metal_rough_factors;
};

// Layout: Set 0, Binding 0
[[vk::binding(0, 0)]] ConstantBuffer<SceneDataStr> sceneData;

// Layout: Set 1, Binding 0
[[vk::binding(0, 1)]] ConstantBuffer<GLTFMaterialDataStr> materialData;

// Layout: Set 1, Binding 1 - Combined Image Sampler
[[vk::binding(1, 1)]] Texture2D<float4> colorTex;
[[vk::binding(1, 1)]] SamplerState colorTexSampler;

// Layout: Set 1, Binding 2 - Combined Image Sampler
[[vk::binding(2, 1)]] Texture2D<float4> metalRoughTex;
[[vk::binding(2, 1)]] SamplerState metalRoughTexSampler;

struct PSInput
{
    [[vk::location(0)]] float3 Normal : NORMAL;
    [[vk::location(1)]] float3 Color : COLOR;
    [[vk::location(2)]] float2 UV : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    // Calculate light intensity
    // Note: sunlightDirection is float4, so we take .xyz for the dot product
    float lightValue = max(dot(input.Normal, sceneData.sunlightDirection.xyz), 0.1f);

    // Sample texture and multiply by vertex color
    // In HLSL, we use the .Sample() method with the paired SamplerState
    float3 color = input.Color * colorTex.Sample(colorTexSampler, input.UV).xyz;
    
    // Calculate ambient
    float3 ambient = color * sceneData.ambientColor.xyz;

    // Combine diffuse (sunlight) and ambient
    // sceneData.sunlightColor.w represents sun power/intensity
    float3 finalColor = color * lightValue * sceneData.sunlightColor.w + ambient;

    return float4(finalColor, 1.0f);
}