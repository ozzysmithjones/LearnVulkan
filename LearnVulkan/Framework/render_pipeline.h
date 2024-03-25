#pragma once
#include <glm/glm.hpp>
#include <tuple>
#include <vulkan/vulkan.h>
#include "shader.h"
#include "swapchain.h"
#include "mesh.h"



struct PipelineResources {
	VkPipelineLayout pipeline_layout;
	VkDescriptorSetLayout descriptor_set_layout;
};


PipelineResources create_pipeline_resources(VkDevice device);
VkRenderPass create_render_pass(VkDevice device, VkFormat swapchain_format, VkFormat depth_buffer_format);
VkPipeline create_render_pipeline(VkDevice device, VkRenderPass render_pass, VkPipelineLayout pipeline_resource_layout, ShaderByStage& shaders_by_stage, VkExtent2D viewport_extent);
