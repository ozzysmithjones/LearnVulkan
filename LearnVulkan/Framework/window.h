#pragma once
//#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <vulkan/vulkan.h>

GLFWwindow* create_window(int width, int height, const char* title);
VkSurfaceKHR create_window_surface(VkInstance vulkan_instance, GLFWwindow* window);