#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> _bindings;

	void Add_Binding(uint32_t aBinding, VkDescriptorType aType);
	void Clear();
	VkDescriptorSetLayout Build(VkDevice aDevice, VkShaderStageFlags aShaderStages, const void* a_pNext = nullptr, VkDescriptorSetLayoutCreateFlags aFlags = 0);
};

// struct DescriptorAllocator
// {
// 	struct PoolSizeRatio
// 	{
// 		VkDescriptorType _type;
// 		float _ratio;
// 	};
//
// 	VkDescriptorPool _pool;
//
// 	void Init_Pool(VkDevice aDevice, uint32_t aMaxSets, std::span<PoolSizeRatio> aPoolRatios);
//
// 	// The clear function is not a 'delete', but a reset. It will destroy all the descriptors created from the pool and put it back to initial state, but won't delete the VkDescriptorPool itself.
// 	void Clear_Descriptors(VkDevice aDevice) const;
// 	void Destroy_Pool(VkDevice aDevice) const;
//
// 	VkDescriptorSet Allocate(VkDevice aDevice, VkDescriptorSetLayout aLayout) const;
// };

struct DescriptorAllocatorGrowable 
{
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void Init(VkDevice aDevice, uint32_t aMaxSets, std::span<PoolSizeRatio> aPoolRatios);
	void Clear_Pools(VkDevice aDevice);
	void Destroy_Pools(VkDevice aDevice);

	VkDescriptorSet Allocate(VkDevice aDevice, VkDescriptorSetLayout aLayout, const void* a_pNext = nullptr);
private:
	VkDescriptorPool Get_Pool(VkDevice aDevice);
	static VkDescriptorPool Create_Pool(VkDevice aDevice, uint32_t aSetCount, std::span<PoolSizeRatio> aPoolRatios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;

};

struct DescriptorWriter {
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set);
};
