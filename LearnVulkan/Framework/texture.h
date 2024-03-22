#pragma once
#include <vulkan/vulkan.h>
#include <tuple>
#include "constants.h"

struct Texture {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
};

VkSampler create_sampler(VkDevice device, uint32_t max_anisotropy);
Texture create_texture(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool command_pool, VkQueue queue, const char* file_path);