#include "renderer.h"

#include <array>
#include <iostream>

#include "constants.h"

// Some Windows header file defines these >:(
#undef min
#undef max

Renderer::Renderer() {
  render_passes_ = std::make_unique<RenderPasses>();
  layouts_ = std::make_unique<Layouts>();

  InitCommandPool();
  InitCommandBuffers();
  resource_manager_ = std::make_unique<ResourceManager>(transfer_buffer_);
  InitDepthBuffer();
  InitFramebuffers();
  InitShadowMaps();
  InitSyncResources();

  scene_environment_map_ = std::make_unique<Texture>(
      "../../../assets/quattro_canti_4k.hdr", Texture::Usage::HDRI);
  scene_irradiance_map_ = std::make_unique<Texture>(
      "../../../assets/quattro_canti_irradiance_2k.hdr", Texture::Usage::HDRI);

  // Lights
  Light back{
      glm::mat4(1.0f),
      glm::vec4(-glm::normalize(glm::vec3(-3.0f, 3.0f, -3.0f)),
                glm::radians(12.0f)),
      glm::vec3(0.2f, 0.2f, 1.0f) * 0.35f * 10.0f,
      glm::vec3(-3.0f, 3.0f, -3.0f),
  };
  Light key{
      glm::mat4(1.0f),
      glm::vec4(-glm::normalize(glm::vec3(-3.0f, 3.0f, 3.0f)),
                glm::radians(9.0f)),
      glm::vec3(1.0f) * 1.0f * 10.0f,
      glm::vec3(-3.0f, 3.0f, 3.0f),
  };
  Light fill{
      glm::mat4(1.0f),
      glm::vec4(-glm::normalize(glm::vec3(3.0f, 1.5f, 3.0f)),
                glm::radians(15.0f)),
      glm::vec3(1.0f, 0.4f, 0.4f) * 0.75f * 10.0f,
      glm::vec3(3.0f, 1.5f, 3.0f),
  };
  lights_[0] = back;
  lights_[1] = key;
  lights_[2] = fill;

  InitSceneDescriptors();

  sky_pipeline_ = GetSkyPipeline();
}

Renderer::~Renderer() {
  vk::Device d = Device::Get()->device();
  d.waitIdle();

  d.destroyPipeline(sky_pipeline_);
  d.destroyDescriptorPool(scene_descriptor_pool_);
  for (auto &shadow_map : shadow_maps_) {
    d.destroySampler(shadow_map.sampler);
    d.destroyFramebuffer(shadow_map.framebuffer);
    d.destroyImageView(shadow_map.image_view);
  }
  for (auto framebuffer : swapchain_framebuffers_) {
    d.destroyFramebuffer(framebuffer);
  }
  d.destroyImageView(depth_buffer_view_);
  d.destroyCommandPool(command_pool_);
  d.destroySemaphore(sync_resources_.image_available);
  d.destroySemaphore(sync_resources_.render_finished);
  d.destroyFence(sync_resources_.in_flight);
}

