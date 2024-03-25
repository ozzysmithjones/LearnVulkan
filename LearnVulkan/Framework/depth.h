#pragma once
#include <vulkan/vulkan.h>

struct DepthBuffer {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView view;
	VkFormat format;
};

DepthBuffer create_depth_buffer(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height);