struct VSOutput
{
    float4 Pos : SV_POSITION; // Maps to gl_Position
    [[vk::location(0)]] float3 Color : COLOR0; // Maps to outColor
};

VSOutput main(uint VertexIndex : SV_VertexID) // SV_VertexID maps to gl_VertexIndex
{
    // Const array of positions for the triangle
    // Note: HLSL array syntax uses curly braces
    const float3 positions[3] =
    {
        float3(1.f, 1.f, 0.0f),
        float3(-1.f, 1.f, 0.0f),
        float3(0.f, -1.f, 0.0f)
    };

    // Const array of colors for the triangle
    const float3 colors[3] =
    {
        float3(1.0f, 0.0f, 0.0f), // red
        float3(0.0f, 1.0f, 0.0f), // green
        float3(0.0f, 0.0f, 1.0f) // blue
    };

    VSOutput output;

    // Output the position of each vertex
    output.Pos = float4(positions[VertexIndex], 1.0f);
    
    // Output the color
    output.Color = colors[VertexIndex];

    return output;
}