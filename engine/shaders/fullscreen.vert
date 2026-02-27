#version 450

// Pass the UV coordinates to the fragment shader
layout(location = 0) out vec2 outUV;

void main() {
    // Generate UV coordinates: 
    // gl_VertexIndex 0 -> UV: (0.0, 0.0)
    // gl_VertexIndex 1 -> UV: (2.0, 0.0)
    // gl_VertexIndex 2 -> UV: (0.0, 2.0)
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);

    // Convert UVs to Vulkan's Normalized Device Coordinates (NDC)
    // Vulkan NDC is X: [-1, 1], Y: [-1, 1]. 
    // IMPORTANT: In Vulkan, Y points DOWN. So (-1, -1) is the Top-Left corner!
    gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
}