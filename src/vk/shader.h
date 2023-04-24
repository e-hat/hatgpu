#ifndef _INCLUDED_SHADER_H
#define _INCLUDED_SHADER_H
#include "hatpch.h"

namespace hatgpu
{
namespace vk
{
VkPipelineShaderStageCreateInfo createShaderStage(VkDevice device,
                                                  const std::string &filename,
                                                  VkShaderStageFlagBits stage);
}
}  // namespace hatgpu

#endif
