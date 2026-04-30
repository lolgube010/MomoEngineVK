#pragma once
#include <vk/render_types.h>
#include <vk/descriptors.h>

struct GLTFMetallic_Roughness
{
    MaterialPipeline _opaquePipeline;
    MaterialPipeline _transparentPipeline;
    MaterialPipeline _opaqueWireframePipeline;
    MaterialPipeline _transparentWireframePipeline;

    VkDescriptorSetLayout _materialLayout;

    struct MaterialConstants
    {
        glm::vec4 _colorFactors;
        glm::vec4 _metalRoughFactors;

        uint32_t _colorTexID;
        uint32_t _metalRoughTexID;
        float _alphaCutOff;
        uint32_t _pad2;
        glm::vec4 _extra[13] = {};
    };

    struct MaterialResources
    {
        AllocatedImage _colorImage;
        VkSampler _colorSampler;
        AllocatedImage _metalRoughImage;
        VkSampler _metalRoughSampler;
        VkBuffer _dataBuffer;
        uint32_t _dataBufferOffset;
        uint32_t _padding;
    };

    DescriptorWriter _writer;

    void Build_Pipelines(VkDevice aDevice, VkDescriptorSetLayout aSceneDataLayout,
                         VkFormat aDrawFormat, VkFormat aDepthFormat);
    void Clear_Resources(VkDevice aDevice) const;

    MaterialInstance Write_Material(VkDevice aDevice, MaterialPass aPass,
                                    const MaterialResources& aResources,
                                    DescriptorAllocatorGrowable& aDescriptorAllocator,
                                    const char* aName);
};

static_assert(sizeof(GLTFMetallic_Roughness::MaterialConstants) == 256);
