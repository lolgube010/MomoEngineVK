// Target: Model 6.6 or higher recommended for 64-bit integers and explicit resource binding
// Compile with: -T vs_6_6 -E main -fspv-target-env=vulkan1.2

struct Vertex
{
    float3 position;
    float uv_x;
    float3 normal;
    float uv_y;
    float4 color;
};

// Define Push Constants
struct PushConstants
{
    // GLSL mat4 is column-major by default. HLSL is row-major.
    // We specify column_major to match the GLSL memory layout expectation.
    column_major float4x4 render_matrix;
    
    // In GLSL this was a reference (pointer). 
    // In HLSL, we pass the 64-bit Buffer Device Address directly.
    uint64_t vertexBufferPtr;
};

// Use the [[vk::push_constant]] attribute
[[vk::push_constant]]
PushConstants PushConsts;
//ConstantBuffer<PushConstants> PushConsts;

// Define Outputs
struct VSOutput
{
    [[vk::location(0)]] float3 outColor : COLOR0;
    [[vk::location(1)]] float2 outUV : TEXCOORD0;
    float4 Pos : SV_Position;
};

VSOutput main(uint v_id : SV_VertexID)
{
    VSOutput output = (VSOutput) 0;

    // --- Buffer Reference Logic ---
    // Calculate the memory address for the current vertex.
    // HLSL packs float3 + float tightly (16 bytes), matching the GLSL std430 layout of this struct (48 bytes total).
    uint64_t vertexAddress = PushConsts.vertexBufferPtr + (uint64_t) v_id * sizeof(Vertex);

    // Load the vertex data using the buffer address.
    // vk::RawBufferLoad requires the 'vk' namespace (available in DXC for SPIR-V).
    Vertex v = vk::RawBufferLoad < Vertex > (vertexAddress);

    // --- Output Data ---
    // Note: mul(Matrix, Vector) in HLSL performs the standard transformation when using column_major matrices.
    output.Pos = mul(PushConsts.render_matrix, float4(v.position, 1.0f));
    
    output.outColor = v.color.xyz;
    output.outUV.x = v.uv_x;
    output.outUV.y = v.uv_y;

    return output;
}