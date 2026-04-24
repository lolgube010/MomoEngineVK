// Assuming your C++ math library (like GLM) uses column-major matrices.
// Change this to row_major if you use a row-major math library.
#pragma pack_matrix(column_major) 

// --- Descriptor Sets ---

struct SceneData
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewproj;
    float4 ambientColor;
    float4 sunlightDirection;
    float4 sunlightColor;
};

struct GLTFMaterialData
{
    float4 colorFactors;
    float4 metal_rough_factors;
};

// layout(set = 0, binding = 0)
// b = buffer, 0 = binding 0, space0 = set 0
ConstantBuffer<SceneData> sceneData : register(b0, space0);

// layout(set = 1, binding = 0)
ConstantBuffer<GLTFMaterialData> materialData : register(b0, space1);

// layout(set = 1, binding = 1)
// Assigning the same register maps BOTH the texture (t) and sampler (s) 
// to a single CombinedImageSampler at binding 1, set 1!
Texture2D colorTex : register(t1, space1);
SamplerState colorSampler : register(s1, space1);

// layout(set = 1, binding = 2)
Texture2D metalRoughTex : register(t2, space1);
SamplerState metalRoughSampler : register(s2, space1);

// --- Structs & Push Constants ---

struct Vertex
{
    float3 position;
    float uv_x;
    float3 normal;
    float uv_y;
    float4 color;
};

struct PushConstants
{
    float4x4 render_matrix;
    uint64_t vertexBuffer; // Replaces layout(buffer_reference)
};

[[vk::push_constant]] PushConstants pushConstants;

// --- Outputs ---

struct VSOutput
{
    // SV_Position replaces gl_Position
    float4 position : SV_Position;
    
    // Explicitly mapping locations to match GLSL
    [[vk::location(0)]] float3 outNormal : NORMAL0;
    [[vk::location(1)]] float3 outColor : COLOR0;
    [[vk::location(2)]] float2 outUV : TEXCOORD0;
};

// gl_VertexIndex becomes SV_VertexID
VSOutput main(uint vertexIndex : SV_VertexID)
{
    VSOutput output;

    // 1. Calculate the byte offset for this specific vertex.
    // (Ensure this matches your C++ struct size! 
    // float3 + float + float3 + float + float4 = 12 floats = 48 bytes)
    uint byteStride = 48;
    uint byteOffset = vertexIndex * byteStride;

    // 2. Load the struct directly from the 64-bit address using the Vulkan intrinsic
    Vertex v = vk::RawBufferLoad < Vertex > (pushConstants.vertexBuffer + byteOffset);
    
    float4 pos = float4(v.position, 1.0f);

    // 2. Matrix Math (GLSL uses A * B, HLSL uses mul(A, B))
    float4 worldPos = mul(pushConstants.render_matrix, pos);
    output.position = mul(sceneData.viewproj, worldPos);
    
    // output.position.y = -output.position.y;

    output.outNormal = mul(pushConstants.render_matrix, float4(v.normal, 0.0f)).xyz;
    output.outColor = v.color.xyz * materialData.colorFactors.xyz;
    
    output.outUV.x = v.uv_x;
    output.outUV.y = v.uv_y;

    return output;
}