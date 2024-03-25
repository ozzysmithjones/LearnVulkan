#pragma once
#include <cstdint>
#include <tuple>
#include <span>
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "constants.h"

struct UniformBufferContent {
	glm::mat4 transform;
};
struct UniformBuffer {
	void* mapped_region{ VK_NULL_HANDLE };
	VkDeviceMemory memory{ VK_NULL_HANDLE };
	VkBuffer buffer{ VK_NULL_HANDLE };
};

using FrameUniformBuffers = std::array<UniformBuffer, MAX_FRAMES_IN_FLIGHT>;

std::tuple<VkBuffer, VkDeviceMemory> create_buffer(VkDevice device, VkPhysicalDevice physical_device, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, std::span<const uint8_t> data);

template<typename T>
constexpr std::tuple<VkBuffer, VkDeviceMemory> create_buffer(VkDevice device, VkPhysicalDevice physical_device, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, std::span<const T> data) {
	return create_buffer(device, physical_device, usage_flags, memory_flags, { (const uint8_t*)data.data(), data.size() * sizeof(T) });
}

std::tuple<VkBuffer, VkDeviceMemory> create_buffer(VkDevice device, VkPhysicalDevice physical_device, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, std::size_t size);
void submit_buffer_copy_command(VkDevice device, VkCommandPool command_pool, VkQueue command_queue, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize amount);


std::tuple<VkBuffer, VkDeviceMemory> create_gpu_buffer(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool command_pool, VkQueue command_queue, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, std::span<const uint8_t> data);

template<typename T>
constexpr std::tuple<VkBuffer, VkDeviceMemory> create_gpu_buffer(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool command_pool, VkQueue command_queue, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, std::span<const T> data) {
	return create_gpu_buffer(device, physical_device, command_pool, command_queue, usage_flags, memory_flags, { (const uint8_t*)data.data(), data.size() * sizeof(T) });
}

FrameUniformBuffers create_frame_uniform_buffers(VkDevice device, VkPhysicalDevice physical_device);
std::tuple<VkImage, VkDeviceMemory> create_image(VkDevice device, VkPhysicalDevice physical_device, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, VkFormat format, VkImageTiling tiling, uint32_t width, uint32_t height, std::span<const uint8_t> data);
std::tuple<VkImage, VkDeviceMemory> create_image(VkDevice device, VkPhysicalDevice physical_device, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, VkFormat format, VkImageTiling tiling, uint32_t width, uint32_t height);
std::tuple<VkImage, VkDeviceMemory> create_gpu_image(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool command_pool, VkQueue command_queue, VkFormat format, VkImageTiling tiling, uint32_t width, uint32_t height, std::span<const uint8_t> data);

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat interpret_format, VkImageAspectFlags interpret_aspect);