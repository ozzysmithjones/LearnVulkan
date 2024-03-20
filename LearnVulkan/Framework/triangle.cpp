//#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//#define GLM_FORCE_LEFT_HANDED
//#define GLFW_INCLUDE_VULKAN
#include <glm/glm.hpp>
#include <glfw/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include <vulkan/vulkan.h>
#include "window.h"
#include "vulkan_instance.h"
#include "physical_device.h"
#include "device.h"
#include "swapchain.h"
#include "shader.h"
#include "render_pipeline.h"
#include "command.h"
#include "error.h"
#include "buffer.h"
#include "descriptor_sets.h"

/*
static const std::vector<Vertex> vertices = {
	{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
	{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};
*/


static const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

static const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};


void update(UniformBuffer& uniform_buffer, VkExtent2D swapchain_extent) {

	UniformBufferContent uniform_buffer_content{};
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
	uniform_buffer_content.transform =
		glm::perspective(glm::radians(45.0f), swapchain_extent.width / (float)swapchain_extent.height, 0.1f, 10.0f) *
		glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
		glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	memcpy(uniform_buffer.mapped_region, &uniform_buffer_content, sizeof(UniformBufferContent));
}



void record_render_commands(VkPipeline render_pipeline, VkRenderPass render_pass, VkFramebuffer frame_buffer, VkExtent2D swapchain_extent, VkDescriptorSet descriptor_set, VkPipelineLayout pipeline_layout, VkBuffer vertex_buffer, VkBuffer index_buffer, std::size_t index_count, VkCommandBuffer command_buffer) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional <- possible flags include: VK_COMMAND_BUFFER_USAGE_ONETIME_SUBMIT_BIT <- if the buffer only needs to be submitted once (maybe for some initial GPU set up). VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT <- this buffer is a secondary buffer that will be used within a single render pass. VK_COMMAND_BUFFER_USAGE_SIMULATANEOUS_USE_BIT <- can be submitted again while still pending execution.
	beginInfo.pInheritanceInfo = nullptr; // Optional <- specifies to inherit state from primary command buffer if it is a secondary command buffer.

	// This call will also implicitly reset the buffer is the reset flag is set
	if (vkBeginCommandBuffer(command_buffer, &beginInfo) != VK_SUCCESS) {
		log_error("Failed to start recording command buffer.");
	}

	// Drawing commands:

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass;
	render_pass_info.framebuffer = frame_buffer;

	// defines where the shader load/stores can take place.
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = swapchain_extent;

	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clearColor;

	// Begin render pass with framebuffer data specified in render pass info, along with the specified load and store OPs. 
	// The third parameter is to notify if we are executing all of the rendering commands from the primary command buffer or if we are using secondary command buffers too.
	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the render pipeline info we provided (shaders, configuration for fixed functions).
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline);

	// We specified that the following values must be provided at run-time during draw calls to support resizing the window, so these are provided here.
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapchain_extent.width);
	viewport.height = static_cast<float>(swapchain_extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchain_extent;
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	VkBuffer vertexBuffers[] = { vertex_buffer };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, vertexBuffers, offsets);


	vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT16);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
	vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(index_count), 1, 0, 0, 0);

	//vkCmdDraw(command_buffer, vertices.size(), 1, 0, 0);

	vkCmdEndRenderPass(command_buffer);

	if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
		log_error("Failed to record commands to command buffer.");
	}
}

