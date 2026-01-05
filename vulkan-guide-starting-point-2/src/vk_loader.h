#pragma once
// Will contain GLTF loading logic

#include <vk_types.h>
#include <unordered_map>
#include <filesystem>

struct GeoSurface 
{
    uint32_t startIndex;
    uint32_t count;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces; // submeshes of this specific mesh
    GPUMeshBuffers meshBuffers;
};

//forward declaration
class VulkanEngine;

// std optional allows our vector to be errored / null. 
std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(VulkanEngine* aEngine, const std::filesystem::path& aFilePath);