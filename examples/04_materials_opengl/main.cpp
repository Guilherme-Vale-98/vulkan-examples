#include "VulkanBase.h"
#include <array>
#include <cassert>
#include <cstring>
#include <cmath>

constexpr int DESCRIPTORS_COUNT = 2;

struct PushBlock {
    glm::mat4 model;         
    glm::vec4 objectColor;  
    glm::mat3 normalMatrix;
    VkDeviceAddress scene;
};
static_assert(sizeof(PushBlock) == 128);

struct Material{
    glm::vec3 ambient;         
    glm::vec3 specular;  
    glm::vec3 diffuse;
    float shininess;
};
static_assert(sizeof(Material) == 40);

struct SceneData{
    glm::mat4 viewProj;
    glm::vec4 lightPos;
    glm::vec4 lightAmbient;
    glm::vec4 lightDiffuse;
    glm::vec4 lightSpecular;
    glm::vec4 viewPos;
};
static_assert(sizeof(SceneData) == 144);

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

static_assert(sizeof(Vertex) == 24, "scalar layout: two vec3 is 24 bytes");

static const std::array<Vertex, 36> kCubeVertices{{
    // back face, normal -Z
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},

    // front face, normal +Z
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},

    // left face, normal -X
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},

    // right face, normal +X
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},

    // bottom face, normal -Y
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},

    // top face, normal +Y
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},
}};



class MaterialsExample : public VulkanBase {
public:
    MaterialsExample() : VulkanBase(makeConfig()) {}

    ~MaterialsExample() override {
        VkDevice device = context().device();
        if (lampPipeline_) vkDestroyPipeline(device, lampPipeline_, nullptr);
        if (objectPipeline_) vkDestroyPipeline(device, objectPipeline_, nullptr);
    }

protected:

    void onFeatures(FeatureChain& chain) override {
       DescriptorHeap::requireFeature(chain);
    }

    std::array<VkDescriptorSetAndBindingMappingEXT,4> mappings(){
        std::array<VkDescriptorSetAndBindingMappingEXT, 4> m{};

        m[0].sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
        m[0].descriptorSet = 0;
        m[0].firstBinding  = 0;
        m[0].bindingCount  = 1;
        m[0].resourceMask  = VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT;
        m[0].source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;
        m[0].sourceData.constantOffset.heapOffset = verticesSlot_.offset;

        m[1].sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
        m[1].descriptorSet = 0;
        m[1].firstBinding  = 1;
        m[1].bindingCount  = 1;
        m[1].resourceMask  = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
        m[1].source        = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_DATA_EXT;
        m[1].sourceData.pushDataOffset = 0;
        
        m[2].sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
        m[2].descriptorSet = 0;
        m[2].firstBinding  = 2;
        m[2].bindingCount  = 1;
        m[2].resourceMask  = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
        m[2].source        = VK_DESCRIPTOR_MAPPING_SOURCE_PUSH_ADDRESS_EXT;
        m[2].sourceData.pushAddressOffset = offsetof(PushBlock, scene);

        m[3].sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
        m[3].descriptorSet = 0;
        m[3].firstBinding  = 3;
        m[3].bindingCount  = 1;
        m[3].resourceMask  = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
        m[3].source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;;
        m[3].sourceData.constantOffset.heapOffset = materialSlot_.offset;

        return m;
    }


