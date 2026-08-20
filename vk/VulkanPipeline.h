#pragma once

#include "VulkanCommon.h"
#include <type_traits>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace VulkanPipeline {

std::vector<uint32_t> loadSpirv(const std::string& path);

std::string shaderDir(const std::string& exampleName);

VkPipelineLayout createLayout(VkDevice device,
                                      uint32_t pushConstantBytes,
                                      VkDescriptorSetLayout setLayout);

}


struct PipelineBuildContext {
    VkGraphicsPipelineCreateInfo&            info;
    VkPipelineShaderStageCreateInfo*         stages;
    uint32_t                                 stageCount;
    VkPipelineVertexInputStateCreateInfo&    vertexInput;
    VkPipelineInputAssemblyStateCreateInfo&  inputAssembly;
    VkPipelineViewportStateCreateInfo&       viewport;
    VkPipelineRasterizationStateCreateInfo&  rasterization;
    VkPipelineMultisampleStateCreateInfo&    multisample;
    VkPipelineDepthStencilStateCreateInfo&   depthStencil;
    VkPipelineColorBlendStateCreateInfo&     blend;
    VkPipelineColorBlendAttachmentState&     blendAttachment;
    VkPipelineRenderingCreateInfo&           rendering;

    std::vector<VkDynamicState>&             dynamicStates;
    std::vector<std::unique_ptr<unsigned char[]>>& blocks;

    template <typename T>
    T& chain(const void*& pNextField, T structure){
        static_assert(alignof(T) <= alignof(std::max_align_t), "PipelineBuildContext::chain<T>: T over-aligned for make_unique<unsigned char[]> storage");
        static_assert(std::is_trivially_destructible_v<T>, "PipelineBuildContext::chain<T>: T destructor would never run");  
        auto storage = std::make_unique<unsigned char[]>(sizeof(T));
        T* slot      = new (storage.get()) T(structure);

        blocks.push_back(std::move(storage));
        auto* tail = reinterpret_cast<VkBaseOutStructure*> (slot);
        while(tail->pNext != nullptr){
          tail = tail->pNext;
        }

        tail->pNext = reinterpret_cast<VkBaseOutStructure*>(const_cast<void*>(pNextField));
        pNextField  = slot;
        return *slot;
    };

};

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
    GraphicsPipelineBuilder& configure(std::function<void(PipelineBuildContext&)>callback); 

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
    std::vector<std::unique_ptr<unsigned char[]>> blocks_;
    std::vector<VkDynamicState>                  dynamicStates_;
    std::function<void(PipelineBuildContext&)>   callback_;
};
