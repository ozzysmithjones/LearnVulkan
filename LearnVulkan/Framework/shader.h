#pragma once
#include <vulkan/vulkan.h>
#include <array>

enum class ShaderStage : std::size_t {
	Vertex,
	Fragment,
	COUNT,
};


using ShaderByStage = std::array<VkShaderModule, static_cast<std::size_t>(ShaderStage::COUNT)>;

VkShaderModule create_shader_module(VkDevice device, const char* file_path);
ShaderByStage create_shaders(VkDevice device, const char* vertex_shader_path, const char* fragment_shader_path);