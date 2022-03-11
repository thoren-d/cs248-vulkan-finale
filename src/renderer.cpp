#include "renderer.h"

#include <array>
#include <iostream>

// Some Windows header file defines these >:(
#undef min
#undef max

Renderer::Renderer() {
  render_passes_ = std::make_unique<RenderPasses>();
  layouts_ = std::make_unique<Layouts>();

  InitFramebuffers();
  InitCommandPool();
  InitCommandBuffers();
  resource_manager_ = std::make_unique<ResourceManager>(transfer_buffer_);
  InitSyncResources();

  scene_environment_map_ = std::make_unique<Texture>(
      "C:/Users/thore/GoogleDrive/Documents/School/stanford-cs248/"
      "final-project/quattro_canti_4k.png");

  materials_.push_back(std::make_unique<OpaqueMaterial>(
      "C:/Users/thore/GoogleDrive/Documents/School/stanford-cs248/"
      "final-project/assets/Stone_Tiles_003_COLOR.png",
      "C:/Users/thore/GoogleDrive/Documents/School/stanford-cs248/"
      "final-project/assets/Stone_Tiles_003_NORM.png",
      1.5f, 0.09f, 0.0f));
  materials_.push_back(std::make_unique<OpaqueMaterial>(
      "C:/Users/thore/GoogleDrive/Documents/School/stanford-cs248/"
      "final-project/assets/Blue_Marble_002_COLOR.png",
      "C:/Users/thore/GoogleDrive/Documents/School/stanford-cs248/"
      "final-project/assets/Blue_Marble_002_NORM.png",
      3.5f, 0.03f, 0.0f));

  meshes_.push_back(
      std::make_unique<Mesh>("C:/Users/thore/GoogleDrive/Documents/School/"
                             "stanford-cs248/final-project/plane.obj"));
  meshes_.push_back(
      std::make_unique<Mesh>("C:/Users/thore/GoogleDrive/Documents/School/"
                             "stanford-cs248/final-project/teapot.obj"));

  // Plane
  objects_.push_back(Object(materials_[0].get(), meshes_[0].get()));
  objects_[0].position() = glm::vec3(0.0f, -0.417f, 0.0f);

  // Teapot
  objects_.push_back(Object(materials_[1].get(), meshes_[1].get()));

  InitSceneDescriptors();
}

Renderer::~Renderer() {
  vk::Device d = Device::Get()->device();
  d.waitIdle();

  d.destroyDescriptorPool(scene_descriptor_pool_);
  for (auto framebuffer : swapchain_framebuffers_) {
    d.destroyFramebuffer(framebuffer);
  }
  d.destroyCommandPool(command_pool_);
  d.destroySemaphore(sync_resources_.image_available);
  d.destroySemaphore(sync_resources_.render_finished);
  d.destroyFence(sync_resources_.in_flight);
}

void Renderer::InitFramebuffers() {
  auto image_views = Device::Get()->swapchain_image_views();
  auto swap_extent = Device::Get()->swapchain_extent();
  for (size_t i = 0; i < image_views.size(); i++) {
    vk::ImageView attachments[] = {
        image_views[i],
    };

    vk::FramebufferCreateInfo create_info;
    create_info.setAttachmentCount(1)
        .setPAttachments(attachments)
        .setWidth(swap_extent.width)
        .setHeight(swap_extent.height)
        .setRenderPass(render_passes_->GetRenderPass(RenderPass::Opaque))
        .setLayers(1);

    swapchain_framebuffers_.push_back(
        Device::Get()->device().createFramebuffer(create_info));
  }
}

void Renderer::InitSyncResources() {
  vk::SemaphoreCreateInfo semaphore_create_info;
  vk::FenceCreateInfo fence_create_info;
  fence_create_info.setFlags(vk::FenceCreateFlagBits::eSignaled);

  sync_resources_.image_available =
      Device::Get()->device().createSemaphore(semaphore_create_info);
  sync_resources_.render_finished =
      Device::Get()->device().createSemaphore(semaphore_create_info);
  sync_resources_.in_flight =
      Device::Get()->device().createFence(fence_create_info);
}

void Renderer::InitCommandPool() {
  vk::CommandPoolCreateInfo create_info;
  create_info.setQueueFamilyIndex(Device::Get()->graphics_queue_family())
      .setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
                vk::CommandPoolCreateFlagBits::eTransient);

  command_pool_ = Device::Get()->device().createCommandPool(create_info);
}

