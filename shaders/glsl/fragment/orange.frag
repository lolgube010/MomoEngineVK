#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#define USE_BINDLESS
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor; // vertex color ofc
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = vec4(1.f,0.549f,0.f,1.f); return;
}
