#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include "physical_device.h"

using QueueByFeature = std::array<VkQueue, FEATURE_COUNT>;
[[nodiscard]] VkDevice create_device(VkPhysicalDevice physical_device, const QueueFamilyIndexByFeature& queue_family_index_by_feature, QueueByFeature& out_queue_by_feature);