void Renderer::InitFramebuffers() {
  auto image_views = Device::Get()->swapchain_image_views();
  auto swap_extent = Device::Get()->swapchain_extent();
  for (size_t i = 0; i < image_views.size(); i++) {
    std::array<vk::ImageView, 2> attachments = {
        image_views[i],
        depth_buffer_view_,
    };

    auto create_info =
        vk::FramebufferCreateInfo()
            .setAttachments(attachments)
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

void Renderer::InitDepthBuffer() {
  vk::Format depth_format = vk::Format::eD32Sfloat;
  vk::Extent2D swap_extent = Device::Get()->swapchain_extent();

  depth_buffer_image_ = resource_manager_->CreateImageUninitialized(
      vk::ImageUsageFlagBits::eDepthStencilAttachment, depth_format,
      swap_extent.width, swap_extent.height);

  auto view_create_info =
      vk::ImageViewCreateInfo()
          .setViewType(vk::ImageViewType::e2D)
          .setFormat(depth_format)
          .setComponents({})
          .setImage(depth_buffer_image_.image)
          .setSubresourceRange(
              vk::ImageSubresourceRange()
                  .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                  .setBaseArrayLayer(0)
                  .setBaseMipLevel(0)
                  .setLayerCount(1)
                  .setLevelCount(1));
  depth_buffer_view_ =
      Device::Get()->device().createImageView(view_create_info);
}

void Renderer::InitShadowMaps() {
  for (int i = 0; i < NUM_LIGHTS; i++) {
    vk::Format format = vk::Format::eD32Sfloat;
    shadow_maps_[i].image = resource_manager_->CreateImageUninitialized(
        vk::ImageUsageFlagBits::eDepthStencilAttachment |
            vk::ImageUsageFlagBits::eSampled,
        format, kShadowMapSize, kShadowMapSize);

    auto view_create_info =
        vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(format)
            .setComponents({})
            .setImage(shadow_maps_[i].image.image)
            .setSubresourceRange(
                vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eDepth)
                    .setBaseArrayLayer(0)
                    .setBaseMipLevel(0)
                    .setLayerCount(1)
                    .setLevelCount(1));
    shadow_maps_[i].image_view =
        Device::Get()->device().createImageView(view_create_info);

    auto framebuffer_info =
        vk::FramebufferCreateInfo()
            .setAttachments(shadow_maps_[i].image_view)
            .setRenderPass(render_passes_->GetRenderPass(RenderPass::Shadow))
            .setLayers(1)
            .setWidth(kShadowMapSize)
            .setHeight(kShadowMapSize);
    shadow_maps_[i].framebuffer =
        Device::Get()->device().createFramebuffer(framebuffer_info);

    auto sampler_info =
        vk::SamplerCreateInfo()
            .setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
            .setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
            .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
            .setMagFilter(vk::Filter::eNearest)
            .setMinFilter(vk::Filter::eNearest)
            .setMipmapMode(vk::SamplerMipmapMode::eNearest)
            .setCompareEnable(false)
            .setAnisotropyEnable(false);
    shadow_maps_[i].sampler =
        Device::Get()->device().createSampler(sampler_info);
  }
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
  auto sampler_size = vk::DescriptorPoolSize()
                          .setDescriptorCount(1 + NUM_LIGHTS)
                          .setType(vk::DescriptorType::eCombinedImageSampler);

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

  // Update instance data.
  instance_data_.clear();
  instance_data_.reserve(objects_.size());
  for (auto &material : materials_) {
    for (auto &mesh : meshes_) {
      for (auto &object : objects_) {
        if (object->material() == material.get() &&
            object->mesh() == mesh.get()) {
          instance_data_.push_back(object->GetInstanceData());
        }
      }
    }
  }
  instance_data_buffer_ = resource_manager_->CreateHostBufferWithData(
      vk::BufferUsageFlagBits::eVertexBuffer, instance_data_.data(),
      sizeof(InstanceData) * instance_data_.size());

  // Begin shadow pass
  for (int i = 0; i < NUM_LIGHTS; i++) {
    auto shadow_pass_begin_info =
        vk::RenderPassBeginInfo()
            .setRenderPass(render_passes_->GetRenderPass(RenderPass::Shadow))
            .setFramebuffer(shadow_maps_[i].framebuffer)
            .setRenderArea({{0, 0}, {kShadowMapSize, kShadowMapSize}})
            .setClearValues(vk::ClearValue().setDepthStencil(
                vk::ClearDepthStencilValue(1.0f, 0)));
    render_buffer_.beginRenderPass(shadow_pass_begin_info,
                                   vk::SubpassContents::eInline);

    Draw(RenderPass::Shadow, lights_[i].world2light);

    render_buffer_.endRenderPass();
  }

  // Begin opaque pass
  vk::RenderPassBeginInfo opaque_begin_info;
  opaque_begin_info
      .setRenderPass(render_passes_->GetRenderPass(RenderPass::Opaque))
      .setFramebuffer(swapchain_framebuffers_[image_idx])
      .setRenderArea(vk::Rect2D({0, 0}, Device::Get()->swapchain_extent()));
  vk::ClearValue clear_color;
  clear_color.setColor(
      vk::ClearColorValue(std::array<float, 4>({0.0f, 0.0f, 1.0f, 1.0f})));
  auto clear_depth =
      vk::ClearValue().setDepthStencil(vk::ClearDepthStencilValue(1.0f, 0));
  std::array<vk::ClearValue, 2> clear_values = {clear_color, clear_depth};
  opaque_begin_info.setClearValues(clear_values);

  render_buffer_.beginRenderPass(opaque_begin_info,
                                 vk::SubpassContents::eInline);

  // Draw normal objects
  Draw(RenderPass::Opaque, camera_.GetViewProj());

  // Draw sky
  PushConstants push_constants;
  push_constants.view_proj = glm::inverse(camera_.GetViewProj());
  render_buffer_.bindPipeline(vk::PipelineBindPoint::eGraphics, sky_pipeline_);
  render_buffer_.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                    layouts_->sky_pipeline_layout(), 0,
                                    scene_descriptors_, {});
  render_buffer_.pushConstants(
      layouts_->sky_pipeline_layout(), vk::ShaderStageFlagBits::eVertex, 0,
      static_cast<uint32_t>(sizeof(PushConstants)), &push_constants);
  render_buffer_.draw(6, 1, 0, 0);

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

void Renderer::Draw(RenderPass pass, glm::mat4 view_proj) {
  PushConstants push_constants;
  push_constants.view_proj = view_proj;

  uint32_t instance_offset = 0;
  vk::Pipeline pipeline = nullptr;
  for (auto &material : materials_) {
    vk::Pipeline new_pipeline = material->GetPipelineForRenderPass(pass);
    vk::PipelineLayout layout = material->GetPipelineLayoutForRenderPass(pass);
    vk::DescriptorSet material_descriptors =
        material->GetMaterialDescriptorSetForRenderPass(pass);

    // Shadow pass only uses push constants and instance data.
    if (pass != RenderPass::Shadow) {
      render_buffer_.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, layout, 0,
          {scene_descriptors_, material_descriptors}, {});
    }

    if (pipeline != new_pipeline) {
      pipeline = new_pipeline;
      render_buffer_.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
      render_buffer_.pushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0,
                                   sizeof(PushConstants), &push_constants);
    }

    for (auto &mesh : meshes_) {
      uint32_t num_instances = 0;
      for (auto &object : objects_) {
        if (object->material() == material.get() &&
            object->mesh() == mesh.get()) {
          num_instances += 1;
        }
      }

      if (num_instances == 0) {
        continue;
      }

      render_buffer_.bindVertexBuffers(
          0, {mesh->vertex_buffer(), instance_data_buffer_.buffer},
          {0, instance_offset * sizeof(InstanceData)});
      render_buffer_.draw(static_cast<uint32_t>(mesh->vertex_count()),
                          num_instances, 0, 0);

      instance_offset += num_instances;
    }
  }
}

