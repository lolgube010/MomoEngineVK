#version 450
#extension GL_EXT_buffer_reference : require

struct DebugVertex {
    vec3 pos;
    uint color;
};

layout(buffer_reference, std430) readonly buffer DebugVertexBuffer {
    DebugVertex vertices[];
};

layout(push_constant) uniform constants {
    mat4 viewProj;
    DebugVertexBuffer vertexBuffer;
} pc;

layout(location = 0) out vec4 outColor;

void main()
{
    DebugVertex v = pc.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = pc.viewProj * vec4(v.pos, 1.0);
    uint c = v.color;
    outColor = vec4(
        float( c         & 0xFFu) / 255.0,
        float((c >>  8u) & 0xFFu) / 255.0,
        float((c >> 16u) & 0xFFu) / 255.0,
        float((c >> 24u) & 0xFFu) / 255.0
    );
}
