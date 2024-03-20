#pragma once
#include <vulkan/vulkan.h>
#include <cstddef>
#include <span>
#include <array>
#include "constants.h"

struct SyncObjects {
	VkSemaphore image_available_semaphore{ VK_NULL_HANDLE };
	VkSemaphore render_finished_semaphore{ VK_NULL_HANDLE };
	VkFence in_flight_fence{ VK_NULL_HANDLE };
};

struct FrameExecution {
	VkCommandBuffer command_buffer;
	SyncObjects sync;
};

using FrameExecutions = std::array<FrameExecution, MAX_FRAMES_IN_FLIGHT>;

VkCommandPool create_command_pool(VkDevice device, std::size_t queue_family_index, bool buffers_individually_resetable, bool buffers_frequently_recorded = false);
void create_command_buffers(VkDevice device, VkCommandPool pool, bool is_primary, std::span<VkCommandBuffer> out_command_buffers);
FrameExecutions create_frame_executions(VkDevice device, VkCommandPool pool);