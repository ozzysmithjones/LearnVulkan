#include <glm/glm.hpp>
#include "render_pipeline.h"
#include "error.h"


// Describes the whole vertex struct to vulkan, how big it is and which buffer to use it in (binding).
static VkVertexInputBindingDescription get_vertex_binding_description() {
	VkVertexInputBindingDescription binding_description{};
	binding_description.binding = 0;
	binding_description.stride = sizeof(Vertex);
	binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return binding_description;
}

// Describes individual members of the struct to vulkan. The location specifies the id from which the value can be referemced oin a vertex shader.
static std::array<VkVertexInputAttributeDescription, 3> get_vertex_attribute_descriptions() {
	std::array<VkVertexInputAttributeDescription, 3> attribute_descriptions{};
	attribute_descriptions[0].binding = 0;
	attribute_descriptions[0].location = 0;
	attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
	attribute_descriptions[0].offset = offsetof(Vertex, pos);

	attribute_descriptions[1].binding = 0;
	attribute_descriptions[1].location = 1;
	attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribute_descriptions[1].offset = offsetof(Vertex, color);

	attribute_descriptions[2].binding = 0;
	attribute_descriptions[2].location = 2;
	attribute_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attribute_descriptions[2].offset = offsetof(Vertex, uv);

	return attribute_descriptions;
}


PipelineResources create_pipeline_resources(VkDevice device)
{
	PipelineResources pipeline_resources{};

	// A descriptor is essentially a pointer to a resource used by shaders in the render pipeline.
	// A descriptor set is a collection of descriptors. The reason why we organise descriptors into sets is so that they can be bound or unbound collectively at different points.
	// A descriptor will occupy a binding. There can be multiple descriptor sets with the same binding numbers so long as they have different usages when bound at the same time. 
	// For example, a descriptor set bound for graphics and a descriptor set bound for compute shading can share the same binding numbers for the descriptors within them.



	VkDescriptorSetLayoutBinding ubo_layout_binding{};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	// It is possible for a shader variable to be an array, in which case multiple buffers can be sent for one binding across instead of one.
	ubo_layout_binding.descriptorCount = 1;

	// stages that the buffer should be available in.
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Not relevant for buffers, but useful for image sampling related stuff.
	ubo_layout_binding.pImmutableSamplers = nullptr; // Optional


	// attach sampler to be used during fragment shading.

	VkDescriptorSetLayoutBinding sampler_layout_binding{};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.pImmutableSamplers = VK_NULL_HANDLE;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;


	// A descriptor set layout is essentially a flyweight-class detailing the format that the descriptor set will be in. It contains an array of bindings which detail the format of each descriptor
	// and the binding it will have. This is like specifying an interface - i.e the contract agreed upon by the graphics pipeline and our CPU code of what the descriptors will look like. 
	// This information must be provided up front in Vulkan, and this descriptor layout must be used to initialise the render pipeline and bind the descriptors when they are used. 
	// Creation of descriptors is also based from this layout. Multiple descriptor sets can use the same descriptor layout. 

	std::array<VkDescriptorSetLayoutBinding, 2> descriptor_layout_bindings = { ubo_layout_binding, sampler_layout_binding };
	VkDescriptorSetLayoutCreateInfo descriptor_layout_info{};
	descriptor_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_layout_info.bindingCount = descriptor_layout_bindings.size();
	descriptor_layout_info.pBindings = descriptor_layout_bindings.data();

	if (vkCreateDescriptorSetLayout(device, &descriptor_layout_info, nullptr, &pipeline_resources.descriptor_set_layout) != VK_SUCCESS) {
		log_error("Failed to create descriptor set layout!");
	}

	// Layout of descriptor sets and constants that can be used to send info to the shaders.
	VkPipelineLayoutCreateInfo pipeline_layout_info{};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = 1; // Optional

	pipeline_layout_info.pSetLayouts = &pipeline_resources.descriptor_set_layout; // Optional 
	pipeline_layout_info.pushConstantRangeCount = 0; // Optional
	pipeline_layout_info.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_resources.pipeline_layout) != VK_SUCCESS) {
		log_error("Failed to create pipeline layout!");
	}

	return pipeline_resources;
}

