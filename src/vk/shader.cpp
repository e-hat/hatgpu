#include "hatpch.h"

#include "shader.h"

#include "vk/initializers.h"

#ifdef DEBUG
#    include <vulkan/vk_enum_string_helper.h>
#endif

#include <fstream>

namespace
{
std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    H_ASSERT(file.is_open(), "Failed to open file");

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}
VkShaderModule createShaderModule(VkDevice device, const std::vector<char> &code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    H_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule),
            "Failed to create shader module");

    return shaderModule;
}
}  // namespace

namespace hatgpu
{
namespace vk
{
VkPipelineShaderStageCreateInfo createShaderStage(VkDevice device,
                                                  const std::string &filename,
                                                  VkShaderStageFlagBits stage)
{
    H_LOG("Compiling shader " + filename + " for stage " + string_VkShaderStageFlagBits(stage));
    const std::vector<char> code = readFile(filename);
    VkShaderModule module        = createShaderModule(device, code);

    return pipelineShaderStageInfo(stage, module);
}
}  // namespace vk
}  // namespace hatgpu
