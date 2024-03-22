#include <optional>
#include "buffer.h"
#include "error.h"

static constexpr VkFormat IMAGE_FORMAT = VK_FORMAT_R8G8B8A8_SRGB;

static VkCommandBuffer begin_single_time_commands(VkDevice device, VkCommandPool command_pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = command_pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}


static void end_single_time_commands(VkDevice device, VkCommandPool command_pool, VkQueue command_queue, VkCommandBuffer command_buffer) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;

    vkQueueSubmit(command_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(command_queue);

    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}


static std::optional<uint32_t> find_memory_type(VkPhysicalDevice physical_device, VkMemoryPropertyFlags required_property_flags, uint32_t type_filter) {

    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    // There's different memory types on the device, each memory type has a set of flags indicating their capabilities. 
    // Find memory type with the properties needed for the buffer.
    // TODO: prioritise these memory types in somer way.
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && ((memory_properties.memoryTypes[i].propertyFlags & required_property_flags) == required_property_flags)) {
            return i;
        }
    }

    log_error("Failed to find suitable memory type for ", required_property_flags);
    return std::nullopt;
}

std::tuple<VkBuffer, VkDeviceMemory> create_buffer(VkDevice device, VkPhysicalDevice physical_device, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, std::span<const uint8_t> data)
{
    // Create buffer object:

    VkBuffer buffer = VK_NULL_HANDLE;
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = data.size();
    buffer_info.usage = usage_flags;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        log_error("Failed to create buffer");
        return { VK_NULL_HANDLE, VK_NULL_HANDLE };
    }

    // Figure out memory requirements and allocate buffer memory in corresponding region:

    VkDeviceMemory device_memory{ VK_NULL_HANDLE };
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memory_requirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(physical_device, memory_flags, memory_requirements.memoryTypeBits).value_or(UINT32_MAX);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &device_memory) != VK_SUCCESS) {
        log_error("Failed to allocate buffer content");
        return { buffer, VK_NULL_HANDLE };
    }

    // Associate buffer memory with buffer handle we created:

    vkBindBufferMemory(device, buffer, device_memory, 0);

    // Copy data into buffer.

    uint8_t* mapped_memory;
    vkMapMemory(device, device_memory, 0, data.size(), 0, (void**)&mapped_memory);
    std::copy(data.begin(), data.end(), mapped_memory);
    vkUnmapMemory(device, device_memory);

    return { buffer, device_memory };
}

std::tuple<VkBuffer, VkDeviceMemory> create_buffer(VkDevice device, VkPhysicalDevice physical_device, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, std::size_t size)
{
    // Create buffer object:

    VkBuffer buffer = VK_NULL_HANDLE;
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage_flags;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        log_error("Failed to create buffer");
        return { VK_NULL_HANDLE, VK_NULL_HANDLE };
    }

    // Figure out memory requirements and allocate buffer memory in corresponding region:

    VkDeviceMemory device_memory{ VK_NULL_HANDLE };
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memory_requirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(physical_device, memory_flags, memory_requirements.memoryTypeBits).value_or(UINT32_MAX);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &device_memory) != VK_SUCCESS) {
        log_error("Failed to allocate buffer content");
        return { buffer, VK_NULL_HANDLE };
    }

    // Associate buffer memory with buffer handle we created:

    vkBindBufferMemory(device, buffer, device_memory, 0);

    return { buffer,device_memory };
}

static void submit_buffer_copy_command(VkDevice device, VkCommandPool command_pool, VkQueue command_queue, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize amount)
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &beginInfo);

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0; // Optional
    copy_region.dstOffset = 0; // Optional
    copy_region.size = amount;
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;

    vkQueueSubmit(command_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(command_queue);
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}



