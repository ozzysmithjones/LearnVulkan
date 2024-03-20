
#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <sstream>
#include <stacktrace>
#include <vector>
#include <array>
#include <optional>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <span>

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <vulkan/vulkan.h>

static constexpr std::size_t WINDOW_WIDTH = 800;
static constexpr std::size_t WINDOW_HEIGHT = 600;

#ifndef NDEBUG
template<typename ... Ts>
void log_error(Ts... error_messages)
{

	std::stringstream ss;
	((ss << error_messages), ...);

	std::string error = ss.str();
	std::cerr << error << "\nstack trace:\n" << std::stacktrace::current() << "\n\n";

#ifdef _MSC_VER
	__debugbreak();
#endif

}

#else
#define log_error(...)
#endif

#ifdef NDEBUG
static constexpr bool is_validation_layers = false;
#else
static constexpr bool is_validation_layers = true;
constexpr std::array<const char*, 1> required_validation_layers = {
	"VK_LAYER_KHRONOS_validation"
};
#endif


std::array<const char*, 1> req_device_extensions{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

struct Swap_Chain_Support_Details {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> surface_formats;
	std::vector<VkPresentModeKHR> surface_present_modes;
};

enum Family_Type {
	FAMILY_GRAPHICS,
	FAMILY_PRESENT,
	FAMILY_COUNT,
};

enum Shader_Stages {
	SHADER_STAGE_VERTEX,
	SHADER_STAGE_FRAG,
	SHADER_STAGE_COUNT,
};

using Queue_Family_Indices = std::array<std::size_t, FAMILY_COUNT>;


struct Device_Requirement_Details {
	Swap_Chain_Support_Details swap_chain_support;
	Queue_Family_Indices queue_families;
};


std::vector<char> read_file(const char* file_path) {
	std::ifstream file(file_path, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		log_error("Failed to open file ", file_path);
		return {};
	}

	std::size_t file_size = file.tellg();
	std::vector<char> buffer(file_size);
	file.seekg(0);
	file.read(buffer.data(), file_size);
	return buffer;
}



class App {

public:

	App() {
		init_window();
		init_vulkan_instance();
		create_surface();
		pick_physical_device();
		create_logical_device();
		create_swap_chain();
		create_swap_chain_image_views();
		create_render_pass();
		create_graphics_pipeline();
		create_frame_buffers();
		create_command_pool();
		create_command_buffer();
		create_sync_objects();
	}

	~App() {
		vkDestroySemaphore(device, image_available_semaphore, nullptr);
		vkDestroySemaphore(device, render_finished_semaphore, nullptr);
		vkDestroyFence(device, in_flight_fence, nullptr);
		vkDestroyCommandPool(device, command_pool, nullptr);
		for (auto framebuffer : swap_chain_framebuffers) {
			vkDestroyFramebuffer(device,framebuffer, nullptr);
		}
		
		vkDestroyPipeline(device, graphics_pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
		vkDestroyRenderPass(device, render_pass, nullptr);

		for (auto image_view : swap_chain_image_views) {
			vkDestroyImageView(device, image_view, nullptr);
		}
		vkDestroySwapchainKHR(device, swap_chain, nullptr);
		vkDestroySurfaceKHR(vulkan_instance, window_surface, nullptr);
		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(vulkan_instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	bool should_close() {
		return glfwWindowShouldClose(window);
	}

	void update() {
		glfwPollEvents();
		draw_frame();
	}

private:

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

	void init_vulkan_instance() {

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
			return;
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
	}

	static Swap_Chain_Support_Details query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR window_surface) {

		// The swap chain is a series of images. One image is drawn to while another is shown to the screen.
		// We need to get additional details about the swap chain to ensure that it is compatible with the window that we have created. 
		Swap_Chain_Support_Details support_details;

		// Get supported limits of swap chain, like max surface size and max numbner of surfaces.
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, window_surface, &support_details.capabilities);

		//Get list of supported surface formats for swap chain.
		uint32_t format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, window_surface, &format_count, nullptr);

		if (format_count > 0) {
			support_details.surface_formats.resize(format_count);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, window_surface, &format_count, support_details.surface_formats.data());
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
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, window_surface, &present_mode_count, nullptr);

		if (present_mode_count != 0) {
			support_details.surface_present_modes.resize(present_mode_count);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, window_surface, &present_mode_count, support_details.surface_present_modes.data());
		}

		return support_details;
	}

	//Rating of 0 indicates not suitable.
	int32_t rate_physical_device(VkPhysicalDevice device, Device_Requirement_Details& requirement_details) {

		int32_t rating = 1;

		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(device, &device_properties);

		if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			rating += 1000;
		}

		const auto& surface_formats = requirement_details.swap_chain_support.surface_formats;
		if (std::find_if(surface_formats.begin(), surface_formats.end(), [](VkSurfaceFormatKHR format)->bool { return format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR && VK_FORMAT_B8G8R8A8_SRGB; }) != surface_formats.end())
		{
			//supports nice colors.
			rating += 10; 
		}

		const auto& present_modes = requirement_details.swap_chain_support.surface_present_modes;
		if (std::find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != present_modes.end()) {

			//support mailbox presenting.
			rating += 10;
		}

		return rating; //rating;
	}

	Queue_Family_Indices find_queue_families(VkPhysicalDevice device) {
		Queue_Family_Indices indices;
		for (auto& index : indices) {
			index = 0;
		}

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

		for (std::size_t i = 0; i < queue_families.size(); ++i) {

			//search for family (queue type) that supports graphical commands. 

			if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

				indices[FAMILY_GRAPHICS] = i;
			}

			//search for family (qeue type) that specifically supports present commands.

			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, window_surface, &present_support);

			if (present_support)
			{
				indices[FAMILY_PRESENT] = i;
			}
		}

		return indices;
	}

	static std::optional<Device_Requirement_Details> physical_device_meets_requirements(VkPhysicalDevice device, VkSurfaceKHR window_surface) {

		Device_Requirement_Details requirement_details;

		// Check to ensure that the device supports swap chain and any other extensions we need. 

		uint32_t extension_count;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

		std::vector<VkExtensionProperties> available_extensions(extension_count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

		for (const char* required_extension : req_device_extensions) {
			bool found_extension = false;
			for (const auto& extension : available_extensions) {
				if (std::strcmp(required_extension, extension.extensionName) == 0)
				{
					found_extension = true;
					break;
				}
			}

			if (!found_extension) {
				return std::nullopt;
			}
		}

		// Check to ensure that the device has formats on the swap chain.

		requirement_details.swap_chain_support = query_swap_chain_support(device, window_surface);
		if (requirement_details.swap_chain_support.surface_formats.empty() || requirement_details.swap_chain_support.surface_present_modes.empty()) {
			return std::nullopt;
		}

		// Check to ensure that the device has the queue families that we need. 

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

		std::size_t family_found_count = 0;
		std::array<bool, FAMILY_COUNT> found_families{ false };

		for (std::size_t i = 0; i < queue_families.size(); ++i) {

			//search for family (queue type) that supports graphical commands. 

			if (!found_families[FAMILY_GRAPHICS] && queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

				found_families[FAMILY_GRAPHICS] = true;
				requirement_details.queue_families[FAMILY_GRAPHICS] = i;
				++family_found_count;
				if (family_found_count >= FAMILY_COUNT) {
					break;
				}
			}


			if (!found_families[FAMILY_PRESENT])
			{
				//search for family (queue type) that specifically supports present commands.

				VkBool32 present_support = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, window_surface, &present_support);

				if (present_support)
				{
					found_families[FAMILY_PRESENT] = true;
					requirement_details.queue_families[FAMILY_PRESENT] = i;
					++family_found_count;
					if (family_found_count >= FAMILY_COUNT) {
						break;
					}
				}
			}
		}

		if (family_found_count != FAMILY_COUNT)
		{
			return std::nullopt;
		}

		return requirement_details;
	}

	void pick_physical_device() {

		uint32_t device_count;
		vkEnumeratePhysicalDevices(vulkan_instance, &device_count, nullptr);

		std::vector<VkPhysicalDevice> physical_devices(device_count);
		vkEnumeratePhysicalDevices(vulkan_instance, &device_count, physical_devices.data());

		int32_t highest_rating = 0;
		for (const auto& physical_device : physical_devices)
		{
			std::optional<Device_Requirement_Details> requirement_details = physical_device_meets_requirements(physical_device, window_surface);
			if (!requirement_details.has_value()) {
				continue;
			}

			int32_t rating = 0;
			rating = rate_physical_device(physical_device, requirement_details.value());
			if (rating > highest_rating)
			{
				highest_rating = rating;
				this->physical_device = physical_device;
				this->device_details = requirement_details.value();
			}
		}

		if (highest_rating == 0) {
			log_error("No suitable physical device found!\n");
		}
		else {
			std::cout << "found physical device!\n";
		}
	}

	static VkSurfaceFormatKHR select_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) {
		for (const auto& format : available_formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return format;
			}
		}

		return available_formats.back();
	}

	static VkPresentModeKHR select_present_mode(const std::vector<VkPresentModeKHR>& available_modes) {

		//If on mobile VK_PRESENT_MODE_FIFO is probably preffered if energy usage is a concern.
		for (const auto& present_mode : available_modes) {
			if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return present_mode;
			}
		}

		// FIFO is guaranteed to be there when using Vulkan
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	static VkExtent2D select_surface_extent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {

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

	void create_logical_device() {

		// Remove duplicate indecies

		std::array<std::size_t, FAMILY_COUNT> unique_indecies = device_details.queue_families;
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
		device_create_info.enabledExtensionCount = static_cast<uint32_t>(req_device_extensions.size());
		device_create_info.ppEnabledExtensionNames = req_device_extensions.data();
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

		if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
			log_error("Failed to create device for associated physical device");
		}

		//get handles to queues that were created with the create device call(). 
		//Note that a queue family can have multiple queues, we are just using one queue per queue family for the moment. 
		// Each queue family supports a different set of commands.

		for (std::size_t i = 0; i < FAMILY_COUNT; ++i)
		{
			vkGetDeviceQueue(device, device_details.queue_families[i], 0, &queue_per_family[i]);
		}
	}


	void create_swap_chain() {

		const auto& swap_chain_support = device_details.swap_chain_support;
		VkSurfaceFormatKHR surface_format = select_surface_format(swap_chain_support.surface_formats);
		VkPresentModeKHR present_mode = select_present_mode(swap_chain_support.surface_present_modes);
		VkExtent2D extent = select_surface_extent(swap_chain_support.capabilities, window);

		uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
		if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount) {
			image_count = swap_chain_support.capabilities.maxImageCount;
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
		std::array<uint32_t, 2> shared_families{ device_details.queue_families[FAMILY_GRAPHICS], device_details.queue_families[FAMILY_PRESENT] };
		if (device_details.queue_families[FAMILY_GRAPHICS] != device_details.queue_families[FAMILY_PRESENT]) {
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create_info.queueFamilyIndexCount = shared_families.size();
			create_info.pQueueFamilyIndices = shared_families.data();
		}
		else {
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			create_info.queueFamilyIndexCount = 0;
			create_info.pQueueFamilyIndices = nullptr;
		}

		// specify if we want the image to be rotated or flipped. Use currentTransform to just keep it as default. 
		create_info.preTransform = swap_chain_support.capabilities.currentTransform;

		// specify if we should use alpha to blend this window with other windows.
		create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		create_info.presentMode = present_mode;
		create_info.clipped = VK_TRUE; // If the window is partly obscured, don't paint that region.

		// When the screen is resized, the swap chain will need to be remade from scratch. 
		// Here you would need to provide a reference to the old swap chain when the resize occurs. 
		// Since this is the first swap chaim, no previous swap chain is provided here.
		create_info.oldSwapchain = VK_NULL_HANDLE; 
		if (vkCreateSwapchainKHR(device, &create_info, nullptr, &swap_chain) != VK_SUCCESS) {
			log_error("Failed to create swap chain");
		}


		//Get swap chain images
		vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr);
		swap_chain_images.resize(image_count);
		vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data());

		swap_chain_image_format = surface_format.format;
		swap_chain_extent = extent;
	}

	void create_swap_chain_image_views() {

		swap_chain_image_views.resize(swap_chain_images.size());
		for (std::size_t i = 0; i < swap_chain_image_views.size(); ++i) {

			VkImageViewCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			create_info.image = swap_chain_images[i];

			// Images in Vulkan can contain multiple layers (similar to layers in photoshop).
			// The view type specifies how the image view should interpret a region of the the image. 
			// 1D indicates that it should interpret the pixels as being in one long line.
			// 2D indicates that it should interpret the pixels as a two dimensional array.
			// 3D indicates that it should interpret the pixels as a three dimensional array.
			create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

			// The format is how the colours are formatted on the image. How much memory to use for each pixel. 
			create_info.format = swap_chain_image_format;

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

			if (vkCreateImageView(device, &create_info, nullptr, &swap_chain_image_views[i]) != VK_SUCCESS) {
				log_error("Failed to create image view ", i);
			}
		}
	}

	VkShaderModule create_shader_module(std::span<char> code) {
		VkShaderModuleCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());
		create_info.codeSize = code.size();

		VkShaderModule shader_module;
		if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
			log_error("Failed to create shader module");
		}

		return shader_module;
	}

	void create_surface() {
		if (glfwCreateWindowSurface(vulkan_instance, window, nullptr, &window_surface) != VK_SUCCESS) {
			log_error("Failed to create window surface");
		}
	}


	void create_render_pass() {


		// An attachment is a description of a resource used during rendering. 
		// Vulkan organises rendering into discreet steps called sub-passes. A subpass is a sequence of steps that read from or edit a set of images, using the attachments that describe them.
		// For example, A full rendering pipeline is an example of a subpass. Multiple Subpasses can be chained together to form a render pass with multiple stages -> i.e first render the scene, then apply shadows and lighting.
		// In this case we just want a single sub-pass to render a triangle with no additional post-processing effects. 
		// We need an attachment to detail the resource that we want to output from the rendering pipeline. This attachment is directly referenced by index in the fragment shader.
		VkAttachmentDescription color_attachment{};
		color_attachment.format = swap_chain_image_format;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		// depth is treated seperately from color just in case we don't want to keep it after render pass.
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		//here we inform vulkan that the layout of the bimage  should be optimised for presenting to the screen after we are done with the render subpass. 
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


		// An attachment reference represents a reference to an attachment at a specific render sub-pass. 
		// The attachment might be in an initial format but it can go under many transformations as it goes through the rendering process.
		// In this case, we specify that when the attachment is referenced in the subpass it should be converted to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL format
		// This is to make writing color more optimal.
		VkAttachmentReference color_attachment_ref{};
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// Here we create the subpass, we must specifically notify Vulkan that it is a graphics subpass to use the render pipeline state. Vulkan also supports compute subpasses. 
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;

		// specifies a dependency between two subpasses. 
		// Wait until the color attachment stage before we can start the main render sub-pass for the triangle.
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;
		render_pass_info.dependencyCount = 1;
		render_pass_info.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass) != VK_SUCCESS) {
			log_error("Failed to create render pass");
		}
	} 

	void create_graphics_pipeline() {
		auto vertex_shader_code = read_file("vert.spv");
		auto frag_shader_code = read_file("frag.spv");

		VkShaderModule vert_shader_module = create_shader_module(vertex_shader_code);
		VkShaderModule frag_shader_module = create_shader_module(frag_shader_code);

		// Create shader pipeline stages:

		std::array<VkPipelineShaderStageCreateInfo, SHADER_STAGE_COUNT> stage_create_infos{};
		for (auto& stage_create_info : stage_create_infos) {
			stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage_create_info.pName = "main";
		}

		stage_create_infos[SHADER_STAGE_VERTEX].module = vert_shader_module;
		stage_create_infos[SHADER_STAGE_VERTEX].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_create_infos[SHADER_STAGE_FRAG].module = frag_shader_module;
		stage_create_infos[SHADER_STAGE_FRAG].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		// The input assembly describes how the vertex data is assembled into primitive shapes.
		// Triangle list inidicates that every three vertices is used to make a triangle.
		// You can use other assembly types to recycle vertices more efficiently. 

		VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
		input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		// This bool indicates if it is possible to prevent reclying of vertices when not using list-type topology.
		// Should it be possible to restart a primitive halfway through? 
		input_assembly_info.primitiveRestartEnable = VK_FALSE;

		// Describes the format of the vertex data that will be passed to the vertex shader.
		// Uses arrays of structs to describe the stride and format of the vertices. 

		VkPipelineVertexInputStateCreateInfo vertex_input_info{};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexAttributeDescriptionCount = 0;
		vertex_input_info.pVertexAttributeDescriptions = nullptr;
		vertex_input_info.vertexBindingDescriptionCount = 0;
		vertex_input_info.pVertexBindingDescriptions = nullptr;

		// The view port describes the portion of the framebuffer that will be rendered to.
		// The content is NOT cropped, it is instead stretched to the fill area.

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swap_chain_extent.width;
		viewport.height = (float)swap_chain_extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// If we want to crop the image, the scissor option can be used.

		VkRect2D scissor{};
		scissor.offset = { 0,0 };
		scissor.extent = swap_chain_extent;

		//^^ It's very common to use dynamic states to configure the size and cropping of the output dynamically at runtime. 
		// Dynamic states must be set during draw-calls.

		std::array<VkDynamicState, 2> dynamic_states = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamic_state_info{};
		dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
		dynamic_state_info.pDynamicStates = dynamic_states.data();

		VkPipelineViewportStateCreateInfo viewport_state_info{};
		viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state_info.viewportCount = 1;
		viewport_state_info.pViewports = &viewport;
		viewport_state_info.scissorCount = 1;
		viewport_state_info.pScissors = &scissor;

		// Configure the rasterisation step to draw lines, fill or dots.
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

		//If depth clamp enable is set to true, then fragments outside of view are clamped to be within view instead. (why idk)
		rasterizer.depthClampEnable = VK_FALSE;

		//this bool can set to disable rasterisation (if we just want to run a vertex shader without any output).
		rasterizer.rasterizerDiscardEnable = VK_FALSE;

		// This mode mode can be either FILL, LINE or POINT. If we just want to draw the lines or the vertices of the mesh, this can be modified. 
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

		//NOTE: you need to enable the wide lines GPU feature if lines thicker than 1 are needed. 
		rasterizer.lineWidth = 1.0f;

		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

		//Depth values can be biased based on fragments slope, this is sometimes used for shadow mapping.
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;
		rasterizer.depthBiasClamp = 0.0f; 
		rasterizer.depthBiasSlopeFactor = 0.0f; 

		// Multisampling is a technique used to provide an anti-aliasing effect on geometry edges.
		// Disabled for now.
		// Revisit sampling at a later time.
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional


		// Describes how fragments that map to the same pixel will be blended together.
		// This is commonly used for transparency effects. 
		// This structure is needed per frame buffer.
		VkPipelineColorBlendAttachmentState color_blend_attachment{};
		color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		color_blend_attachment.blendEnable = VK_FALSE;
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional


		// Color blend state create info contains an array of color blend attachments, one per frame buffer, to describe the different blending methods for each one. 
		// Custom global blend operation can be specified by setting the logicOpEnable flag and providing one on the logic Op.
		VkPipelineColorBlendStateCreateInfo color_blend_info{};
		color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_info.logicOpEnable = VK_FALSE;
		color_blend_info.logicOp = VK_LOGIC_OP_COPY; // Optional
		color_blend_info.attachmentCount = 1;
		color_blend_info.pAttachments = &color_blend_attachment;
		color_blend_info.blendConstants[0] = 0.0f; // Optional
		color_blend_info.blendConstants[1] = 0.0f; // Optional
		color_blend_info.blendConstants[2] = 0.0f; // Optional
		color_blend_info.blendConstants[3] = 0.0f; // Optional

		VkPipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = 0; // Optional
		pipeline_layout_info.pSetLayouts = nullptr; // Optional
		pipeline_layout_info.pushConstantRangeCount = 0; // Optional
		pipeline_layout_info.pPushConstantRanges = nullptr; // Optional

		if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
			log_error("Failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = stage_create_infos.size();
		pipeline_info.pStages = stage_create_infos.data();
		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly_info;
		pipeline_info.pViewportState = &viewport_state_info;
		pipeline_info.pRasterizationState = &rasterizer;
		pipeline_info.pMultisampleState = &multisampling;
		pipeline_info.pDepthStencilState = nullptr; // Optional
		pipeline_info.pColorBlendState = &color_blend_info;
		pipeline_info.pDynamicState = &dynamic_state_info;
		pipeline_info.layout = pipeline_layout;
		pipeline_info.renderPass = render_pass; // parent render pass.
		pipeline_info.subpass = 0; //. subpass where this render pipeline will be used.

		// This allows to derive from existing pipeline data, disabled here.
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional 
		pipeline_info.basePipelineIndex = -1; // Optional

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
			log_error("Failed to create graphics pipeline!");
		}

		vkDestroyShaderModule(device, vert_shader_module, nullptr);
		vkDestroyShaderModule(device, frag_shader_module, nullptr);

	}


	void create_frame_buffers() {
		swap_chain_framebuffers.resize(swap_chain_image_views.size());

		for (std::size_t i = 0; i < swap_chain_image_views.size(); ++i) {
			std::array<VkImageView, 1> attachments{
				swap_chain_image_views[i]
			};

			// A frame buffer is a set of attachments, this time by direct reference (not just a description).
			// Frame buffers must be compatible with a certain render pass.
			// Frame buffers will be sent through a series of render-subpasses. 

			VkFramebufferCreateInfo framebuffer_info{};
			framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebuffer_info.renderPass = render_pass;
			framebuffer_info.attachmentCount = attachments.size();
			framebuffer_info.pAttachments = attachments.data();
			framebuffer_info.width = swap_chain_extent.width;
			framebuffer_info.height = swap_chain_extent.height;
			framebuffer_info.layers = 1; 

			if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, &swap_chain_framebuffers[i]) != VK_SUCCESS) {
				log_error("Failed to create frame buffer ", i);
			}
		}
	}

	void create_command_pool() {

		// Memory pool for creating commands.

		// Vulkan sends the commands to the GPU in batches using CommandQueues.
		// A Command queue contains an array of commands with references to data needed to execute the commands. 
		// Command queues can be created from a command pool, which can be configured to allocate the queues differently.
		// VK_COMMAND_POOL_CREATE_RESET_BUFFER_BIT allows the buffers to be reset individually.
		// There's also VK_COMMAND_POOL_CREATE_TRANSIENT, which is a hint that the buffer will be edited frequently (change allocation behaviour). 

		VkCommandPoolCreateInfo command_pool_info{};
		command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		command_pool_info.queueFamilyIndex = device_details.queue_families[FAMILY_GRAPHICS];

		if (vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool) != VK_SUCCESS) {
			log_error("Failed to create command pool!");
		}
	}

	void create_command_buffer() {

		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // Only primary buffers can be  used for submitting commands to GPU, but secondary buggers can be made to add commands in parallel. 
		alloc_info.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(device, &alloc_info, &command_buffer) != VK_SUCCESS) {
			log_error("Failed to allocate command buffer.");
		}
	}

	void record_command_buffer(VkCommandBuffer command_buffer, std::size_t image_index) {

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
		render_pass_info.framebuffer = swap_chain_framebuffers[image_index];

		// defines where the shader load/stores can take place.
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = swap_chain_extent; 

		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clearColor;

		// Begin render pass with framebuffer data specified in render pass info, along with the specified load and store OPs. 
		// The third parameter is to notify if we are executing all of the rendering commands from the primary command buffer or if we are using secondary command buffers too.
		vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

		// Bind the render pipeline info we provided (shaders, configuration for fixed functions).
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

		// We specified that the following values must be provided at run-time during draw calls to support resizing the window, so these are provided here.
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swap_chain_extent.width);
		viewport.height = static_cast<float>(swap_chain_extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(command_buffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swap_chain_extent;
		vkCmdSetScissor(command_buffer, 0, 1, &scissor);

		vkCmdDraw(command_buffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(command_buffer);

		if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
			log_error("Failed to record commands to command buffer.");
		}
	}

	void create_sync_objects() {

		// Sempahores are used to implement task precedence on the GPU, such as A must occur before B, which must occur before C.
		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		// Fences are used to block the CPU while we wait for tasks on the GPU to finish. 
		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphore) != VK_SUCCESS) {
			log_error("Failed to create image available semaphore.");
		}

		if (vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphore) != VK_SUCCESS) {
			log_error("Failed to create render finished semaphore.");
		}

		if (vkCreateFence(device, &fence_info, nullptr, &in_flight_fence) != VK_SUCCESS) {
			log_error("Failed to create in flight fence.");
		}
	}


	void draw_frame() {
		vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &in_flight_fence);

		uint32_t image_index;
		vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX, image_available_semaphore, VK_NULL_HANDLE, &image_index);

		vkResetCommandBuffer(command_buffer, 0);
		record_command_buffer(command_buffer, static_cast<std::size_t>(image_index));

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Wait until the image is available before submitting rendering commands. 

		VkSemaphore wait_semaphores[] = { image_available_semaphore };
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; 
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;

		VkSemaphore signal_semaphores[] = { render_finished_semaphore };
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		if (vkQueueSubmit(queue_per_family[FAMILY_GRAPHICS], 1, &submit_info, in_flight_fence) != VK_SUCCESS) {
			log_error("Failed to submit queue for rendering");
		}

		// Wait for rendering to finish before submitting present command.
		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;

		VkSwapchainKHR swapChains[] = { swap_chain};
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapChains;
		present_info.pImageIndices = &image_index;
		present_info.pResults = nullptr; // Optional array of result values if using an array of swap chains.

		vkQueuePresentKHR(queue_per_family[FAMILY_PRESENT], &present_info);
	}

	void init_window() {

		glfwInit();

		// Disable OpenGL context (using Vulkan)
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		// Disable window resizing (will handle later).
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); 

		window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Learn Vulkan", nullptr, nullptr);
	}

	GLFWwindow* window;
	std::vector<VkImage> swap_chain_images;
	std::vector<VkImageView> swap_chain_image_views;
	std::vector<VkFramebuffer> swap_chain_framebuffers;

	// Connection between the application and the Vulkan API.
	VkInstance vulkan_instance = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device;
	Device_Requirement_Details device_details;
	std::array<VkQueue, FAMILY_COUNT> queue_per_family;
	VkSurfaceKHR window_surface;
	VkSwapchainKHR swap_chain;
	VkFormat swap_chain_image_format;
	VkExtent2D swap_chain_extent;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	VkRenderPass render_pass;

	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;

	VkSemaphore image_available_semaphore;
	VkSemaphore render_finished_semaphore;
	VkFence in_flight_fence;
};


int main()
{
	App app;
	while (!app.should_close())
	{
		app.update();
	}

	return 0;
}