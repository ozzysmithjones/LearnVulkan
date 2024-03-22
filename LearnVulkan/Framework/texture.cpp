#include "texture.h"
#include "buffer.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "error.h"

Texture create_texture(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool command_pool, VkQueue queue, const char* file_path)
{
	Texture texture{};

	int image_width, image_height, image_channels;
	stbi_uc* pixels = stbi_load("textures/statue.jpg", &image_width, &image_height, &image_channels, STBI_rgb_alpha);
	VkDeviceSize image_size = (VkDeviceSize)(image_width * image_height * 4);
	auto [gpu_image, gpu_image_memory] = create_gpu_image(device, physical_device, command_pool, queue, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, image_width, image_height, { pixels, image_size });

	VkImageViewCreateInfo view_info{};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = gpu_image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;

	texture.image = gpu_image;
	texture.memory = gpu_image_memory;

	if (vkCreateImageView(device, &view_info, nullptr, &texture.view) != VK_SUCCESS) {
		log_error("Failed to create image view ", file_path);
	}
	
	return texture;
}

VkSampler create_sampler(VkDevice device, uint32_t max_anisotropy)
{
	VkSampler sampler{ VK_NULL_HANDLE };
	VkSamplerCreateInfo sampler_info{};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	// These two enums specify the filtering process that should be applied when sampling from the texture.
	// This filtering needs to occur because sometimes multiple texels can map to a single pixel on the screen. 
	// A texel is a pixel within an image/texture.
	// magFilter specifies what should happen when multiple texels are within the pixel region. If mag filtering is not applied, the output will look blurry.
	// minFilter specifies what should happen when multiple pixels are within a texel region. If min filtering is not applied, the output will look blocky.

	// One solution for mag filtering is anisotropic filtering, this is where different versions of the texture are shown depending upon distance and viewing angle, so that the resolution of the image
	// approximately matches the resolution on screen. The key thing with anisotropic filtering is to use different width and height values depending on the angle. Then the sampling continues by interpolating the sampled pixels in the adjusted texture. 

	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;

	// The address mode specifies what should happen at the borders of the image when used as a texture. 
	// Should the texture repeat across the model/sampling region? Or should the texture clamp at the borders? 
	// U V W is the three axis for texture sampling.

	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.maxAnisotropy = max_anisotropy;
	sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;

	// Can override the sampling behaviour to use a comparison function, disabled here.
	// This is used for percentage-closer filtering for shadows: https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;

	// Mip-mapping, type of filter that can be applied to sample the texture at different resolutions. 
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;

	if (vkCreateSampler(device, &sampler_info, nullptr, &sampler) != VK_SUCCESS) {
		log_error("Failed to create sampler");
	}

	return sampler;
}
