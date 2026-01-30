// HLSL version for Vulkan

// 1. Define the Push Constant structure
struct Constants
{
    float4 data1;
    float4 data2;
    float4 data3;
    float4 data4;
};

// 2. Declare the global variable with the specific attribute
[[vk::push_constant]] Constants PushConstants;

// 3. Bind the image (Set 0, Binding 0)
[[vk::binding(0, 0)]] 
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> image;

[numthreads(16, 16, 1)]
void main(uint3 globalID : SV_DispatchThreadID)
{
    uint width, height;
    image.GetDimensions(width, height);
    
    // Boundary check
    if (globalID.x < width && globalID.y < height)
    {
        // Access data from the PushConstant struct
        float4 topColor = PushConstants.data1;
        float4 bottomColor = PushConstants.data2;
        
        // Calculate blend factor
        // Note: Explicit casting to float is important here
        float blend = (float) globalID.y / (float) height;
        
        // 'mix' in GLSL is 'lerp' (Linear Interpolation) in HLSL
        float4 finalColor = lerp(topColor, bottomColor, blend);
        
        image[globalID.xy] = finalColor;
    }
}