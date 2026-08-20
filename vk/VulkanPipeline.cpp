#include "VulkanPipeline.h"

#include <SDL3/SDL_filesystem.h>

#include <cstdio>
#include <cstdlib>
#include <SDL3/SDL_iostream.h>

std::vector<uint32_t> VulkanPipeline::loadSpirv(const std::string& path) {
    SDL_IOStream* file = SDL_IOFromFile(path.c_str(), "rb");

    if (!file) {
        SDL_LogError(
            SDL_LOG_CATEGORY_ERROR,
            "cannot open SPIR-V: %s",
            path.c_str()
        );
        std::abort();
    }

    Sint64 bytes = SDL_GetIOSize(file);

    if (bytes <= 0 || bytes % 4 != 0) {
        SDL_LogError(
            SDL_LOG_CATEGORY_ERROR,
            "bad SPIR-V size (%lld) in %s",
            static_cast<long long>(bytes),
            path.c_str()
        );
        SDL_CloseIO(file);
        std::abort();
    }

    std::vector<uint32_t> words(
        static_cast<size_t>(bytes) / sizeof(uint32_t)
    );

    size_t read = SDL_ReadIO(
        file,
        words.data(),
        static_cast<size_t>(bytes)
    );

    SDL_CloseIO(file);

    if (read != static_cast<size_t>(bytes)) {
        SDL_LogError(
            SDL_LOG_CATEGORY_ERROR,
            "short read on %s",
            path.c_str()
        );
        std::abort();
    }

    return words;
}


std::string VulkanPipeline::shaderDir(const std::string& exampleName) {
    const char* base = SDL_GetBasePath();
    return std::string(base ? base : "") + "shaders/" + exampleName + "/";
}

VkPipelineLayout VulkanPipeline::createLayout(VkDevice device,
                                      uint32_t pushConstantBytes,
                                      VkDescriptorSetLayout setLayout)
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_ALL;
    range.offset     = 0;
    range.size       = pushConstantBytes;

    VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    info.setLayoutCount         = setLayout != VK_NULL_HANDLE ? 1u : 0u;
    info.pSetLayouts            = setLayout != VK_NULL_HANDLE ? &setLayout : nullptr;
    info.pushConstantRangeCount = pushConstantBytes > 0 ? 1u : 0u;
    info.pPushConstantRanges    = pushConstantBytes > 0 ? &range : nullptr;

    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &layout));
    return layout;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::shaders(
    const std::vector<uint32_t>& vertexSpirv, const std::vector<uint32_t>& fragmentSpirv)
{
    vertexSpirv_   = vertexSpirv;
    fragmentSpirv_ = fragmentSpirv;
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::colorFormat(VkFormat format) {
    colorFormat_ = format; return *this;
}
GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthFormat(VkFormat format) {
    depthFormat_ = format; return *this;
}
GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthTest(bool enable) {
    depthTest_ = enable; return *this;
}
GraphicsPipelineBuilder& GraphicsPipelineBuilder::cullMode(VkCullModeFlags mode) {
    cullMode_ = mode; return *this;
}
GraphicsPipelineBuilder& GraphicsPipelineBuilder::topology(VkPrimitiveTopology t) {
    topology_ = t; return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::configure(
    std::function<void(PipelineBuildContext&)> callback){

    callback_ = std::move(callback); return *this;
}
VkPipeline GraphicsPipelineBuilder::build(VkPipelineLayout layout) {
    blocks_.clear();
    dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkShaderModuleCreateInfo vertModule{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    vertModule.codeSize = vertexSpirv_.size() * sizeof(uint32_t);
    vertModule.pCode    = vertexSpirv_.data();

    VkShaderModuleCreateInfo fragModule{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    fragModule.codeSize = fragmentSpirv_.size() * sizeof(uint32_t);
    fragModule.pCode    = fragmentSpirv_.data();

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext  = &vertModule;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = VK_NULL_HANDLE;
    stages[0].pName  = "main";

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext  = &fragModule;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = VK_NULL_HANDLE;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = topology_;

    VkPipelineViewportStateCreateInfo viewport{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo raster{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode    = cullMode_;
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

    depthStencil.depthTestEnable  = depthTest_ ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = depthTest_ ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.maxDepthBounds   = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments    = &blendAttachment;

    
    VkPipelineDynamicStateCreateInfo dynamic{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  

    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount    = 1;
    rendering.pColorAttachmentFormats = &colorFormat_;
    rendering.depthAttachmentFormat   = depthFormat_;

    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.pNext               = &rendering;
    info.stageCount          = 2;
    info.pStages             = stages;
    info.pVertexInputState   = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState      = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState   = &multisample;
    info.pDepthStencilState  = &depthStencil;
    info.pColorBlendState    = &blend;
    info.pDynamicState       = &dynamic;
    info.layout              = layout;
    info.renderPass          = VK_NULL_HANDLE;


    if(callback_){
        PipelineBuildContext ctx{
            .info            = info,
            .stages          = stages,
            .stageCount      = 2u,
            .vertexInput     = vertexInput,
            .inputAssembly   = inputAssembly,
            .viewport        = viewport,
            .rasterization   = raster,
            .multisample     = multisample,
            .depthStencil    = depthStencil,
            .blend           = blend,
            .blendAttachment = blendAttachment,
            .rendering       = rendering,
            .dynamicStates   = dynamicStates_,
            .blocks          = blocks_,
        };

      callback_(ctx);
    }
    dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates_.size());
    dynamic.pDynamicStates    = dynamicStates_.data();
    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
    return pipeline;
}
