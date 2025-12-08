// HLSL version for Vulkan

// Map to set 0, binding 0. 
// Note: rgba16f handles float4 data, so RWTexture2D<float4> is appropriate.
[[vk::binding(0, 0)]] RWTexture2D<float4> image;

[numthreads(16, 16, 1)]
void main(uint3 globalID : SV_DispatchThreadID, uint3 localID : SV_GroupThreadID)
{
    uint width, height;
    image.GetDimensions(width, height);

    // Boundary check using Global ID
    if (globalID.x < width && globalID.y < height)
    {
        float4 color = float4(0.0, 0.0, 0.0, 1.0);

        // Check Local ID (gl_LocalInvocationID) to create the grid pattern
        if (localID.x != 0 && localID.y != 0)
        {
            color.x = (float) globalID.x / (float) width;
            color.y = (float) globalID.y / (float) height;
        }

        image[globalID.xy] = color;
    }
}