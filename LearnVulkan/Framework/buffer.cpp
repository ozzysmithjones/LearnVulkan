#include <optional>
#include "buffer.h"
#include "error.h"


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

void submit_buffer_copy_command(VkDevice device, VkCommandPool command_pool, VkQueue command_queue, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize amount)
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