    void onInit() override {
        camera_.setPosition({0.0f, 0.0f, 5.0f});
        camera_.setPerspective(45.0f, float(config().width) / float(config().height), 0.1f, 100.0f);


        vertices_ = VulkanResources::createBufferWithData(context(), kCubeVertices.data(), sizeof(kCubeVertices),
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        scene_.resize(config().framesInFlight);
        for (auto& b : scene_) {
              b = VulkanResources::createBuffer(context(), sizeof(SceneData),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VMA_MEMORY_USAGE_AUTO_PREFER_HOST, true);
        }


        Material mat{};
        mat.ambient  = glm::vec3(1.0f, 0.5f, 0.31f);
        mat.diffuse  = glm::vec3(1.0f, 0.5f, 0.31f);
        mat.specular = glm::vec3(0.5f, 0.5f, 0.5f);
        mat.shininess = 32.0f;

        material_ = VulkanResources::createBufferWithData(context(), &mat, sizeof(mat),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        heap_ = VulkanDescriptorHeap(context()); 
        verticesSlot_ = heap_.allocate(context(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        materialSlot_ = heap_.allocate(context(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        heap_.writeBuffer(verticesSlot_, vertices_.address(), vertices_.size());
        heap_.writeBuffer(materialSlot_, material_.address(), material_.size());
        SDL_Log("[vulkan] Heap size: %llu", heap_.size()/1024);
        const auto m = mappings();

        auto makePipeline = [&](const char* frag){
          return GraphicsPipelineBuilder(context().device())
                  .shaders(VulkanPipeline::loadSpirv(shaderPath("cube.vert.spv")),
                              VulkanPipeline::loadSpirv(shaderPath(frag)))
                  .colorFormat(swapchain().format())
                  .depthFormat(swapchain().depthFormat())
                  .depthTest(true)
                  .configure([&m](PipelineBuildContext& ctx){
                      DescriptorHeap::chainMappings(ctx, m.data(),static_cast<uint32_t>(m.size()));
                      })
                  .build(VK_NULL_HANDLE);
        };
        objectPipeline_ = makePipeline("object.frag.spv");
        lampPipeline_   = makePipeline("lamp.frag.spv"); 
    }

    void onUpdate(float dt) override { 
        camera_.update(dt); 
        time_ += dt;
        const float radius = 2.5f;
        lightPos_ = glm::vec3(std::cos(time_) * radius, -0.0f, std::sin(time_) * radius);

    } 
    
    void onResize(uint32_t w, uint32_t h) override {
        camera_.setAspect(float(w) / float(h));
    } 

    void onEvent(const SDL_Event& event) override {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT) {
            SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(event.button.windowID), true);
            looking_ = true;
        }
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT) {
            SDL_SetWindowRelativeMouseMode(SDL_GetWindowFromID(event.button.windowID), false);
            looking_ = false;
        }
        if (looking_ || event.type != SDL_EVENT_MOUSE_MOTION) camera_.handleEvent(event);
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
        
        
        heap_.bind(cmd);
        SceneData sd{};
        sd.viewProj     = camera_.viewProjection();
        sd.lightAmbient = glm::vec4(0.2f);
        sd.lightDiffuse = glm::vec4(0.5f);
        sd.lightSpecular= glm::vec4(1.f);
        
        sd.lightPos = glm::vec4(lightPos_,1.0f);
        sd.viewPos= glm::vec4(camera_.position(),1.0f);
        std::memcpy(scene_[frame.frameIndex].mapped(), &sd, sizeof(sd));
        
        
        PushBlock pb{};
        pb.scene = scene_[frame.frameIndex].address();
        VkPushDataInfoEXT push{VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT};
        push.offset       = 0;
        push.data.address = &pb;
        push.data.size    = sizeof(pb);

        pb.model       = glm::mat4(1.0f);
        pb.normalMatrix = glm::transpose(glm::inverse(glm::mat3(pb.model)));
        pb.objectColor = glm::vec4(1.0f, 0.5f, 0.31f, 1.0f);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, objectPipeline_);
        vkCmdPushDataEXT(cmd, &push);
        vkCmdDraw(cmd, 36, 1, 0, 0);
        
        pb.model = glm::scale(glm::translate(glm::mat4(1.0f), lightPos_), glm::vec3(0.2f));
        pb.normalMatrix = glm::transpose(glm::inverse(glm::mat3(pb.model)));
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lampPipeline_);
        vkCmdPushDataEXT(cmd, &push);
        vkCmdDraw(cmd, 36, 1, 0, 0);
        vkCmdEndRendering(cmd);
    }

private:
    static ExampleConfig makeConfig() {
        ExampleConfig cfg;
        cfg.name   = "04_materials_opengl";
        cfg.title  = "04 - materials example from learn opengl";
        cfg.width  = 1280;
        cfg.height = 720;
        cfg.depth  = true;
        cfg.forceValidation = true;
        return cfg;
    }

    VulkanBuffer              vertices_;
    VulkanCamera              camera_;
    bool                      looking_ = false;
    std::vector<VulkanBuffer> scene_;

    VulkanBuffer material_;
    VkPipeline   objectPipeline_  = VK_NULL_HANDLE;
    VkPipeline   lampPipeline_    = VK_NULL_HANDLE;
    DescriptorSlot materialSlot_;
    DescriptorSlot verticesSlot_; 
    
    VulkanDescriptorHeap heap_;
    float time_ = 0.0f;
    glm::vec3 lightPos_{0.0f};
};
int main(int, char*[]) {
    MaterialsExample app;
    return app.run();
}