VkRenderPass create_render_pass(VkDevice device, VkFormat swapchain_format)
{
	VkRenderPass render_pass;
	// An attachment is a description of a resource used during rendering. 
		// Vulkan organises rendering into discreet steps called sub-passes. A subpass is a sequence of steps that read from or edit a set of images, using the attachments that describe them.
		// For example, A full rendering pipeline is an example of a subpass. Multiple Subpasses can be chained together to form a render pass with multiple stages -> i.e first render the scene, then apply shadows and lighting.
		// In this case we just want a single sub-pass to render a triangle with no additional post-processing effects. 
		// We need an attachment to detail the resource that we want to output from the rendering pipeline. This attachment is directly referenced by index in the fragment shader.
	VkAttachmentDescription color_attachment{};
	color_attachment.format = swapchain_format;
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

	return render_pass;
}

using ShaderStageInfos = std::array<VkPipelineShaderStageCreateInfo, static_cast<std::size_t>(ShaderStage::COUNT)>;

static ShaderStageInfos create_shader_stage_infos(ShaderByStage& shaders_by_stage) {

	ShaderStageInfos shader_stage_infos{};
	std::array<VkShaderStageFlagBits, static_cast<std::size_t>(ShaderStage::COUNT)> stage_bit_by_enum_value {
		VK_SHADER_STAGE_VERTEX_BIT,
		VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	for (std::size_t i = 0; i < shader_stage_infos.size(); ++i) {
		auto& shader_stage_info = shader_stage_infos[i];
		shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stage_info.pName = "main";
		shader_stage_info.module = shaders_by_stage.at(i);
		shader_stage_info.stage = stage_bit_by_enum_value[i];
	}

	return shader_stage_infos;
}

struct InputGeometryInfo {
	VkPipelineInputAssemblyStateCreateInfo primitive_layout{};
	VkPipelineVertexInputStateCreateInfo vertex_layout{};
	VkVertexInputBindingDescription vertex_binding_description{};
	std::array<VkVertexInputAttributeDescription, 3> vertex_attribute_descriptions{};
};

static InputGeometryInfo create_input_geometry_info() {
	InputGeometryInfo input_layout_info{};

	VkPipelineInputAssemblyStateCreateInfo& primitive_layout = input_layout_info.primitive_layout;
	primitive_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	primitive_layout.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// This bool indicates if it is possible to prevent reclying of vertices when not using list-type topology.
	// Should it be possible to restart a primitive halfway through? 
	primitive_layout.primitiveRestartEnable = VK_FALSE;

	// Describes the format of the vertex data that will be passed to the vertex shader.
	// Uses arrays of structs to describe the stride and format of the vertices. 

	input_layout_info.vertex_binding_description = get_vertex_binding_description();
	input_layout_info.vertex_attribute_descriptions = get_vertex_attribute_descriptions();

	VkPipelineVertexInputStateCreateInfo& vertex_layout = input_layout_info.vertex_layout;
	vertex_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_layout.vertexAttributeDescriptionCount = static_cast<uint32_t>(input_layout_info.vertex_attribute_descriptions.size());
	vertex_layout.pVertexAttributeDescriptions = input_layout_info.vertex_attribute_descriptions.data();
	vertex_layout.vertexBindingDescriptionCount = 1;
	vertex_layout.pVertexBindingDescriptions = &input_layout_info.vertex_binding_description;

	return input_layout_info;
}


struct ViewportInfo {
	VkViewport viewport;
	VkRect2D crop;
	VkPipelineViewportStateCreateInfo viewport_create_info;
};

static ViewportInfo create_viewport_info(VkExtent2D extent) {
	ViewportInfo viewport_info{};

	// If we want to crop the image, the scissor option can be used.

	VkRect2D& scissor = viewport_info.crop;
	scissor.offset = { 0,0 };
	scissor.extent = extent;

	// The view port describes the portion of the framebuffer that will be rendered to.
	// The content is NOT cropped, it is instead stretched to the fill area.

	VkViewport& viewport = viewport_info.viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)extent.width;
	viewport.height = (float)extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkPipelineViewportStateCreateInfo& viewport_create_info = viewport_info.viewport_create_info;
	viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_create_info.viewportCount = 1;
	viewport_create_info.pViewports = &viewport;
	viewport_create_info.scissorCount = 1;
	viewport_create_info.pScissors = &scissor;

	return viewport_info;
}

struct ColorOutputInfo {
	VkPipelineRasterizationStateCreateInfo rasterizer_info;
	VkPipelineMultisampleStateCreateInfo multisampling_info;
	VkPipelineColorBlendAttachmentState color_blend_attachment_info;
	VkPipelineColorBlendStateCreateInfo color_blend_info;
};


