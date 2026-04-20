#version 450

#extension GL_GOOGLE_include_directive : require
//#include "input_structures.glsl"
//
//layout (location = 0) in vec3 inNormal;
//layout (location = 1) in vec3 inColor;
//layout (location = 2) in vec2 inUV;
//
layout (location = 0) out vec4 outFragColor;
//
void main() 
{
	outFragColor = vec4(1.f,0.f,1.f,1.f); return;
//    //outFragColor = vec4(inNormal.xyz, 1.f); return;
//    outFragColor = vec4(inColor.xy, 0.f, 1.f); return;
//    vec4 metalRough = texture(metalRoughTex, inUV);
//    //outFragColor = metalRough; return;
//    float roughness = metalRough.g * materialData.metal_rough_factors.y;
//    float metallic  = metalRough.b * materialData.metal_rough_factors.x;
//
//    float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
//
//    vec3 color   = inColor * texture(colorTex, inUV).xyz *
//    materialData.colorFactors.xyz;
//    vec3 ambient = color * sceneData.ambientColor.xyz;
//
//    // Metals have no diffuse; attenuate by (1 - metallic)
//    vec3 diffuse = color * (1.0 - metallic) * lightValue * sceneData.sunlightColor.w;
//
//    outFragColor = vec4(diffuse + ambient, 1.0);
}