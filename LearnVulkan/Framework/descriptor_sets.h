#pragma once
#include <vulkan/vulkan.hpp>
#include <array>
#include "buffer.h"
#include  "constants.h"

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

FrameDescriptorSets create_frame_descriptor_sets(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout descriptor_set_layout, const FrameUniformBuffers& uniform_buffers);