#pragma once
#include <vk/gpu_types.h>

struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> _bindings;

	void Add_Binding(uint32_t aBinding, VkDescriptorType aType);
	void Clear();
	VkDescriptorSetLayout Build(VkDevice aDevice, VkShaderStageFlags aShaderStages, const char* aName, const void* a_pNext = nullptr, VkDescriptorSetLayoutCreateFlags aFlags = 0);
};

struct DescriptorAllocatorGrowable 
{
	struct PoolSizeRatio 
	{
		VkDescriptorType _type;
		float _ratio;
	};

	void Init(VkDevice aDevice, uint32_t aMaxSets, std::span<PoolSizeRatio> aPoolRatios, const char* aName, VkDescriptorPoolCreateFlags aPoolFlags = 0);
	void Clear_Pools(VkDevice aDevice);
	void Destroy_Pools(VkDevice aDevice);

	VkDescriptorSet Allocate(VkDevice aDevice, VkDescriptorSetLayout aLayout, const char* aNewName, const void* a_pNext = nullptr);
private:
	VkDescriptorPool Get_Pool(VkDevice aDevice, const char* aNewPoolName);
	VkDescriptorPool Create_Pool(VkDevice aDevice, uint32_t aSetCount, std::span<PoolSizeRatio> aPoolRatios, const char* aName) const;

	std::vector<PoolSizeRatio> _ratios;
	std::vector<VkDescriptorPool> _full_pools;
	std::vector<VkDescriptorPool> _ready_pools;
	uint32_t _sets_per_pool = 0;
	VkDescriptorPoolCreateFlags _poolFlags = 0;
};

struct DescriptorWriter 
{
	std::deque<VkDescriptorImageInfo> _imageInfos;
	std::deque<VkDescriptorBufferInfo> _bufferInfos;
	std::vector<VkWriteDescriptorSet> _writes;

	void Write_Image(int aBinding, VkImageView aImage, VkSampler aSampler, VkImageLayout aLayout, VkDescriptorType aType);
	void Write_Buffer(int aBinding, VkBuffer aBuffer, size_t aSize, size_t aOffset, VkDescriptorType aType);

	void Clear();
	void Update_Set(VkDevice aDevice, VkDescriptorSet aSet);
};
