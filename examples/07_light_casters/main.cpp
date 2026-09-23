#include "VulkanBase.h"
#include <array>
#include <cassert>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include "stb_image.h"
struct PushBlock {
    glm::mat4 model;         
    glm::vec4 objectColor;  
    glm::mat3 normalMatrix;
    VkDeviceAddress scene;
};

struct Material{
    float shininess;
};

struct SceneData{
    glm::mat4 viewProj;
    glm::vec4 lightPos;
    glm::vec4 lightAmbient;
    glm::vec4 lightDiffuse;
    glm::vec4 lightSpecular;
    glm::vec4 viewPos;
    float c;
    float l;
    float k;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};


static const std::array<Vertex, 36> kCubeVertices{{
    // back face, normal -Z
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},

    // front face, normal +Z
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},

    // left face, normal -X
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},

    // right face, normal +X
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},

    // bottom face, normal -Y
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},

    // top face, normal +Y
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
}};

static const std::array<glm::vec3, 10> kCubePositions{{
    { 0.0f,  0.0f,   0.0f},
    { 2.0f,  5.0f, -15.0f},
    {-1.5f, -2.2f,  -2.5f},
    {-3.8f, -2.0f, -12.3f},
    { 2.4f, -0.4f,  -3.5f},
    {-1.7f,  3.0f,  -7.5f},
    { 1.3f, -2.0f,  -2.5f},
    { 1.5f,  2.0f,  -2.5f},
    { 1.5f,  0.2f,  -1.5f},
    {-1.3f,  1.0f,  -1.5f},
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

    std::array<VkDescriptorSetAndBindingMappingEXT,6> mappings(){
        std::array<VkDescriptorSetAndBindingMappingEXT, 6> m{};

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

        m[4].sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
        m[4].descriptorSet = 0;
        m[4].firstBinding  = 4;
        m[4].bindingCount  = 1;
        m[4].resourceMask  = VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT;
        m[4].source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;;
        m[4].sourceData.constantOffset.heapOffset = textureSlot_.offset;
        m[4].sourceData.constantOffset.samplerHeapOffset= samplerSlot_.offset;

        m[5].sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
        m[5].descriptorSet = 0;
        m[5].firstBinding  = 5;
        m[5].bindingCount  = 1;
        m[5].resourceMask  = VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT;
        m[5].source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT;;
        m[5].sourceData.constantOffset.heapOffset = specularSlot_.offset;
        m[5].sourceData.constantOffset.samplerHeapOffset= samplerSlot_.offset;
        return m;
    }


    void onInit() override {
        if (!VulkanTextures::supportsLinearBlit(
            context().physicalDevice(),
            VK_FORMAT_R8G8B8A8_SRGB)) { throw std::runtime_error(
                "05_texture_mapping requires linear blit support "
                  "for its RGBA8 sRGB mipmaps");}



        stbi_set_flip_vertically_on_load(true);
        samplerHeap_ = VulkanSamplerHeap(context());
        samplerSlot_ = samplerHeap_.allocate(context());

        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter               = VK_FILTER_LINEAR;
        sampler.minFilter               = VK_FILTER_LINEAR;
        sampler.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.mipLodBias              = 0.0f;
        sampler.anisotropyEnable        = VK_FALSE;
        sampler.maxAnisotropy           = 1.0f;
        sampler.compareEnable           = VK_FALSE;
        sampler.minLod                  = 0.0f;
        sampler.maxLod                  = 0.0f;
        sampler.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        sampler.unnormalizedCoordinates = VK_FALSE;

        texture_ = VulkanTextures::loadRgba8(
            context(), "assets/container2.png", 
            TEXTURE_COLOR_SPACE::SRGB, MIP_MODE::GENERATE);
        specularTex_ = VulkanTextures::loadRgba8(
            context(), "assets/container2_specular.png", 
            TEXTURE_COLOR_SPACE::LINEAR, MIP_MODE::GENERATE);

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
        mat.shininess = 32.0f;

        material_ = VulkanResources::createBufferWithData(context(), &mat, sizeof(mat),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        heap_ = VulkanDescriptorHeap(context()); 
        verticesSlot_ = heap_.allocate(context(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        materialSlot_ = heap_.allocate(context(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        textureSlot_ = heap_.allocate(
            context(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
        specularSlot_ = heap_.allocate(context(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

        heap_.writeImage(specularSlot_,specularTex_.viewInfo(), specularTex_.layout());

        heap_.writeBuffer(verticesSlot_, vertices_.address(), vertices_.size());
        heap_.writeBuffer(materialSlot_, material_.address(), material_.size());
        
        heap_.writeImage( textureSlot_,texture_.viewInfo() , texture_.layout());

        sampler.maxLod = static_cast<float>(texture_.mipLevels() - 1);
        samplerHeap_.write(samplerSlot_, sampler);
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
        
        samplerHeap_.bind(cmd); 
        heap_.bind(cmd);
        SceneData sd{};
        sd.viewProj     = camera_.viewProjection();
        sd.lightAmbient = glm::vec4(0.2f);
        sd.lightDiffuse = glm::vec4(0.5f);
        sd.lightSpecular= glm::vec4(1.f);
        
        sd.lightPos = glm::vec4(1.2f, 1.0f, 2.0f, 1.0f);
        sd.c= 1.0f;
        sd.l= 0.09f;
        sd.k= 0.032f;
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

        for (size_t i = 0; i < kCubePositions.size(); ++i) {
            pb.model = glm::translate(glm::mat4(1.0f), kCubePositions[i]);

            const float angle = 20.0f * static_cast<float>(i);
            pb.model = glm::rotate( pb.model, glm::radians(angle),
                  glm::vec3(1.0f, 0.3f, 0.5f));

            pb.normalMatrix =
            glm::transpose(glm::inverse(glm::mat3(pb.model)));

            vkCmdPushDataEXT(cmd, &push);
            vkCmdDraw(cmd, 36, 1, 0, 0);
        }
        
        vkCmdEndRendering(cmd);
    }

private:
    static ExampleConfig makeConfig() {
        ExampleConfig cfg;
        cfg.name   = "07_light_casters";
        cfg.title  = "07 - light casters";
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

    DescriptorSlot textureSlot_;
    VulkanTexture  texture_;
    
    DescriptorSlot specularSlot_;
    VulkanTexture  specularTex_;

    SamplerSlot    samplerSlot_;
    VulkanSamplerHeap    samplerHeap_;
    VulkanDescriptorHeap heap_;
    float time_ = 0.0f;
};
int main(int, char*[]) {
    MaterialsExample app;
    return app.run();
}
