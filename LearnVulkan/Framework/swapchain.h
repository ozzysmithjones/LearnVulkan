#pragma once
#include <vulkan/vulkan.h>
//#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include "physical_device.h"

struct SwapchainImages {
	VkFormat format{};
	VkExtent2D extent{};
	std::vector<VkImage> images{};
};

struct RenderTargets {
	std::vector<VkImageView> image_views{};
	std::vector<VkFramebuffer> framebuffers{};
};

VkSwapchainKHR create_swapchain(GLFWwindow* window, VkSurfaceKHR window_surface, VkDevice device, std::size_t graphics_family_index, std::size_t present_family_index, const SwapchainDetails& swapchain_details, SwapchainImages& out_swapchain_images);
RenderTargets create_render_targets(VkDevice device, VkRenderPass render_pass, VkSwapchainKHR swapchain, const SwapchainImages& swapchain_images, VkImageView depth_buffer_view);