static ColorOutputInfo create_color_output_info() {
	ColorOutputInfo color_output_info{};

	// Configure the rasterisation step to draw lines, fill or dots.
	VkPipelineRasterizationStateCreateInfo& rasterizer_info = color_output_info.rasterizer_info;
	rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	//If depth clamp enable is set to true, then fragments outside of view are clamped to be within view instead. (why idk)
	rasterizer_info.depthClampEnable = VK_FALSE;

	//this bool can set to disable rasterisation (if we just want to run a vertex shader without any output).
	rasterizer_info.rasterizerDiscardEnable = VK_FALSE;

	// This mode mode can be either FILL, LINE or POINT. If we just want to draw the lines or the vertices of the mesh, this can be modified. 
	rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;

	//NOTE: you need to enable the wide lines GPU feature if lines thicker than 1 are needed. 
	rasterizer_info.lineWidth = 1.0f;

	rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	//Depth values can be biased based on fragments slope, this is sometimes used for shadow mapping.
	rasterizer_info.depthBiasEnable = VK_FALSE;
	rasterizer_info.depthBiasConstantFactor = 0.0f;
	rasterizer_info.depthBiasClamp = 0.0f;
	rasterizer_info.depthBiasSlopeFactor = 0.0f;


	// Multisampling is a technique used to provide an anti-aliasing effect on geometry edges.
	// Disabled for now.
	// Revisit sampling at a later time.
	VkPipelineMultisampleStateCreateInfo& multisampling = color_output_info.multisampling_info;
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
	VkPipelineColorBlendAttachmentState& color_blend_attachment = color_output_info.color_blend_attachment_info;
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
	VkPipelineColorBlendStateCreateInfo& color_blend_info = color_output_info.color_blend_info;
	color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_info.logicOpEnable = VK_FALSE;
	color_blend_info.logicOp = VK_LOGIC_OP_COPY; // Optional
	color_blend_info.attachmentCount = 1;
	color_blend_info.pAttachments = &color_blend_attachment;
	color_blend_info.blendConstants[0] = 0.0f; // Optional
	color_blend_info.blendConstants[1] = 0.0f; // Optional
	color_blend_info.blendConstants[2] = 0.0f; // Optional
	color_blend_info.blendConstants[3] = 0.0f; // Optional

	return color_output_info;
}

VkPipeline create_render_pipeline(VkDevice device, VkRenderPass render_pass, VkPipelineLayout pipeline_resource_layout, ShaderByStage&& shaders_by_stage, VkExtent2D viewport_extent)
{
	VkPipeline pipeline;
	ShaderStageInfos shader_stage_infos = create_shader_stage_infos(shaders_by_stage);
	InputGeometryInfo input_layout_info = create_input_geometry_info();
	ViewportInfo viewport_info = create_viewport_info(viewport_extent);
	ColorOutputInfo color_output_info = create_color_output_info();

	//States that change during runtime must be explicitly declared.
	std::array<VkDynamicState, 2> dynamic_states = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_info{};
	dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_info.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
	dynamic_state_info.pDynamicStates = dynamic_states.data();

	VkGraphicsPipelineCreateInfo pipeline_info{};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = static_cast<uint32_t>(shader_stage_infos.size());
	pipeline_info.pStages = shader_stage_infos.data();
	pipeline_info.pVertexInputState = &input_layout_info.vertex_layout;
	pipeline_info.pInputAssemblyState = &input_layout_info.primitive_layout;
	pipeline_info.pViewportState = &viewport_info.viewport_create_info;
	pipeline_info.pRasterizationState = &color_output_info.rasterizer_info;
	pipeline_info.pMultisampleState = &color_output_info.multisampling_info;
	pipeline_info.pDepthStencilState = nullptr; // Optional
	pipeline_info.pColorBlendState = &color_output_info.color_blend_info;
	pipeline_info.pDynamicState = &dynamic_state_info;
	pipeline_info.layout = pipeline_resource_layout;
	pipeline_info.renderPass = render_pass; // parent render pass.
	pipeline_info.subpass = 0; //. subpass where this render pipeline will be used.

	// This allows to derive from existing pipeline data, disabled here.
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional 
	pipeline_info.basePipelineIndex = -1; // Optional

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
		log_error("Failed to create graphics pipeline!");
	}

	for (VkShaderModule shader_module : shaders_by_stage) {
		vkDestroyShaderModule(device, shader_module, nullptr);
	}
	shaders_by_stage.fill(VK_NULL_HANDLE);

	return pipeline;
}
