#include <algorithm>
#include "device.h"
#include "error.h"

#ifdef NDEBUG
#else
extern std::array<const char*, 1> required_validation_layers;
#endif

VkDevice create_device(VkPhysicalDevice physical_device, const QueueFamilyIndexByFeature& queue_family_index_by_feature, QueueByFeature& out_queue_by_feature)
{
	std::array<std::size_t, FEATURE_COUNT> unique_indecies = queue_family_index_by_feature;
	std::sort(unique_indecies.begin(), unique_indecies.end());
	auto end = std::unique(unique_indecies.begin(), unique_indecies.end());

	// Request a single queue in each queue family index. 
	float queue_priority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queue_family_create_infos(static_cast<std::size_t>((end - unique_indecies.begin())));
	for (std::size_t i = 0; i < queue_family_create_infos.size(); ++i) {
		auto& queue_info = queue_family_create_infos[i];

		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.queueFamilyIndex = unique_indecies[i];
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &queue_priority;
		queue_info.flags = 0;
		queue_info.pNext = nullptr;
	}

	// Logical devices specify a subset of the physical device that we wish to use. 
	// Here we specify the queues that we want available from the physical device. 
	// We can also specify additional vulkan layers or features that we need for the queues to work.  
	// It used to be the case that you could specify vulkan layers for logical devices, but this feature is now no longer true in newer versions. 

	VkDeviceCreateInfo device_create_info{};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.queueCreateInfoCount = queue_family_create_infos.size();
	device_create_info.pQueueCreateInfos = queue_family_create_infos.data();
	device_create_info.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size());
	device_create_info.ppEnabledExtensionNames = required_device_extensions.data();
	device_create_info.flags = 0;
	device_create_info.pEnabledFeatures = nullptr;
	device_create_info.pNext = nullptr;


	//For now, just specify extra layers anyway for backwards combatibility.
#ifndef NDEBUG

	device_create_info.enabledLayerCount = static_cast<uint32_t>(required_validation_layers.size());
	device_create_info.ppEnabledLayerNames = required_validation_layers.data();
#else
	device_create_info.enabledLayerCount = 0;
#endif

	VkDevice device;
	if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
		log_error("Failed to create device for associated physical device");
	}

	// Get handles to queues that were created with the create device call(). 
	// Note that a queue family can have multiple queues, we are just using one queue per queue family for the moment. 
	// Each queue family supports a different set of commands.

	for (std::size_t i = 0; i < FEATURE_COUNT; ++i)
	{
		vkGetDeviceQueue(device, queue_family_index_by_feature[i], 0, &out_queue_by_feature[i]);
	}

	return device;
}