static void submit_image_transition_command(VkDevice device, VkCommandPool command_pool, VkQueue command_queue, VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, VkAccessFlags available_memory, VkAccessFlags visible_memory, VkPipelineStageFlags dependent_stages, VkPipelineStageFlags output_stages) {
    VkCommandBuffer command_buffer = begin_single_time_commands(device, command_pool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    // Transfering layout (data format)
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;

    // Transfering queue family index.
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;

    // Region of image
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // The src mask specifies the memory that needs to be made available for the later commands to use.
    // This memory can only be made available from the specific stages specified in the src stages flags.
    // This is essentially specifying the data that we need to be moved at certain points to the L2 cache.
    barrier.srcAccessMask = available_memory;

    // The dst mask specifies the memory that needs to be made visible from the available memory for later commands to use.
    // It's possible to make memory available but not visible. 
    // This is essentially specifying the data that we need in the L1 cache for direct reads/writes.
    barrier.dstAccessMask = visible_memory;

    // ^ The problem that these flags solve is keeping data coherent across GPU caches. Once execution of previous stages has completed, there can still be information in the L1 cache
    // which needs to be moved back to the L2 cache to be redistributed for other work. These flags also come in READ and WRITE forms, to inform the Vulkan API of the types of operations performed
    // with the memory. For example, we could have some memory which is written to in the source and read from the destination. These READ/WRITE forms do not appear to hold significant purpose appart from 
    // being more explicit to the Vulkan API so it can make optimisations behind the scenes. 

    // A pipeline barrier is similar to a traffic light: It prevents certain later commands from executing until certain previous commands finish executing.
    // Note that the order of this command is relevant. That is, if you re-order this function call there will be different commands before or after this which MIGHT be affected. 
    // The dst mask specifies the furthest stages the later commands can go before having to wait for the previous commands to reach a certain stage, 
    // The src mask specifies the certain stages that the dst mask must wait on before the commands can continue. 
    // This is essentially informing later commands to wait on certain stages until earlier commands reach certain stages and necessary data arrives. 
    vkCmdPipelineBarrier(
        command_buffer,
        dependent_stages,
        output_stages,
        0, /*0 or VK_DEPENDECY_BY_REGION_BIT*/
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    end_single_time_commands(device, command_pool, command_queue, command_buffer);
}


static void submit_buffer_to_image_command(VkDevice device, VkCommandPool command_pool, VkQueue command_queue, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer command_buffer = begin_single_time_commands(device, command_pool);

    // The region of the image to copy:

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    // Set copy buffer to image command. 
    // The layout the image is expected to be in must be specified.
    // This layout is the way that the image is interpreted by the time of reaching the command.
    vkCmdCopyBufferToImage(
        command_buffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    end_single_time_commands(device, command_pool, command_queue, command_buffer);
}

std::tuple<VkBuffer, VkDeviceMemory> create_gpu_buffer(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool command_pool, VkQueue command_queue, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, std::span<const uint8_t> data)
{
    auto [staging_buffer, staging_buffer_memory] = create_buffer(device, physical_device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, data);
    auto [gpu_buffer, gpu_buffer_memory] = create_buffer(device, physical_device, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage_flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | memory_flags, data.size());
    submit_buffer_copy_command(device, command_pool, command_queue, staging_buffer, gpu_buffer, data.size());
    vkFreeMemory(device, staging_buffer_memory, nullptr);
    vkDestroyBuffer(device, staging_buffer, nullptr);
    return { gpu_buffer, gpu_buffer_memory };
}

FrameUniformBuffers create_frame_uniform_buffers(VkDevice device, VkPhysicalDevice physical_device)
{
    FrameUniformBuffers frame_uniform_buffers{};

    for (UniformBuffer& uniform_buffer : frame_uniform_buffers) {
        auto [buffer, buffer_memory] = create_buffer(device, physical_device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(UniformBufferContent));
        uniform_buffer.buffer = buffer;
        uniform_buffer.memory = buffer_memory;
        vkMapMemory(device, uniform_buffer.memory, 0, sizeof(UniformBufferContent), 0, &uniform_buffer.mapped_region);
    }

    return frame_uniform_buffers;
}

std::tuple<VkImage, VkDeviceMemory> create_image(VkDevice device, VkPhysicalDevice physical_device, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, VkFormat format, VkImageTiling tiling, uint32_t width, uint32_t height, std::span<const uint8_t> data)
{
    VkImage image{ VK_NULL_HANDLE };
    VkDeviceMemory image_memory{ VK_NULL_HANDLE };

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;

    // The tiling cannot be changed later, this must be set to linear if the txels need to be directly accessed.
    image_info.tiling = tiling;

    // The initial layout specifies if it is okay to discard the texels before entering the first transition.
    // In this case, we will be transitioning the image straight to the GPU before initialising the texels so there's no need to preseve them on the first transition.
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage_flags;

    // Sharing mode is needed if we want to share the resource across multiple queue families.
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = 0; // Optional. Here can specify a flag to make a spare image - a spare image is an image that is not completely allocated. This allows for unfilled gaps in the image memory.


    if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS) {
        log_error("Failed to create image");
        return { VK_NULL_HANDLE, VK_NULL_HANDLE };
    }

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, image, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(physical_device, memory_flags, requirements.memoryTypeBits).value_or(UINT32_MAX);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &image_memory) != VK_SUCCESS) {
        log_error("Failed to create image memory");
        return { image, VK_NULL_HANDLE };
    }

    vkBindImageMemory(device, image, image_memory, 0);

    uint8_t* mapped_memory = VK_NULL_HANDLE;
    vkMapMemory(device, image_memory, 0, requirements.size, 0, (void**)&mapped_memory);
    std::copy(data.begin(), data.end(), mapped_memory);
    vkUnmapMemory(device, image_memory);

    return { image, image_memory };
}

