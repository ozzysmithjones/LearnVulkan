#include "command.h"
#include "error.h"

VkCommandPool create_command_pool(VkDevice device, std::size_t queue_family_index, bool buffers_individually_resetable, bool buffers_frequently_recorded)
{
	VkCommandPool command_pool;
	VkCommandPoolCreateInfo command_pool_info{};
	command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_info.flags = 0;

	if (buffers_individually_resetable) {
		command_pool_info.flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	}
	
	if (buffers_frequently_recorded) {
		command_pool_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	}

	command_pool_info.queueFamilyIndex = static_cast<uint32_t>(queue_family_index);

	if (vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool) != VK_SUCCESS) {
		log_error("Failed to create command pool!");
	}

	return command_pool;
}

void create_command_buffers(VkDevice device, VkCommandPool pool, bool is_primary, std::span<VkCommandBuffer> out_command_buffers)
{
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = pool;
	alloc_info.level = is_primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY; // Only primary buffers can be  used for submitting commands to GPU, but secondary buggers can be made to add commands in parallel. 
	alloc_info.commandBufferCount = out_command_buffers.size();

	if (vkAllocateCommandBuffers(device, &alloc_info, out_command_buffers.data()) != VK_SUCCESS) {
		log_error("Failed to allocate command buffer.");
	}
}

static SyncObjects create_sync_objects(VkDevice device)
{
	SyncObjects sync_objects{};

	// Sempahores are used to implement task precedence on the GPU, such as A must occur before B, which must occur before C.
	VkSemaphoreCreateInfo semaphore_info{};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Fences are used to block the CPU while we wait for tasks on the GPU to finish. 
	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(device, &semaphore_info, nullptr, &sync_objects.image_available_semaphore) != VK_SUCCESS) {
		log_error("Failed to create image available semaphore.");
	}

	if (vkCreateSemaphore(device, &semaphore_info, nullptr, &sync_objects.render_finished_semaphore) != VK_SUCCESS) {
		log_error("Failed to create render finished semaphore.");
	}

	if (vkCreateFence(device, &fence_info, nullptr, &sync_objects.in_flight_fence) != VK_SUCCESS) {
		log_error("Failed to create in flight fence.");
	}

	return sync_objects;
}


FrameExecutions create_frame_executions(VkDevice device, VkCommandPool pool)
{
	FrameExecutions frame_executions{};
	std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> command_buffers{ VK_NULL_HANDLE };
	create_command_buffers(device, pool, true, command_buffers);

	for (std::size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		frame_executions[i].command_buffer = command_buffers[i];
		frame_executions[i].sync = create_sync_objects(device);
	}

	return frame_executions;
}


