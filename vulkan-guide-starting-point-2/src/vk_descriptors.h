#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> _bindings;

	void Add_Binding(uint32_t aBinding, VkDescriptorType aType);
	void Clear();
	VkDescriptorSetLayout Build(VkDevice aDevice, VkShaderStageFlags aShaderStages, const void* a_pNext = nullptr, VkDescriptorSetLayoutCreateFlags aFlags = 0);
};

struct DescriptorAllocatorGrowable 
{
public:
	struct PoolSizeRatio 
	{
		VkDescriptorType _type;
		float _ratio;
	};

	void Init(VkDevice aDevice, uint32_t aMaxSets, std::span<PoolSizeRatio> aPoolRatios);
	void Clear_Pools(VkDevice aDevice);
	void Destroy_Pools(VkDevice aDevice);

	VkDescriptorSet Allocate(VkDevice aDevice, VkDescriptorSetLayout aLayout, const void* a_pNext = nullptr);
private:
	VkDescriptorPool Get_Pool(VkDevice aDevice);
	static VkDescriptorPool Create_Pool(VkDevice aDevice, uint32_t aSetCount, std::span<PoolSizeRatio> aPoolRatios);

	std::vector<PoolSizeRatio> _ratios;
	std::vector<VkDescriptorPool> _full_pools;
	std::vector<VkDescriptorPool> _ready_pools;
	uint32_t _sets_per_pool = 0;
};

struct DescriptorWriter 
{
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void Write_Buffer(int aBinding, VkBuffer aBuffer, size_t aSize, size_t aOffset, VkDescriptorType aType);

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set);
};
