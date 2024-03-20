#include "vulkan_instance.h"
#include <iostream>
#include <array>
#include <vector>
#include "error.h"

#ifdef NDEBUG
#else
std::array<const char*, 1> required_validation_layers = {
	"VK_LAYER_KHRONOS_validation"
};
#endif

static bool has_required_validation_layers() {

#ifndef NDEBUG
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

	std::vector<VkLayerProperties> layer_properties(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, layer_properties.data());

	for (const char* required_layer : required_validation_layers) {

		bool layer_found = false;
		for (const auto& layer_property : layer_properties) {
			if (std::strcmp(layer_property.layerName, required_layer) == 0) {
				layer_found = true;
				break;
			}
		}

		if (!layer_found) {
			return false;
		}
	}
#endif

	return true;
}

VkInstance create_vulkan_instance()
{
	VkInstance vulkan_instance;
	{ //If we wanted to see the extensions that vulkan supports beforehand we can do that here. 
		uint32_t supported_extension_count;
		vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, nullptr);

		std::vector<VkExtensionProperties> extensions(supported_extension_count);
		vkEnumerateInstanceExtensionProperties(nullptr, &supported_extension_count, extensions.data());

		std::cout << "Supported Vulkan extensions : \n";

		for (const auto& extension : extensions)
		{
			std::cout << '\t' << extension.extensionName << '\n';
		}
	}

	// Provide details of our app to Vulkan like name and engine version so it can make optimisations if our application is well known.

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pNext = nullptr;
	app_info.pApplicationName = "First Triangle in Vulkan";
	app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	// Device create info used to describe the Vulkan instance we want to create, note how each struct uses an sType parameter for when the struct is accessed by void*. 

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.pApplicationInfo = &app_info;

	// Get the Vulkan extensions needed by GLFW to interface the window with the Vulkan API.

	uint32_t glfw_extension_count = 0;
	const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	create_info.enabledExtensionCount = glfw_extension_count;
	create_info.ppEnabledExtensionNames = glfw_extensions;

	// Vulkan "layers" are functions that sit between your application and the Vulkan drivers to report errors.

#ifndef NDEBUG

	if (!has_required_validation_layers())
	{
		log_error("Requested validation layers not available!");
		return VK_NULL_HANDLE;
	}

	create_info.enabledLayerCount = static_cast<uint32_t>(required_validation_layers.size());
	create_info.ppEnabledLayerNames = required_validation_layers.data();

#else
	create_info.enabledLayerCount = 0;
#endif

	// Create the instance.

	VkResult result = vkCreateInstance(&create_info, nullptr, &vulkan_instance);

	if (result != VK_SUCCESS) {
		log_error("Failed to create instance");
	}

	return vulkan_instance;
}
