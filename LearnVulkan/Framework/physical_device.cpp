#include <span>
#include "physical_device.h"
#include "Error.h"
#include "Enum.h"

std::array<const char*, 1> required_device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

static [[nodiscard]] bool has_required_extensions(VkPhysicalDevice physical_device, std::span<const char*> required_extensions)
{
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

	for (const char* required_extension : required_extensions) {
		bool found_extension = false;
		for (const auto& extension : available_extensions) {
			if (std::strcmp(required_extension, extension.extensionName) == 0)
			{
				found_extension = true;
				break;
			}
		}

		if (!found_extension) {
			return false;
		}
	}

	return true;
}

static [[nodiscard]] bool try_get_swap_chain_details(VkSurfaceKHR window_surface, VkPhysicalDevice physical_device, SwapchainDetails& out_swap_chain_details) {

	// Get supported limits of swap chain, like max surface size and max numbner of surfaces.
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, window_surface, &out_swap_chain_details.capabilities);

	//Get list of supported surface formats for swap chain.
	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, window_surface, &format_count, nullptr);

	if (format_count > 0) {
		out_swap_chain_details.surface_formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, window_surface, &format_count, out_swap_chain_details.surface_formats.data());
	}
	else {
		return false;
	}

	// Get list of supported techniques to refresh the screen (commonly in setting this is known as vsync. )
	// IMMEDIATE -> is allowed to present at any time (Screen tearing can happen).
	// FIFO -> first in first out -> it will queue surfaces, only present during the VBLANK interval (VBLANK interval is the the time between the last visible rendered line on the frame and the start of the next line on the new frame).
	// MAILBOX -> Similar to FIFO except instead of queing images (which might incur an unresponsiveness feeling), other images in the queue are pushed out of the queue and re-used for drawing.  There's only one image in the queue. 
	// FIFO_RELAXED -> Similar to FIFO except if there is no image available to present, then the next image will be presented in IMMEDIATE MODE (immediately). This means a frame will not wait for the next for the next VBLANK if it is behind.

	// FIFO RELAXED Is a trade-off -> potentially show an image a frame earlier if the frame is late but might be some screen tearing. Good for slow applications. 
	// MAILBOX Is a trade-off -> Responsive to changes in the application, but less smooth differences between frames. 
	// FIFO prevents screen tearing, but the queueing of frames might feel unresponsive if the application is far ahead of what's occuring on the screen.  

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, window_surface, &present_mode_count, nullptr);

	if (present_mode_count > 0) {
		out_swap_chain_details.surface_present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, window_surface, &present_mode_count, out_swap_chain_details.surface_present_modes.data());
	}
	else {
		return false;
	}

	return true;
}

static [[nodiscard]] bool try_get_queue_family_details(VkSurfaceKHR window_surface, VkPhysicalDevice physical_device,  QueueFamilyIndexByFeature& out_queue_family_index_by_feature) {

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

	std::size_t found_feature_count = 0;
	std::array<bool, FEATURE_COUNT> found_features{ false };

	for (std::size_t i = 0; i < queue_families.size(); ++i) {

		//search for family (queue type) that supports graphical commands. 

		if (!found_features[FEATURE_GRAPHICS] && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

			found_features[FEATURE_GRAPHICS] = true;
			out_queue_family_index_by_feature[FEATURE_GRAPHICS] = i;
			++found_feature_count;
			if (found_feature_count >= FEATURE_COUNT) {
				break;
			}
		}


		if (!found_features[FEATURE_PRESENT])
		{
			//search for family (queue type) that specifically supports present commands.

			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, window_surface, &present_support);

			if (present_support)
			{
				found_features[FEATURE_PRESENT] = true;
				out_queue_family_index_by_feature[FEATURE_PRESENT] = i;
				++found_feature_count;
				if (found_feature_count >= FEATURE_COUNT) {
					break;
				}
			}
		}
	}

	return found_feature_count == FEATURE_COUNT;
}

static bool try_get_required_anistropy_details(VkPhysicalDevice physical_device, uint32_t& out_max_anistropy_samples) {
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(physical_device, &features);
	if (!features.samplerAnisotropy) {
		return false;
	}

	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(physical_device, &properties);
	out_max_anistropy_samples = properties.limits.maxSamplerAnisotropy;
	return true;
}

static [[nodiscard]] bool try_get_required_details(VkSurfaceKHR window_surface, VkPhysicalDevice physical_device, DeviceDetails& out_device_details) {

	if (!has_required_extensions(physical_device, required_device_extensions)) {
		return false;
	}

	if (!try_get_swap_chain_details(window_surface, physical_device, out_device_details.swapchain)) {
		return false;
	}

	if (!try_get_queue_family_details(window_surface, physical_device, out_device_details.queue_family_index_by_feature)) {
		return false;
	}

	if (!try_get_required_anistropy_details(physical_device, out_device_details.max_anistropy_samples)) {
		return false;
	}

	vkGetPhysicalDeviceProperties(physical_device, &out_device_details.properties);
	return true;
}

static std::size_t rate_details(DeviceDetails& details) {

	int32_t rating = 1;
	if (details.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		rating += 1000;
	}

	const auto& surface_formats = details.swapchain.surface_formats;
	if (std::find_if(surface_formats.begin(), surface_formats.end(), [](VkSurfaceFormatKHR format)->bool { return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && VK_FORMAT_B8G8R8A8_SRGB; }) != surface_formats.end())
	{
		//supports nice colors.
		rating += 10;
	}

	const auto& present_modes = details.swapchain.surface_present_modes;
	if (std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end()) {

		//support mailbox presenting.
		rating += 10;
	}

	return rating;
}

[[nodiscard]] VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR window_surface, DeviceDetails& out_details) {

	uint32_t device_count;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

	std::vector<VkPhysicalDevice> physical_devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());

	std::size_t highest_rating = 0;
	VkPhysicalDevice picked = VK_NULL_HANDLE;
	
	for (VkPhysicalDevice physical_device : physical_devices) {

		DeviceDetails details{};
		if (!try_get_required_details(window_surface, physical_device, details)) {
			continue;
		}

		std::size_t rating = rate_details(details);
		if (rating >= highest_rating) {
			highest_rating = rating;
			picked = physical_device;
			out_details = details;
		}
	}

	if (picked == VK_NULL_HANDLE) {
		log_error("Failed to find suitable device.");
	}

	return picked;
}