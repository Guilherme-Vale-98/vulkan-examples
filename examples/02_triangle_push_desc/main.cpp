#include "VulkanBase.h"
#include <cstring>
#include <array>
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

struct Transform{
    glm::mat4       mvp;
};

static_assert(sizeof(Vertex) == 24, "scalar layout: vec3 is 12 bytes");
static_assert(sizeof(Transform) == 64, "A single mat4, each vec is 16 bytes");

class TriangleExample : public VulkanBase {
public:
    TriangleExample() : VulkanBase(makeConfig()) {}

    ~TriangleExample() override {
        VkDevice device = context().device();
        if (pipeline_) vkDestroyPipeline(device, pipeline_, nullptr);
        if (layout_)   vkDestroyPipelineLayout(device, layout_, nullptr);
    }

protected:
    void onInit() override {
        const std::array<Vertex, 3> vertices{{
            {{ 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        }};

        vertices_ = createBufferWithData(context(), vertices.data(), sizeof(vertices),
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        transforms_.resize(config().framesInFlight);
        for (auto& buffer : transforms_) {
            buffer = createBuffer(context(), sizeof(Transform),
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VMA_MEMORY_USAGE_AUTO_PREFER_HOST, true);
        }
        createDescriptorLayout();
        layout_ = createPipelineLayout(context().device(), 0, setLayout_);
        setObjectName(context().device(), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
              reinterpret_cast<uint64_t>(setLayout_), "02.materialSetLayout");
        pipeline_ = GraphicsPipelineBuilder(context().device())
                        .shaders(loadSpirv(shaderPath("push.vert.spv")),
                                 loadSpirv(shaderPath("push.frag.spv")))
                        .colorFormat(swapchain().format())
                        .depthFormat(swapchain().depthFormat())
                        .depthTest(true)
                        .build(layout_);
    }

    
    void createDescriptorLayout() {
        VkDescriptorSetLayoutBinding bindings[2]{};

        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;
        info.bindingCount = 2;
        info.pBindings    = bindings;

        VK_CHECK(vkCreateDescriptorSetLayout(context().device(), &info, nullptr, &setLayout_));
    }


    void onRender(VkCommandBuffer cmd, const FrameContext& frame) override {
        Transform xform{};
        xform.mvp = glm::mat4(1.0f);
        std::memcpy(transforms_[frame.frameIndex].mapped(), &xform, sizeof(xform));


        VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        color.imageView        = frame.swapchainView;
        color.imageLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color = {{0.02f, 0.02f, 0.05f, 1.0f}};

        VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depth.imageView               = frame.depthView;
        depth.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp                 = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.clearValue.depthStencil = {1.0f, 0};

        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea           = {{0, 0}, frame.extent};
        rendering.layerCount           = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments    = &color;
        rendering.pDepthAttachment     = frame.depthView ? &depth : nullptr;

        vkCmdBeginRendering(cmd, &rendering);
        VkDescriptorBufferInfo vertexInfo{};
        vertexInfo.buffer = vertices_.handle();
        vertexInfo.offset = 0;
        vertexInfo.range  = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo transformInfo{};
        transformInfo.buffer = transforms_[frame.frameIndex].handle();
        transformInfo.offset = 0;
        transformInfo.range  = sizeof(Transform);


        VkWriteDescriptorSet writes[2]{};

        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstBinding      = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].pBufferInfo     = &vertexInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstBinding      = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].pBufferInfo     = &transformInfo;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdPushDescriptorSet(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 2, writes);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);
    }

private:
    static ExampleConfig makeConfig() {
        ExampleConfig cfg;
        cfg.name   = "02_triangle_push_desc";
        cfg.title  = "02 - Triangle (push descriptor)";
        cfg.width  = 1280;
        cfg.height = 720;
        cfg.depth  = true;
        cfg.forceValidation = true;
        return cfg;
    }

    VulkanBuffer              vertices_;
    std::vector<VulkanBuffer> transforms_;
    VkDescriptorSetLayout     setLayout_   = VK_NULL_HANDLE;
    VkPipelineLayout          layout_      = VK_NULL_HANDLE;
    VkPipeline                pipeline_    = VK_NULL_HANDLE;
};

int main(int, char*[]) {
    TriangleExample app;
    return app.run();
}
