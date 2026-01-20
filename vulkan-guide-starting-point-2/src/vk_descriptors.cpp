#include <vk_descriptors.h>

#include <algorithm>

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

void DescriptorAllocatorGrowable::Init(const VkDevice aDevice, const uint32_t aMaxSets, const std::span<PoolSizeRatio> aPoolRatios)
{
	_ratios.clear();

	for (auto r : aPoolRatios) 
	{
		_ratios.push_back(r);
	}

	const VkDescriptorPool newPool = Create_Pool(aDevice, aMaxSets, aPoolRatios);

	_sets_per_pool = static_cast<uint32_t>(static_cast<double>(aMaxSets) * 1.5); //grow it next allocation

	_ready_pools.push_back(newPool);
}

void DescriptorAllocatorGrowable::Clear_Pools(const VkDevice aDevice)
{
	for (const auto p : _ready_pools) 
	{
		vkResetDescriptorPool(aDevice, p, 0);
	}
	for (auto p : _full_pools) 
	{
		vkResetDescriptorPool(aDevice, p, 0);
		_ready_pools.push_back(p);
	}
	_full_pools.clear();
}

void DescriptorAllocatorGrowable::Destroy_Pools(const VkDevice aDevice)
{
	for (const auto p : _ready_pools) 
	{
		vkDestroyDescriptorPool(aDevice, p, nullptr);
	}
	_ready_pools.clear();
	for (const auto p : _full_pools) 
	{
		vkDestroyDescriptorPool(aDevice, p, nullptr);
	}
	_full_pools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::Allocate(const VkDevice aDevice, const VkDescriptorSetLayout aLayout, const void* a_pNext)
{
	//get or create a pool to allocate from
	VkDescriptorPool poolToUse = Get_Pool(aDevice);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.pNext = a_pNext;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = poolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &aLayout;

	VkDescriptorSet ds;

	//allocation failed. Try again
	if (const VkResult result = vkAllocateDescriptorSets(aDevice, &allocInfo, &ds); 
		result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) 
	{
		_full_pools.push_back(poolToUse);

		poolToUse = Get_Pool(aDevice);
		allocInfo.descriptorPool = poolToUse;

		// If the second time fails too, stuff is completely broken so it just asserts and crashes.
		VK_CHECK(vkAllocateDescriptorSets(aDevice, &allocInfo, &ds)); 
	}

	_ready_pools.push_back(poolToUse);
	return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::Get_Pool(const VkDevice aDevice)
{
	VkDescriptorPool newPool;
	if (!_ready_pools.empty()) 
	{
		// An important detail on this function is that we are removing the pool from the readyPools array when grabbing it. This is so then we can add it back into that array or the other one once a descriptor is allocated.
		newPool = _ready_pools.back();
		_ready_pools.pop_back();
	}
	else 
	{
		//need to create a new pool
		newPool = Create_Pool(aDevice, _sets_per_pool, _ratios);

		_sets_per_pool = static_cast<uint32_t>(static_cast<double>(_sets_per_pool) * 1.5);
		_sets_per_pool = std::min<uint32_t>(_sets_per_pool, 4092); // can modify 4092 if we want to
	}

	return newPool;

}

VkDescriptorPool DescriptorAllocatorGrowable::Create_Pool(const VkDevice aDevice, const uint32_t aSetCount, const std::span<PoolSizeRatio> aPoolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (const auto [type, ratio] : aPoolRatios) 
	{
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = type,
			.descriptorCount = static_cast<uint32_t>(ratio * aSetCount)
			});
	}

	VkDescriptorPoolCreateInfo pool_Info = {};
	pool_Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_Info.flags = 0;
	pool_Info.maxSets = aSetCount;
	pool_Info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	pool_Info.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(aDevice, &pool_Info, nullptr, &newPool);
	return newPool;
}



/// <summary>
/// </summary>
/// <param name="aBinding"></param>
/// <param name="aImage"></param>
/// <param name="aSampler"></param>
/// <param name="aLayout">For Shader read: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL. For Compute R/W: VK_IMAGE_LAYOUT_GENERAL</param>
/// <param name="aType">The 3 parameters in the ImageInfo can be optional, depending on the specific VkDescriptorType.
// VK_DESCRIPTOR_TYPE_SAMPLER is JUST the sampler, so it does not need ImageView or layout to be set.
// VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE doesn't need the sampler set because it's going to be accessed with different samplers within the shader, this descriptor type is just a pointer to the image.
// VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER needs everything set, as it holds the information for both the sampler, and the image it samples.This is a useful type because it means we only need 1 descriptor binding to access the texture.
// VK_DESCRIPTOR_TYPE_STORAGE_IMAGE was used back in chapter 2, it does not need sampler, and it's used to allow compute shaders to directly access pixel data.
//</param>
void DescriptorWriter::Write_Image(const int aBinding, const VkImageView aImage, const VkSampler aSampler, const VkImageLayout aLayout, const VkDescriptorType aType)
{
	const VkDescriptorImageInfo& info = _image_infos.emplace_back(VkDescriptorImageInfo{
	.sampler = aSampler,
	.imageView = aImage,
	.imageLayout = aLayout
		});

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = aBinding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = aType;
	write.pImageInfo = &info;

	_writes.push_back(write);
}


// The descriptor types that are allowed for a buffer are these.
// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC

// Vulkan Type,				Read/Write,	Size Limit,				Offset Fixed?,		DX11 Closest Equivalent,				Typical Use Case
// UNIFORM_BUFFER,			Read-only,	"Small (e.g., ≤64KB)",	Yes,				Constant buffer(cbuffer),				Per - frame / camera data
// STORAGE_BUFFER,			Read+Write,	Large,					Yes,				StructuredBuffer / RWStructuredBuffer,	"Particles, compute data"
// UNIFORM_BUFFER_DYNAMIC,	Read-only,	Small,					No(dynamic),		(No direct; multiple cbuffers),			"Many objects, one big UBO"
// STORAGE_BUFFER_DYNAMIC,	Read+Write,	Large,					No(dynamic),		(No direct; multiple SRVs / UAVs),		"Many objects, one big SSBO"
void DescriptorWriter::Write_Buffer(const int aBinding, const VkBuffer aBuffer, const size_t aSize, const size_t aOffset, const VkDescriptorType aType)
{
	const VkDescriptorBufferInfo& info = _buffer_infos.emplace_back(VkDescriptorBufferInfo{
		.buffer = aBuffer,
		.offset = aOffset,
		.range = aSize
		});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = aBinding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = aType;
	write.pBufferInfo = &info; // We have created the info by doing emplace_back on the std::deque, so it's fine to take a pointer to it.

	_writes.push_back(write);
}

void DescriptorWriter::Clear()
{
	_image_infos.clear();
	_writes.clear();
	_buffer_infos.clear();
}

void DescriptorWriter::Update_Set(const VkDevice aDevice, const VkDescriptorSet aSet)
{
	for (VkWriteDescriptorSet& write : _writes) 
	{
		write.dstSet = aSet;
	}

	vkUpdateDescriptorSets(aDevice, static_cast<uint32_t>(_writes.size()), _writes.data(), 0, nullptr);

}
