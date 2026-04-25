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
    vec4 colorTexture = texture(allTextures[materialData.colorTexID], inUV);
    if(colorTexture.a < materialData.alphaCutOff)
    {
        discard;
    }
    //outFragColor = vec4(1.f,1.f,1.f,1.f); return;
    //outFragColor = vec4(normalize(inNormal) * 0.5 + 0.5, 1.f); return;
    //outFragColor = vec4(inColor.xyz, 1.f); return;
    vec4 metalRough = texture(allTextures[materialData.metalRoughTexID], inUV);
    float roughness = metalRough.g * materialData.metal_rough_factors.y;
    float metallic  = metalRough.b * materialData.metal_rough_factors.x;

    float lightValue = max(dot(normalize(inNormal), sceneData.sunlightDirection.xyz), 0.1f);
    vec3 color   = inColor * colorTexture.xyz * materialData.colorFactors.xyz;
    vec3 ambient = color * sceneData.ambientColor.xyz;

    // Metals have no diffuse; attenuate by (1 - metallic)
    vec3 diffuse = color * (1.0 - metallic) * lightValue * sceneData.sunlightColor.w;

    outFragColor = vec4(diffuse + ambient, 1.0);
}
