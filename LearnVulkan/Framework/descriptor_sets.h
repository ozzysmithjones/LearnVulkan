#pragma once
#include <vulkan/vulkan.hpp>
#include <array>
#include "buffer.h"
#include "constants.h"


// Steps for adding a new descriptor in Vulkan:
// 1.	Add a descriptor layout binding to the render pipeline, to inform the render pipeline about the new descriptor. This is also used as a flyweight to create descriptors from.
//		When adding a descriptor layout binding, it's important that you add it to the VKDescriptorSetLayoutCreateInfo, to add it to the descriptor set it belongs to.
//		This descriptor set layout is used in the pipeline layout, which is used to encapsulate all of the descriptor sets and the push constants for the render pipeline.
// 2.   Once the layout is created, the next step is to ensure that the descriptor pool has sufficient space to store the new descriptor set type. If this is not checked it might NOT 
//		throw and error, and the behaviour could be inconsistent between different platforms. The descriptor pool needs two things: The type of the descriptor and how many should be reserved. 
//		In the case of multiple frames, you would probably want to reserve one per frame so that one descriptor can be used in a render while another is being used elsewhere.
// 3.	Once the pool is set to a sufficient size, a descriptor set can be created using the layout created for the render pipeline. This layout will also be needed when invoking 
//		commands using the descriptor. Descriptor sets are allocated slightly differently in vulkan as they are allocated in bulk using the descriptor pool. You need:
//		VkDescriptorSetAllocateInfo to select how many to allocate and VkWriteDescriptorSet to write into them once they are created. 
//
// Descriptors are essentially pointers in Vulkan used to attach data (besides the vertices and the index buffers).  

using FrameDescriptorSets = std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>;

VkDescriptorPool create_descriptor_pool(VkDevice device, VkDescriptorType type, bool descriptor_sets_individually_resetable, std::size_t max_sets, std::size_t max_descriptors_per_set);
void create_descriptor_sets(VkDevice device, VkDescriptorPool pool, std::span<VkDescriptorSetLayout> layouts, std::span<VkDescriptorSet> out_descriptor_sets);

template<std::size_t NumDescriptors>
void create_descriptor_sets(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, std::array<VkDescriptorSet, NumDescriptors>& out_descriptor_sets)
{
	std::array<VkDescriptorSetLayout, NumDescriptors> layouts;
	layouts.fill(layout);
	create_descriptor_sets(device, pool, layouts, out_descriptor_sets);
}

FrameDescriptorSets create_frame_descriptor_sets(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout descriptor_set_layout, const FrameUniformBuffers& uniform_buffers, VkImageView texture, VkSampler texture_sampler);