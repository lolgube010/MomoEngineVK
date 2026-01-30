// HLSL equivalent

// The return value acts as the output. 
// SV_Target maps to layout(location = 0) out automatically, 
// or you can use [[vk::location(0)]] explicitly.

float4 main([[vk::location(0)]] float3 inColor : COLOR) : SV_TARGET
{
    // return red (combined with alpha)
    return float4(inColor, 1.0f);
}