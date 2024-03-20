#include "window.h"
#include "error.h"

GLFWwindow* create_window(int width, int height, const char* title)
{
	glfwInit();
	// Disable OpenGL context (using Vulkan)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Disable window resizing (will handle later).
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	return glfwCreateWindow(width, height, title, nullptr, nullptr);
}

VkSurfaceKHR create_window_surface(VkInstance vulkan_instance, GLFWwindow* window)
{
	VkSurfaceKHR window_surface;
	auto result = glfwCreateWindowSurface(vulkan_instance, window, nullptr, &window_surface);
	if (result != VK_SUCCESS) {

		switch (result) {
		case VK_ERROR_INITIALIZATION_FAILED:
			log_error("Init failed.");
			break;
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			log_error("Extension not present.");
			break;
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			log_error("Window already in use.");
			break;
		}

		log_error("Failed to create window surface");
	}

	return window_surface;
}
