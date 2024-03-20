#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <fstream>
#include "shader.h"
#include "error.h"


std::vector<char> read_entire_file(const char* file_path) {
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

VkShaderModule create_shader_module(VkDevice device, const char* file_path)
{
	std::vector<char> code = read_entire_file(file_path);

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

ShaderByStage create_shaders(VkDevice device, const char* vertex_shader_path, const char* fragment_shader_path)
{
	return { create_shader_module(device, vertex_shader_path), create_shader_module(device, fragment_shader_path) };
}
