#pragma once
#include <vector>
#include <array>
#include <vulkan/vulkan.h>

enum {
	FEATURE_GRAPHICS,
	FEATURE_PRESENT,
	FEATURE_COUNT,
};

extern std::array<const char*, 1> required_device_extensions;
using QueueFamilyIndexByFeature = std::array<std::size_t, FEATURE_COUNT>;

struct SwapchainDetails {
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> surface_formats{};
	std::vector<VkPresentModeKHR> surface_present_modes{};
};

struct DeviceDetails {
	VkPhysicalDeviceProperties properties{};
	SwapchainDetails swapchain{};
	QueueFamilyIndexByFeature queue_family_index_by_feature{};
};

[[nodiscard]] VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR window_surface, DeviceDetails& out_details);
