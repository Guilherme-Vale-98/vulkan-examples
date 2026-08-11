#pragma once

#include "VulkanCommon.h"

#include <string>
#include <vector>

std::vector<uint32_t> loadSpirv(const std::string& path);

std::string shaderDir(const std::string& exampleName);

VkPipelineLayout createPipelineLayout(VkDevice device,
                                      uint32_t pushConstantBytes,
                                      VkDescriptorSetLayout setLayout);

class GraphicsPipelineBuilder {
public:
    explicit GraphicsPipelineBuilder(VkDevice device) : device_(device) {}

    GraphicsPipelineBuilder& shaders(const std::vector<uint32_t>& vertexSpirv,
                                     const std::vector<uint32_t>& fragmentSpirv);
    GraphicsPipelineBuilder& colorFormat(VkFormat format);
    GraphicsPipelineBuilder& depthFormat(VkFormat format);
    GraphicsPipelineBuilder& depthTest(bool enable);
    GraphicsPipelineBuilder& cullMode(VkCullModeFlags mode);
    GraphicsPipelineBuilder& topology(VkPrimitiveTopology topology);

    VkPipeline build(VkPipelineLayout layout);

private:
    VkDevice              device_;
    std::vector<uint32_t> vertexSpirv_;
    std::vector<uint32_t> fragmentSpirv_;
    VkFormat              colorFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormat              depthFormat_ = VK_FORMAT_UNDEFINED;
    bool                  depthTest_   = false;
    VkCullModeFlags       cullMode_    = VK_CULL_MODE_NONE;
    VkPrimitiveTopology   topology_    = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
};
