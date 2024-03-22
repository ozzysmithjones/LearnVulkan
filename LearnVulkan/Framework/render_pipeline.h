#pragma once
#include <glm/glm.hpp>
#include <tuple>
#include <vulkan/vulkan.h>
#include "shader.h"
#include "swapchain.h"

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;
	glm::vec2 uv;
};

struct PipelineResources {
	VkPipelineLayout pipeline_layout;
	VkDescriptorSetLayout descriptor_set_layout;
};


PipelineResources create_pipeline_resources(VkDevice device);
VkRenderPass create_render_pass(VkDevice device, VkFormat swapchain_format);
VkPipeline create_render_pipeline(VkDevice device, VkRenderPass render_pass, VkPipelineLayout pipeline_resource_layout, ShaderByStage&& shaders_by_stage, VkExtent2D viewport_extent);
