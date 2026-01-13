// HLSL - Fragment shader (Pixel Shader)
// For Vulkan + custom engine (typically compiled with DXC to SPIR-V)

Texture2D displayTexture : register(t0);
SamplerState samplerDisplay : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET0
{
    return displayTexture.Sample(samplerDisplay, input.uv);
}