int main() {

	DeviceDetails device_details{};
	QueueByFeature queue_by_feature{};
	SwapchainImages swapchain_images{};

	GLFWwindow* window = create_window(600, 400, "Hello triangle");
	VkInstance instance = create_vulkan_instance();
	VkSurfaceKHR window_surface = create_window_surface(instance, window);
	VkPhysicalDevice physical_device = pick_physical_device(instance, window_surface, device_details);
	VkDevice device = create_device(physical_device, device_details.queue_family_index_by_feature, queue_by_feature);
	VkSwapchainKHR swapchain = create_swapchain(window, window_surface, device, device_details.queue_family_index_by_feature[FEATURE_GRAPHICS], device_details.queue_family_index_by_feature[FEATURE_PRESENT], device_details.swapchain, swapchain_images);
	VkRenderPass render_pass = create_render_pass(device, swapchain_images.format);
	RenderTargets render_targets = create_render_targets(device, render_pass, swapchain, swapchain_images);
	ShaderByStage shader_by_stage = create_shaders(device, "vert.spv", "frag.spv");
	PipelineResources pipeline_resources = create_pipeline_resources(device);
	VkPipeline pipeline = create_render_pipeline(device, render_pass, pipeline_resources.pipeline_layout, std::move(shader_by_stage), swapchain_images.extent);
	VkCommandPool command_pool = create_command_pool(device, device_details.queue_family_index_by_feature[FEATURE_GRAPHICS], true, false);

	VkDescriptorPool descriptor_pool = create_descriptor_pool(device, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, false, MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT);
	//Multiple frames can be queued up while we wait asynchronously for the GPU to do the render commands. 
	FrameExecutions frame_executions = create_frame_executions(device, command_pool);
	FrameUniformBuffers frame_uniform_buffers = create_frame_uniform_buffers(device, physical_device);
	FrameDescriptorSets frame_descriptor_sets = create_frame_descriptor_sets(device, descriptor_pool, pipeline_resources.descriptor_set_layout, frame_uniform_buffers);

	auto [gpu_vertex_buffer, gpu_vertex_memory] = create_gpu_buffer<Vertex>(device, physical_device, command_pool, queue_by_feature[FEATURE_GRAPHICS], VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 0, vertices);
	auto [gpu_index_buffer, gpu_index_memory] = create_gpu_buffer<uint16_t>(device, physical_device, command_pool, queue_by_feature[FEATURE_GRAPHICS], VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 0, indices);


	std::size_t current_executing_frame = 0;
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		update(frame_uniform_buffers[current_executing_frame], swapchain_images.extent);

		SyncObjects& sync_objects = frame_executions[current_executing_frame].sync;
		VkCommandBuffer command_buffer = frame_executions[current_executing_frame].command_buffer;

		vkWaitForFences(device, 1, &sync_objects.in_flight_fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &sync_objects.in_flight_fence);

		uint32_t image_index;
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, sync_objects.image_available_semaphore, VK_NULL_HANDLE, &image_index);

		vkResetCommandBuffer(command_buffer, 0);
		record_render_commands(pipeline, render_pass, render_targets.framebuffers[static_cast<std::size_t>(image_index)], swapchain_images.extent, frame_descriptor_sets[current_executing_frame], pipeline_resources.pipeline_layout, gpu_vertex_buffer, gpu_index_buffer, indices.size(), command_buffer);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Wait until the image is available before submitting rendering commands. 

		VkSemaphore wait_semaphores[] = { sync_objects.image_available_semaphore };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;

		VkSemaphore signal_semaphores[] = { sync_objects.render_finished_semaphore };
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		if (vkQueueSubmit(queue_by_feature[FEATURE_GRAPHICS], 1, &submit_info, sync_objects.in_flight_fence) != VK_SUCCESS) {
			log_error("Failed to submit queue for rendering");
		}

		// Wait for rendering to finish before submitting present command.
		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;

		VkSwapchainKHR swapChains[] = { swapchain };
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapChains;
		present_info.pImageIndices = &image_index;
		present_info.pResults = nullptr; // Optional array of result values if using an array of swap chains.

		vkQueuePresentKHR(queue_by_feature[FEATURE_PRESENT], &present_info);
		++current_executing_frame;
		if (current_executing_frame >= MAX_FRAMES_IN_FLIGHT) {
			current_executing_frame = 0;
		}
	}

	vkDeviceWaitIdle(device);
	vkFreeMemory(device, gpu_index_memory, nullptr);
	vkDestroyBuffer(device, gpu_index_buffer, nullptr);
	vkFreeMemory(device, gpu_vertex_memory, nullptr);
	vkDestroyBuffer(device,gpu_vertex_buffer, nullptr);

	vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

	for (auto& frame_uniform_buffer : frame_uniform_buffers) {
		vkDestroyBuffer(device, frame_uniform_buffer.buffer, nullptr);
		vkFreeMemory(device, frame_uniform_buffer.memory, nullptr);
	}

	for (auto& frame_execution : frame_executions) {
		auto& sync_objects = frame_execution.sync;
		vkDestroySemaphore(device, sync_objects.image_available_semaphore, nullptr);
		vkDestroySemaphore(device, sync_objects.render_finished_semaphore, nullptr);
		vkDestroyFence(device, sync_objects.in_flight_fence, nullptr);
	}
	
	vkDestroyCommandPool(device, command_pool, nullptr);

	for (const auto& image_view: render_targets.image_views) {
		vkDestroyImageView(device, image_view, nullptr);
	}

	for (const auto& framebuffer : render_targets.framebuffers) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
	vkDestroyRenderPass(device, render_pass, nullptr);
	vkDestroyPipelineLayout(device, pipeline_resources.pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(device, pipeline_resources.descriptor_set_layout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyDevice(device, nullptr);

	vkDestroySurfaceKHR(instance, window_surface, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();

	vkDestroyInstance(instance, nullptr);
	return 0;
}