std::tuple<VkImage, VkDeviceMemory> create_image(VkDevice device, VkPhysicalDevice physical_device, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags memory_flags, VkFormat format, VkImageTiling tiling, uint32_t width, uint32_t height)
{
    VkImage image{ VK_NULL_HANDLE };
    VkDeviceMemory image_memory{ VK_NULL_HANDLE };

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;

    // The tiling cannot be changed later, this must be set to linear if the txels need to be directly accessed.
    image_info.tiling = tiling;

    // The initial layout specifies if it is okay to discard the texels before entering the first transition.
    // In this case, we will be transitioning the image straight to the GPU before initialising the texels so there's no need to preseve them on the first transition.
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage_flags;

    // Sharing mode is needed if we want to share the resource across multiple queue families.
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.flags = 0; // Optional. Here can specify a flag to make a spare image - a spare image is an image that is not completely allocated. This allows for unfilled gaps in the image memory.


    if (vkCreateImage(device, &image_info, nullptr, &image) != VK_SUCCESS) {
        log_error("Failed to create image");
        return { VK_NULL_HANDLE, VK_NULL_HANDLE };
    }

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, image, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(physical_device, memory_flags, requirements.memoryTypeBits).value_or(UINT32_MAX);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &image_memory) != VK_SUCCESS) {
        log_error("Failed to create image memory");
        return { image, VK_NULL_HANDLE };
    }

    vkBindImageMemory(device, image, image_memory, 0);
    return { image, image_memory };
}

std::tuple<VkImage, VkDeviceMemory> create_gpu_image(VkDevice device, VkPhysicalDevice physical_device, VkCommandPool command_pool, VkQueue command_queue,  VkFormat format, VkImageTiling tiling, uint32_t width, uint32_t height, std::span<const uint8_t> data)
{
    auto [buffer, buffer_memory] = create_buffer(device, physical_device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, data);
    auto [image, image_memory] = create_image(device, physical_device, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, format, tiling, width, height);

    submit_image_transition_command(device, command_pool, command_queue, image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    submit_buffer_to_image_command(device, command_pool, command_queue, buffer, image, width, height);
    submit_image_transition_command(device, command_pool, command_queue, image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    vkFreeMemory(device, buffer_memory, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);

    return { image, image_memory };
}