void Renderer::InitCommandBuffers() {
  vk::CommandBufferAllocateInfo alloc_info;
  alloc_info.setCommandPool(command_pool_)
      .setCommandBufferCount(2)
      .setLevel(vk::CommandBufferLevel::ePrimary);

  vk::CommandBuffer buffers[2] = {nullptr, nullptr};
  if (Device::Get()->device().allocateCommandBuffers(&alloc_info, buffers) !=
      vk::Result::eSuccess)
    throw "Error allocating command buffers.";

  render_buffer_ = buffers[0];
  transfer_buffer_ = buffers[1];
}

void Renderer::InitSceneDescriptors() {
  auto ubo_size = vk::DescriptorPoolSize().setDescriptorCount(1).setType(
      vk::DescriptorType::eUniformBuffer);
  auto sampler_size = vk::DescriptorPoolSize().setDescriptorCount(1).setType(
      vk::DescriptorType::eCombinedImageSampler);

  std::array<vk::DescriptorPoolSize, 2> sizes = {ubo_size, sampler_size};
  auto pool_info = vk::DescriptorPoolCreateInfo()
                       .setPoolSizeCount(static_cast<uint32_t>(sizes.size()))
                       .setPPoolSizes(sizes.data())
                       .setMaxSets(1);
  scene_descriptor_pool_ =
      Device::Get()->device().createDescriptorPool(pool_info);

  vk::DescriptorSetLayout layout = layouts_->scene_dsl();

  auto alloc_info = vk::DescriptorSetAllocateInfo()
                        .setDescriptorPool(scene_descriptor_pool_)
                        .setDescriptorSetCount(1)
                        .setPSetLayouts(&layout);
  scene_descriptors_ =
      Device::Get()->device().allocateDescriptorSets(alloc_info)[0];
}

void Renderer::Render() {
  if (Device::Get()->device().waitForFences(
          {sync_resources_.in_flight}, true,
          std::numeric_limits<uint64_t>::max()) != vk::Result::eSuccess)
    throw "Error waiting for fences.";
  Device::Get()->device().resetFences({sync_resources_.in_flight});

  resource_manager_->WaitForTransfers();

  UpdateSceneDescriptors();

  render_buffer_.reset({});

  vk::ResultValue<uint32_t> acquire_res =
      Device::Get()->device().acquireNextImageKHR(
          Device::Get()->swapchain(), std::numeric_limits<uint64_t>::max(),
          sync_resources_.image_available, nullptr);
  if (acquire_res.result != vk::Result::eSuccess) {
    throw "Failed to acquire next swap image.";
  }
  uint32_t image_idx = acquire_res.value;

  vk::CommandBufferBeginInfo begin_info;
  render_buffer_.begin(begin_info);

  // TODO: Begin shadow pass

  // Begin opaque pass
  vk::RenderPassBeginInfo opaque_begin_info;
  opaque_begin_info
      .setRenderPass(render_passes_->GetRenderPass(RenderPass::Opaque))
      .setFramebuffer(swapchain_framebuffers_[image_idx])
      .setRenderArea(vk::Rect2D({0, 0}, Device::Get()->swapchain_extent()));
  vk::ClearValue clear_color;
  clear_color.setColor(
      vk::ClearColorValue(std::array<float, 4>({0.0f, 0.0f, 1.0f, 1.0f})));
  opaque_begin_info.setClearValueCount(1).setPClearValues(&clear_color);

  render_buffer_.beginRenderPass(opaque_begin_info,
                                 vk::SubpassContents::eInline);

  PushConstants push_constants;
  push_constants.view_proj = camera_.GetViewProj();

  // Update instance data.
  instance_data_.clear();
  instance_data_.reserve(objects_.size());
  for (auto &material : materials_) {
    for (auto &mesh : meshes_) {
      for (auto &object : objects_) {
        if (object.material() == material.get() &&
            object.mesh() == mesh.get()) {
          instance_data_.push_back(object.GetInstanceData());
        }
      }
    }
  }
  instance_data_buffer_ = resource_manager_->CreateHostBufferWithData(
      vk::BufferUsageFlagBits::eVertexBuffer, instance_data_.data(),
      sizeof(InstanceData) * instance_data_.size());

  uint32_t instance_offset = 0;
  vk::Pipeline pipeline = nullptr;
  for (auto &material : materials_) {
    vk::Pipeline new_pipeline =
        material->GetPipelineForRenderPass(RenderPass::Opaque);
    vk::PipelineLayout layout =
        material->GetPipelineLayoutForRenderPass(RenderPass::Opaque);
    vk::DescriptorSet material_descriptors =
        material->GetMaterialDescriptorSetForRenderPass(RenderPass::Opaque);
    render_buffer_.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, layout, 0,
        {scene_descriptors_, material_descriptors}, {});
    if (pipeline != new_pipeline) {
      pipeline = new_pipeline;
      render_buffer_.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
      render_buffer_.pushConstants(layout,
                                   vk::ShaderStageFlagBits::eVertex |
                                       vk::ShaderStageFlagBits::eFragment,
                                   0, sizeof(PushConstants), &push_constants);
    }


    for (auto &mesh : meshes_) {
      render_buffer_.bindVertexBuffers(
          0, {mesh->vertex_buffer(), instance_data_buffer_.buffer},
          {0, instance_offset * sizeof(InstanceData)});
      uint32_t num_instances = 0;
      for (auto &object : objects_) {
        if (object.material() == material.get() &&
            object.mesh() == mesh.get()) {
          num_instances += 1;
        }
      }
      render_buffer_.draw(static_cast<uint32_t>(mesh->vertex_count()),
                          num_instances, 0, 0);

      instance_offset += num_instances;
    }
  }

  render_buffer_.endRenderPass();

  render_buffer_.end();

  // Submit render work
  vk::SubmitInfo submit_info;
  vk::Semaphore wait_semaphores[] = {sync_resources_.image_available};
  vk::PipelineStageFlags wait_stages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  vk::Semaphore signal_semaphores[] = {sync_resources_.render_finished};
  submit_info.setCommandBufferCount(1)
      .setPCommandBuffers(&render_buffer_)
      .setWaitSemaphoreCount(1)
      .setPWaitSemaphores(wait_semaphores)
      .setPWaitDstStageMask(wait_stages)
      .setSignalSemaphoreCount(1)
      .setPSignalSemaphores(signal_semaphores);

  Device::Get()->graphics_queue().submit(submit_info,
                                         sync_resources_.in_flight);

  // Present!
  vk::PresentInfoKHR present_info;
  vk::SwapchainKHR swapchain = Device::Get()->swapchain();
  present_info.setSwapchainCount(1)
      .setPSwapchains(&swapchain)
      .setPImageIndices(&image_idx)
      .setWaitSemaphoreCount(1)
      .setPWaitSemaphores(signal_semaphores);
  if (Device::Get()->present_queue().presentKHR(present_info) !=
      vk::Result::eSuccess)
    throw "Error on present.";
}

