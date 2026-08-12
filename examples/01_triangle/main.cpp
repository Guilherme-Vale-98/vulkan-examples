#include "VulkanBase.h"

#include <array>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

struct TrianglePush {
    glm::mat4       mvp;
    VkDeviceAddress vertices;
};

static_assert(sizeof(Vertex) == 24, "scalar layout: vec3 is 12 bytes");
static_assert(sizeof(TrianglePush) == 72, "mat4 at 0, pointer at 64");
static_assert(offsetof(TrianglePush, vertices) == 64, "must match SPIR-V reflection");

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

        layout_ = createPipelineLayout(context().device(), sizeof(TrianglePush), VK_NULL_HANDLE);

        pipeline_ = GraphicsPipelineBuilder(context().device())
                        .shaders(loadSpirv(shaderPath("triangle.vert.spv")),
                                 loadSpirv(shaderPath("triangle.frag.spv")))
                        .colorFormat(swapchain().format())
                        .depthFormat(swapchain().depthFormat())
                        .depthTest(true)
                        .build(layout_);
    }

    void onRender(VkCommandBuffer cmd, const FrameContext& frame) override {
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

        TrianglePush push{};
        push.mvp      = glm::mat4(1.0f);
        push.vertices = vertices_.address();

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_ALL, 0, sizeof(push), &push);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);
    }

private:
    static ExampleConfig makeConfig() {
        ExampleConfig cfg;
        cfg.name   = "01_triangle";
        cfg.title  = "01 - Triangle (buffer device address)";
        cfg.width  = 1280;
        cfg.height = 720;
        cfg.depth  = true;
        cfg.forceValidation = true;
        return cfg;
    }

    VulkanBuffer     vertices_;
    VkPipelineLayout layout_   = VK_NULL_HANDLE;
    VkPipeline       pipeline_ = VK_NULL_HANDLE;
};

int main(int, char*[]) {
    TriangleExample app;
    return app.run();
}