void Renderer::UpdateSceneDescriptors() {
  SceneUniforms data;
  data.camera_position = camera_.position;
  for (int i = 0; i < NUM_LIGHTS; i++) {
    glm::mat4 view =
        glm::lookAt(lights_[i].position,
                    lights_[i].position + glm::vec3(lights_[i].direction_angle),
                    glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspectiveFov(2.0f * lights_[i].direction_angle.w,
                                         1.0f, 1.0f, 0.5f, 10.0f);
    proj[1][1] *= -1.0f;
    lights_[i].world2light = proj * view;

    data.lights[i] = lights_[i];
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

  auto env_info = vk::DescriptorImageInfo()
                      .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                      .setImageView(scene_environment_map_->image_view())
                      .setSampler(scene_environment_map_->sampler());
  auto env_write =
      vk::WriteDescriptorSet()
          .setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setDstSet(scene_descriptors_)
          .setDstBinding(1)
          .setDstArrayElement(0)
          .setPImageInfo(&env_info);

  auto irr_info = vk::DescriptorImageInfo()
    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
    .setImageView(scene_irradiance_map_->image_view())
    .setSampler(scene_irradiance_map_->sampler());
  auto irr_write =
      vk::WriteDescriptorSet()
          .setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setDstSet(scene_descriptors_)
          .setDstBinding(3)
          .setDstArrayElement(0)
          .setPImageInfo(&irr_info);

  std::array<vk::DescriptorImageInfo, NUM_LIGHTS> shadow_map_infos = {};
  for (int i = 0; i < NUM_LIGHTS; i++) {
    shadow_map_infos[i] =
        vk::DescriptorImageInfo()
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImageView(shadow_maps_[i].image_view)
            .setSampler(shadow_maps_[i].sampler);
  }
  auto shadow_maps_write =
      vk::WriteDescriptorSet()
          .setDescriptorCount(NUM_LIGHTS)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setDstSet(scene_descriptors_)
          .setDstBinding(2)
          .setDstArrayElement(0)
          .setImageInfo(shadow_map_infos);

  Device::Get()->device().updateDescriptorSets(
      {ubo_write, env_write, shadow_maps_write, irr_write}, {});
}

Material *Renderer::AddMaterial(std::unique_ptr<Material> material) {
  Material *res = material.get();
  materials_.emplace_back(std::move(material));
  return res;
}

Mesh *Renderer::AddMesh(const std::string &mesh) {
  std::unique_ptr<Mesh> m = std::make_unique<Mesh>(mesh);
  Mesh *res = m.get();
  meshes_.emplace_back(std::move(m));
  return res;
}

Object *Renderer::AddObject(Mesh *mesh, Material *material) {
  std::unique_ptr<Object> o = std::make_unique<Object>(material, mesh);
  Object *res = o.get();
  objects_.emplace_back(std::move(o));
  return res;
}
