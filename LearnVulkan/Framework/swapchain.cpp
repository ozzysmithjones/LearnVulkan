#include <span>
#include <algorithm>
#include "swapchain.h"
#include "error.h"


static VkSurfaceFormatKHR pick_surface_format(std::span<const VkSurfaceFormatKHR> surface_formats) {
	for (const auto& format : surface_formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return format;
		}
	}

	return surface_formats.back();
}

static VkPresentModeKHR pick_present_mode(std::span<const VkPresentModeKHR> present_modes) {
	//If on mobile VK_PRESENT_MODE_FIFO is probably preffered if energy usage is a concern.
	for (const auto& present_mode : present_modes) {
		if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return present_mode;
		}
	}

	// FIFO is guaranteed to be there when using Vulkan
	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D pick_surface_extent(GLFWwindow* window, VkSurfaceCapabilitiesKHR capabilities) {
	//uint32_t MAX indiciates maximum allowed by the display
	if (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max() || capabilities.currentExtent.height == std::numeric_limits<uint32_t>::max())
	{
		//Pixels do not necesserally line up with screen coordinates.
		int pixel_width, pixel_height;
		glfwGetFramebufferSize(window, &pixel_width, &pixel_height);

		VkExtent2D extent{
			static_cast<uint32_t>(pixel_width),
			static_cast<uint32_t>(pixel_height),
		};

		extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return extent;
	}

	return capabilities.currentExtent;
}

VkSwapchainKHR create_swapchain(GLFWwindow* window, VkSurfaceKHR window_surface, VkDevice device, std::size_t graphics_family_index, std::size_t present_family_index, const SwapchainDetails& swapchain_details, SwapchainImages& out_swapchain_images)
{
	VkSurfaceFormatKHR surface_format = pick_surface_format(swapchain_details.surface_formats);
	VkPresentModeKHR present_mode = pick_present_mode(swapchain_details.surface_present_modes);
	VkExtent2D extent = pick_surface_extent(window, swapchain_details.capabilities);

	uint32_t image_count = swapchain_details.capabilities.minImageCount + 1;
	if (swapchain_details.capabilities.maxImageCount > 0 && image_count > swapchain_details.capabilities.maxImageCount) {
		image_count = swapchain_details.capabilities.maxImageCount;
	}

	//specify the format of the swap chain (surface format, present mode, extent).
	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = window_surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1; // <- this is used for stereoscopic effects
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	//Specify if multiple different queue families use the swap chain.
	std::array<uint32_t, 2> shared_families{ static_cast<uint32_t>(graphics_family_index), static_cast<uint32_t>(present_family_index) };
	if (graphics_family_index != present_family_index) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = static_cast<uint32_t>(shared_families.size());
		create_info.pQueueFamilyIndices = shared_families.data();
	}
	else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
	}

	// specify if we want the image to be rotated or flipped. Use currentTransform to just keep it as default. 
	create_info.preTransform = swapchain_details.capabilities.currentTransform;

	// specify if we should use alpha to blend this window with other windows.
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE; // If the window is partly obscured, don't paint that region.

	// When the screen is resized, the swap chain will need to be remade from scratch. 
	// Here you would need to provide a reference to the old swap chain when the resize occurs. 
	// Since this is the first swap chaim, no previous swap chain is provided here.
	create_info.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain;
	if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) != VK_SUCCESS) {
		log_error("Failed to create swap chain");
	}

	//Get swap chain images
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
	out_swapchain_images.images.resize(image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, out_swapchain_images.images.data());

	out_swapchain_images.format = surface_format.format;
	out_swapchain_images.extent = extent;

	return swapchain;
}

static std::vector<VkImageView> create_swapchain_image_views(VkDevice device, std::span<const VkImage> images, const VkFormat& image_format) {

	std::vector<VkImageView> image_views(images.size());
	for (std::size_t i = 0; i < images.size(); ++i) {

		VkImageViewCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = images[i];

		// Images in Vulkan can contain multiple layers (similar to layers in photoshop).
		// The view type specifies how the image view should interpret a region of the the image. 
		// 1D indicates that it should interpret the pixels as being in one long line.
		// 2D indicates that it should interpret the pixels as a two dimensional array.
		// 3D indicates that it should interpret the pixels as a three dimensional array.
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

		// The format is how the colours are formatted on the image. How much memory to use for each pixel. 
		create_info.format = image_format;

		// Component swizzle is an option to rebind colour outputs. For example, using red for all channels.
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// The subresource range specifies the number of layers that the image view should interpret (if interpreting layers)
		// and if we should be using any mip-mapping levels when we use the image.
		// Mip-mapping allows sampling the image at different resolutions depending upon certain conditions like how "far away" we are drawing it. 

		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &create_info, nullptr, &image_views[i]) != VK_SUCCESS) {
			log_error("Failed to create image view ", i);
		}
	}

	return image_views;
}

static std::vector<VkFramebuffer> create_framebuffers(VkDevice device, VkRenderPass render_pass, VkExtent2D extent, std::span<const VkImageView> image_views, VkImageView depth_buffer_view) {

	std::vector<VkFramebuffer> framebuffers(image_views.size());

	for (std::size_t i = 0; i < framebuffers.size(); ++i) {
		std::array<VkImageView, 2> attachments{
			image_views[i],
			depth_buffer_view,
		};

		// A frame buffer is a set of attachments, this time by direct reference (not just a description).
		// The render pass describes how these attachments are used
		// Frame buffers must be compatible with a certain render pass.
		// Frame buffers will be sent through a series of render-subpasses. 

		VkFramebufferCreateInfo framebuffer_info{};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = render_pass;
		framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebuffer_info.pAttachments = attachments.data();
		framebuffer_info.width = extent.width;
		framebuffer_info.height = extent.height;
		framebuffer_info.layers = 1;

		if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffers[i]) != VK_SUCCESS) {
			log_error("Failed to create frame buffer ", i);
		}
	}

	return framebuffers;
}

RenderTargets create_render_targets(VkDevice device, VkRenderPass render_pass, VkSwapchainKHR swapchain, const SwapchainImages& swapchain_images, VkImageView depth_buffer_view)
{
	RenderTargets render_targets;
	render_targets.image_views = create_swapchain_image_views(device, swapchain_images.images, swapchain_images.format);
	render_targets.framebuffers = create_framebuffers(device, render_pass, swapchain_images.extent, render_targets.image_views, depth_buffer_view);
	return render_targets;
}



