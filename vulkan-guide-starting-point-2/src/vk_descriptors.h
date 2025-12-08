#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> _bindings;

	void Add_Binding(uint32_t aBinding, VkDescriptorType aType);
	void Clear();
	VkDescriptorSetLayout Build(VkDevice aDevice, VkShaderStageFlags aShaderStages, const void* a_pNext = nullptr, VkDescriptorSetLayoutCreateFlags aFlags = 0);
};

struct DescriptorAllocator
{
	struct PoolSizeRatio
	{
		VkDescriptorType _type;
		float _ratio;
	};

	VkDescriptorPool _pool;

	void Init_Pool(VkDevice aDevice, uint32_t aMaxSets, std::span<PoolSizeRatio> aPoolRatios);

	// The clear function is not a 'delete', but a reset. It will destroy all the descriptors created from the pool and put it back to initial state, but won't delete the VkDescriptorPool itself.
	void Clear_Descriptors(VkDevice aDevice) const;
	void Destroy_Pool(VkDevice aDevice) const;

	VkDescriptorSet Allocate(VkDevice aDevice, VkDescriptorSetLayout aLayout) const;
};
