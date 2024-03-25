#include <array>
#include <span>
#include <optional>
#include "depth.h"
#include "error.h"
#include "buffer.h"


static std::optional<VkFormat> find_supported_format(VkPhysicalDevice physical_device,std::span<VkFormat> formats, VkImageTiling tiling, VkFormatFeatureFlags feature_flags) {
	for (VkFormat format : formats) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & feature_flags) == feature_flags) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & feature_flags) == feature_flags) {
			return format;
		}
	}

	log_error("Failed to find suitable format");
	return std::nullopt;
}

static bool has_stencil_component(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

DepthBuffer create_depth_buffer(VkDevice device, VkPhysicalDevice physical_device, uint32_t width, uint32_t height)
{
	DepthBuffer depth_buffer{};
	std::array<VkFormat, 3> formats{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	depth_buffer.format = find_supported_format(physical_device, formats, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT).value();

	auto [image, memory] = create_image(device, physical_device, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_buffer.format, VK_IMAGE_TILING_OPTIMAL, width, height);
	depth_buffer.image = image;
	depth_buffer.memory = memory;
	depth_buffer.view = create_image_view(device, depth_buffer.image, depth_buffer.format, VK_IMAGE_ASPECT_DEPTH_BIT);

	// Note that we don't transition the actual layout of the underlying image here, just the way it is interpreted from the image view. 
	// The layout of the image can be transitioned to depth-stencil attachment optimal from the render pass. 
	// Layout can also be specified by issuing commands directly to the GPU.

	return depth_buffer;
}
