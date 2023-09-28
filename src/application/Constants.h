#ifndef _INCLUDE_CONSTANTS_H
#define _INCLUDE_CONSTANTS_H

#include <vulkan/vulkan_core.h>

namespace hatgpu
{
namespace constants
{
static constexpr size_t kMaxFramesInFlight = 1;
static constexpr VkFormat kDepthFormat     = VK_FORMAT_D32_SFLOAT;
}  // namespace constants
}  // namespace hatgpu

#endif