void Renderer::UpdateSceneDescriptors() {
  SceneUniforms data;
  data.camera_position = camera_.position;
  data.light_positions[0] = glm::vec4(-3.0f, 3.0f, -3.0f, 0.0f);
  data.light_positions[1] = glm::vec4(-3.0f, 3.0f, 3.0f, 0.0f);
  data.light_positions[2] = glm::vec4(3.0f, 0.0f, 3.0f, 0.0f);
  data.light_intensities[0] = glm::vec4(0.2f, 0.2f, 1.0f, 0.0f) * 0.25f * 10.0f;
  data.light_intensities[1] = glm::vec4(1.0f) * 1.0f * 10.0f;
  data.light_intensities[2] = glm::vec4(1.0f, 0.4f, 0.4f, 0.0f) * 0.5f * 10.0f;
  constexpr float kAngles[3] = {6.0f, 9.0f, 7.0f};
  for (int i = 0; i < 3; i++) {
    data.light_directions_angles[i] =
        glm::vec4(glm::normalize(-glm::vec3(data.light_positions[i])),
                  glm::radians(kAngles[i]));
  }

  if (scene_uniform_buffer_.buffer) {
    resource_manager_->UpdateHostBufferData(scene_uniform_buffer_, &data,
                                            sizeof(SceneUniforms));
  } else {
    scene_uniform_buffer_ = resource_manager_->CreateHostBufferWithData(
        vk::BufferUsageFlagBits::eUniformBuffer, &data, sizeof(SceneUniforms));
  }

  auto buffer_info = vk::DescriptorBufferInfo()
                         .setBuffer(scene_uniform_buffer_.buffer)
                         .setOffset(0)
                         .setRange(scene_uniform_buffer_.size);
  auto ubo_write = vk::WriteDescriptorSet()
                       .setDescriptorCount(1)
                       .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                       .setDstSet(scene_descriptors_)
                       .setDstBinding(0)
                       .setDstArrayElement(0)
                       .setPBufferInfo(&buffer_info);

  auto sampler_info =
      vk::DescriptorImageInfo()
          .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
          .setImageView(scene_environment_map_->image_view())
          .setSampler(scene_environment_map_->sampler());
  auto sampler_write =
      vk::WriteDescriptorSet()
          .setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setDstSet(scene_descriptors_)
          .setDstBinding(1)
          .setDstArrayElement(0)
          .setPImageInfo(&sampler_info);
  Device::Get()->device().updateDescriptorSets({ubo_write, sampler_write}, {});
}
