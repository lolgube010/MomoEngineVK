#include <vk_descriptors.h>

void DescriptorLayoutBuilder::Add_Binding(const uint32_t aBinding, const VkDescriptorType aType)
{
	VkDescriptorSetLayoutBinding newBind{};
	newBind.binding = aBinding;
	newBind.descriptorCount = 1;
	newBind.descriptorType = aType;

	_bindings.push_back(newBind);
}

void DescriptorLayoutBuilder::Clear()
{
	_bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::Build(const VkDevice aDevice, const VkShaderStageFlags aShaderStages, const void* a_pNext, const VkDescriptorSetLayoutCreateFlags aFlags)
{
	for (auto& b : _bindings)
	{
		b.stageFlags |= aShaderStages;
	}

	VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	info.pNext = a_pNext;

	info.pBindings = _bindings.data();
	info.bindingCount = static_cast<uint32_t>(_bindings.size());
	info.flags = aFlags;

	VkDescriptorSetLayout set;
	VK_CHECK(vkCreateDescriptorSetLayout(aDevice, &info, nullptr, &set));

	return set;

}

/// <param name="aMaxSets">controls how many VkDescriptorSets we can create from the pool in total, and the pool sizes give how many individual bindings of a given type are owned.</param>
/// <param name="aPoolRatios">A ratio to multiply the maxSets parameter. This lets us directly control how big the pool is going to be. </param>
void DescriptorAllocator::Init_Pool(const VkDevice aDevice, const uint32_t aMaxSets, const std::span<PoolSizeRatio> aPoolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : aPoolRatios)
	{
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio._type,
			.descriptorCount = static_cast<uint32_t>(ratio._ratio * aMaxSets)
		});
	}

	VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	pool_info.flags = 0;
	pool_info.maxSets = aMaxSets;
	pool_info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	pool_info.pPoolSizes = poolSizes.data();

	vkCreateDescriptorPool(aDevice, &pool_info, nullptr, &_pool);
}

void DescriptorAllocator::Clear_Descriptors(const VkDevice aDevice) const
{
	vkResetDescriptorPool(aDevice, _pool, 0);
}

void DescriptorAllocator::Destroy_Pool(const VkDevice aDevice) const
{
	vkDestroyDescriptorPool(aDevice, _pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::Allocate(const VkDevice aDevice, const VkDescriptorSetLayout aLayout) const
{
	VkDescriptorSetAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = _pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &aLayout;

	VkDescriptorSet ds;
	VK_CHECK(vkAllocateDescriptorSets(aDevice, &allocInfo, &ds));

	return ds;
}
