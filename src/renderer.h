#ifndef RENDERER_H_
#define RENDERER_H_

#include <memory>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "camera.h"
#include "device.h"
#include "layouts.h"
#include "material.h"
#include "mesh.h"
#include "object.h"
#include "render_passes.h"
#include "resource_manager.h"
#include "structures.h"
#include "texture.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    Camera& camera() {
        return camera_;
    }

    void Render();
private:
    struct SyncResources {
        vk::Semaphore image_available;
        vk::Semaphore render_finished;
        vk::Fence in_flight;
    } sync_resources_;

    void InitCommandBuffers();
    void InitCommandPool();
    void InitFramebuffers();
    void InitSyncResources();

    void InitSceneDescriptors();

    void UpdateSceneDescriptors();

    vk::CommandPool command_pool_;
    vk::CommandBuffer render_buffer_;
    vk::CommandBuffer transfer_buffer_;

    std::vector<vk::Framebuffer> swapchain_framebuffers_;
    std::unique_ptr<RenderPasses> render_passes_;
    std::unique_ptr<Layouts> layouts_;
    std::unique_ptr<ResourceManager> resource_manager_;

    std::vector<std::unique_ptr<Material>> materials_;
    std::vector<std::unique_ptr<Mesh>> meshes_;
    std::vector<Object> objects_;

    std::vector<InstanceData> instance_data_;
    ResourceManager::Buffer instance_data_buffer_;

    vk::DescriptorPool scene_descriptor_pool_;
    vk::DescriptorSet scene_descriptors_;
    ResourceManager::Buffer scene_uniform_buffer_;
    std::unique_ptr<Texture> scene_environment_map_;

    Camera camera_;
};

#endif  // RENDERER_H_
