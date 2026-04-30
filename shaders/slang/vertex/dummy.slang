struct VertexOutput
{
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput main(uint vertexID : SV_VertexID)
{
    VertexOutput output;
    output.position = float4(0.0, 0.0, 0.0, 1.0);
    return output;
}
