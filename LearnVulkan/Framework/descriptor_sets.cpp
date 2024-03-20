#include "descriptor_sets.h"
#include "error.h"

VkDescriptorPool create_descriptor_pool(VkDevice device, VkDescriptorType type, bool descriptor_sets_individually_resetable, std::size_t max_sets, std::size_t max_descriptors_per_set)
{
	VkDescriptorPool descriptor_pool{ VK_NULL_HANDLE };
	VkDescriptorPoolSize pool_size{};
	pool_size.type = type;
	pool_size.descriptorCount = static_cast<uint32_t>(max_descriptors_per_set);

	VkDescriptorPoolCreateInfo descriptor_pool_info{};
	descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	if (descriptor_sets_individually_resetable) {
		descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	}

	descriptor_pool_info.poolSizeCount = 1;
	descriptor_pool_info.pPoolSizes = &pool_size;
	descriptor_pool_info.maxSets = max_sets;

	if (vkCreateDescriptorPool(device, &descriptor_pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
		log_error("Failed to create descriptor pool with num sets ", max_sets, " and num descriptors per set ", max_descriptors_per_set);
	}

	return descriptor_pool;
}

void create_descriptor_sets(VkDevice device, VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts, std::span<VkDescriptorSet> out_descriptor_sets)
{
	if (layouts.size() != out_descriptor_sets.size()) {
		log_error("Failed to create descriptor sets, number of layouts must match number of descriptors. Num layouts = ", layouts.size(), ". Num descriptors = ", out_descriptor_sets.size());
		return;
	}

	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = pool;
	alloc_info.descriptorSetCount = static_cast<uint32_t>(out_descriptor_sets.size());
	alloc_info.pSetLayouts = layouts.data();

	vkAllocateDescriptorSets(device, &alloc_info, out_descriptor_sets.data());
}

FrameDescriptorSets create_frame_descriptor_sets(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout descriptor_set_layout, const FrameUniformBuffers& uniform_buffers)
{
	// Create descriptor sets using the layout specified for the render pipeline.

	FrameDescriptorSets frame_descriptor_sets{};
	create_descriptor_sets(device, pool, descriptor_set_layout, frame_descriptor_sets);

	// initialise the descriptors so that they point to the data needed.

	for (std::size_t i = 0; i < frame_descriptor_sets.size(); ++i) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniform_buffers[i].buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferContent);

		VkWriteDescriptorSet writer{};
		writer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writer.dstSet = frame_descriptor_sets[i];
		writer.dstBinding = 0;
		writer.dstArrayElement = 0;
		writer.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writer.descriptorCount = 1;
		writer.pBufferInfo = &bufferInfo;
		writer.pImageInfo = nullptr; // Optional
		writer.pTexelBufferView = nullptr; // Optional

		vkUpdateDescriptorSets(device, 1, &writer, 0, nullptr);
	}
	
	return frame_descriptor_sets;
}


