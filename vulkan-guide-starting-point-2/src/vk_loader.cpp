#include "stb_image.h"
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

std::optional<std::vector<std::shared_ptr<MeshAsset>>> LoadGltfMeshes(VulkanEngine* aEngine, const std::filesystem::path& aFilePath)
{
    std::cout << "Loading GLTF: " << aFilePath << '\n';

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(aFilePath);

    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

    fastgltf::Asset gltf;
    fastgltf::Parser parser{};

    auto load = parser.loadBinaryGLTF(&data, aFilePath.parent_path(), gltfOptions);
    
	if (load) 
    {
        gltf = std::move(load.get());
    }
    else 
    {
        fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes;

    // use the same vectors for all meshes so that the memory doesn't reallocate as often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    for (fastgltf::Mesh& mesh : gltf.meshes) 
    {
        MeshAsset newMesh;

        newMesh.name = mesh.name;

        // clear the mesh arrays each mesh, we don't want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) 
        {
            GeoSurface newSurface;
            newSurface.startIndex = static_cast<uint32_t>(indices.size());
            newSurface.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexAccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                    [&](std::uint32_t idx) 
                    {
                        indices.push_back(idx + static_cast<uint32_t>(initial_vtx));
                    });
            }

            // load vertex positions, this is guaranteed to be in the file. the other info isn't.
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index)
                    {
                        Vertex new_vtx;
                        new_vtx.pos = v;
                        new_vtx.normal = { 1, 0, 0 };
                        new_vtx.color = glm::vec4{ 1.f };
                        new_vtx.uv_x = 0;
                        new_vtx.uv_y = 0;
                        vertices[initial_vtx + index] = new_vtx;
                    });
            }

	        // load vertex normals. data except position isn't guaranteed to exist so we need to check first.
            {
	            auto normals = p.findAttribute("NORMAL");
	            if (normals != p.attributes.end()) 
	            {
	                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->second],
	                    [&](glm::vec3 v, size_t index) {
	                        vertices[initial_vtx + index].normal = v;
	                    });
	            }
            }

            // load UVs
            {
	            auto uv = p.findAttribute("TEXCOORD_0");
	            if (uv != p.attributes.end()) 
	            {
	                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->second],
	                    [&](glm::vec2 v, size_t index) {
	                        vertices[initial_vtx + index].uv_x = v.x;
	                        vertices[initial_vtx + index].uv_y = v.y;
	                    });
	            }
            }

            // load vertex colors
            {
	            auto colors = p.findAttribute("COLOR_0");
	            if (colors != p.attributes.end()) 
	            {
	                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->second],
	                    [&](glm::vec4 v, size_t index) {
	                        vertices[initial_vtx + index].color = v;
	                    });
	            }
            }
            newMesh.surfaces.push_back(newSurface);
        }

        // override the vertex colors with the vertex normals which is useful for debugging
        constexpr bool OverrideColors = false;
        if (OverrideColors) 
        {
            for (Vertex& vtx : vertices) 
            {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }

        // if we ever want to do something with the model data while it still lives on the cpu, THIS is that moment. after this they're gpu only.

        // where we create and fill our buffers.
        newMesh.meshBuffers = aEngine->UploadMesh(indices, vertices);

        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
    }

    return meshes;
}
