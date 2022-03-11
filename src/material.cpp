#include "material.h"

#include <array>
#include <fstream>
#include <iostream>
#include <tuple>
#include <vector>

#include "device.h"
#include "layouts.h"
#include "mesh.h"
#include "structures.h"

namespace {

// Shader modules come in groups of 32-bits, so might as well read the file
// in that way, so it's aligned properly.
std::vector<uint32_t> ReadShaderFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    throw "Failed to open shader file.";
  }

  size_t size = static_cast<size_t>(file.tellg());
  if (size % 4 != 0) {
    throw "Shader file was not a multiple of 4 bytes...";
  }
  std::vector<uint32_t> buffer(size / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()), size);
  file.close();

  return buffer;
}

vk::ShaderModule CreateShaderModule(const std::string &filename) {
  std::vector<uint32_t> code = ReadShaderFile(filename);

  vk::ShaderModuleCreateInfo create_info;
  create_info.setCodeSize(code.size() * sizeof(uint32_t)).setPCode(code.data());

  return Device::Get()->device().createShaderModule(create_info);
}

vk::PipelineShaderStageCreateInfo
GetShaderStageCreateInfo(vk::ShaderStageFlagBits stage,
                         vk::ShaderModule module) {
  vk::PipelineShaderStageCreateInfo create_info;
  create_info.setModule(module).setPName("main").setStage(stage);
  return create_info;
}

} // namespace

std::weak_ptr<OpaqueMaterial::Pipelines> OpaqueMaterial::s_pipelines_{};

OpaqueMaterial::OpaqueMaterial(const std::string &diffuse_map,
                               const std::string &normal_map, float ior,
                               float roughness, float metalness)
    : pipelines_(GetPipelines()) {
  diffuse_map_ = std::make_unique<Texture>(diffuse_map);
  normal_map_ = std::make_unique<Texture>(normal_map);
  params.ior = ior;
  params.roughness = roughness;
  params.metalness = metalness;

  material_layout_ = Layouts::Get()->material_dsl();

  std::array<vk::DescriptorPoolSize, 2> pool_sizes = {
      vk::DescriptorPoolSize()
          .setType(vk::DescriptorType::eUniformBuffer)
          .setDescriptorCount(1),
      vk::DescriptorPoolSize()
          .setType(vk::DescriptorType::eCombinedImageSampler)
          .setDescriptorCount(2),
  };

  auto pool_info =
      vk::DescriptorPoolCreateInfo().setPoolSizes(pool_sizes).setMaxSets(1);
  descriptor_pool_ = Device::Get()->device().createDescriptorPool(pool_info);

  auto alloc_info = vk::DescriptorSetAllocateInfo()
                        .setDescriptorPool(descriptor_pool_)
                        .setSetLayouts(material_layout_)
                        .setDescriptorSetCount(1);
  descriptor_set_ =
      Device::Get()->device().allocateDescriptorSets(alloc_info)[0];

  uniform_buffer_ = ResourceManager::Get()->CreateDeviceBufferWithData(
      vk::BufferUsageFlagBits::eUniformBuffer, &params,
      sizeof(MaterialUniforms));

  auto buffer_info = vk::DescriptorBufferInfo()
                         .setBuffer(uniform_buffer_.buffer)
                         .setOffset(0)
                         .setRange(sizeof(MaterialUniforms));
  auto diffuse_info =
      vk::DescriptorImageInfo()
          .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
          .setImageView(diffuse_map_->image_view())
          .setSampler(diffuse_map_->sampler());
  auto normal_info =
      vk::DescriptorImageInfo()
          .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
          .setImageView(normal_map_->image_view())
          .setSampler(normal_map_->sampler());

  std::array<vk::WriteDescriptorSet, 3> writes = {
      vk::WriteDescriptorSet()
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setDescriptorCount(1)
          .setDstSet(descriptor_set_)
          .setDstBinding(0)
          .setDstArrayElement(0)
          .setPBufferInfo(&buffer_info),
      vk::WriteDescriptorSet()
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setDescriptorCount(1)
          .setDstSet(descriptor_set_)
          .setDstBinding(1)
          .setDstArrayElement(0)
          .setPImageInfo(&diffuse_info),
      vk::WriteDescriptorSet()
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setDescriptorCount(1)
          .setDstSet(descriptor_set_)
          .setDstBinding(2)
          .setDstArrayElement(0)
          .setPImageInfo(&normal_info),
  };
  Device::Get()->device().updateDescriptorSets(writes, {});
}

OpaqueMaterial::~OpaqueMaterial() {
  Device::Get()->device().destroyDescriptorPool(descriptor_pool_);
}

OpaqueMaterial::Pipelines::Pipelines() {
  auto vert = CreateShaderModule("./basic.vert.spv");
  auto frag = CreateShaderModule("./basic.frag.spv");

  auto vert_stage =
      GetShaderStageCreateInfo(vk::ShaderStageFlagBits::eVertex, vert);
  auto frag_stage =
      GetShaderStageCreateInfo(vk::ShaderStageFlagBits::eFragment, frag);

  vk::PipelineShaderStageCreateInfo shader_stages[] = {vert_stage, frag_stage};

  vk::PipelineVertexInputStateCreateInfo vertex_input;
  auto vertex_bindings = GetVertexInputBindingDescriptions();
  auto vertex_attributes = GetVertexInputAttributeDescriptions();

  vertex_input
      .setVertexBindingDescriptionCount(
          static_cast<uint32_t>(vertex_bindings.size()))
      .setPVertexBindingDescriptions(vertex_bindings.data())
      .setVertexAttributeDescriptionCount(
          static_cast<uint32_t>(vertex_attributes.size()))
      .setPVertexAttributeDescriptions(vertex_attributes.data());

  vk::PipelineInputAssemblyStateCreateInfo input_assembly;
  input_assembly.setTopology(vk::PrimitiveTopology::eTriangleList)
      .setPrimitiveRestartEnable(false);

  vk::Extent2D swapchain_extent = Device::Get()->swapchain_extent();

  vk::Viewport viewport(0.0f, 0.0f, (float)swapchain_extent.width,
                        (float)swapchain_extent.height, 0.0f, 1.0f);
  vk::Rect2D scissor_rect{};
  scissor_rect.offset = vk::Offset2D{0, 0};
  scissor_rect.extent = swapchain_extent;

  vk::PipelineViewportStateCreateInfo viewport_state;
  viewport_state.setViewportCount(1)
      .setPViewports(&viewport)
      .setScissorCount(1)
      .setPScissors(&scissor_rect);

  vk::PipelineRasterizationStateCreateInfo rasterizer_state;
  rasterizer_state.setDepthClampEnable(false)
      .setRasterizerDiscardEnable(false)
      .setPolygonMode(vk::PolygonMode::eFill)
      .setLineWidth(1.0f)
      .setCullMode(vk::CullModeFlagBits::eBack)
      .setFrontFace(vk::FrontFace::eClockwise)
      .setDepthBiasEnable(false);

  vk::PipelineMultisampleStateCreateInfo multisample_state;
  multisample_state.setSampleShadingEnable(false).setRasterizationSamples(
      vk::SampleCountFlagBits::e1);

  vk::PipelineColorBlendAttachmentState color_blend_attachment;
  color_blend_attachment
      .setColorWriteMask(
          vk::ColorComponentFlagBits::eA | vk::ColorComponentFlagBits::eR |
          vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB)
      .setBlendEnable(false);

  vk::PipelineColorBlendStateCreateInfo color_blend_state;
  color_blend_state.setLogicOpEnable(false)
      .setAttachmentCount(1)
      .setPAttachments(&color_blend_attachment);

  vk::GraphicsPipelineCreateInfo create_info;
  create_info.setStageCount(2)
      .setPStages(shader_stages)
      .setPVertexInputState(&vertex_input)
      .setPInputAssemblyState(&input_assembly)
      .setPViewportState(&viewport_state)
      .setPRasterizationState(&rasterizer_state)
      .setPMultisampleState(&multisample_state)
      .setPColorBlendState(&color_blend_state)
      .setLayout(Layouts::Get()->general_pipeline_layout())
      .setRenderPass(RenderPasses::Get()->GetRenderPass(RenderPass::Opaque))
      .setSubpass(0);

  opaque_pass = Device::Get()
                    ->device()
                    .createGraphicsPipeline(nullptr, create_info)
                    .value;

  Device::Get()->device().destroyShaderModule(vert);
  Device::Get()->device().destroyShaderModule(frag);
}

OpaqueMaterial::Pipelines::~Pipelines() {
  Device::Get()->device().destroyPipeline(opaque_pass);
}

std::shared_ptr<OpaqueMaterial::Pipelines> OpaqueMaterial::GetPipelines() {
  std::shared_ptr<Pipelines> res =
      s_pipelines_.expired() ? nullptr : s_pipelines_.lock();
  if (res) {
    return res;
  }

  auto pipelines = std::make_shared<Pipelines>();
  s_pipelines_ = pipelines;
  return pipelines;
}

vk::Pipeline OpaqueMaterial::GetPipelineForRenderPass(RenderPass pass) {
  if (pass == RenderPass::Opaque) {
    return pipelines_->opaque_pass;
  } else if (pass == RenderPass::Shadow) {
    return pipelines_->shadow_pass;
  }
  return nullptr;
}

vk::PipelineLayout
OpaqueMaterial::GetPipelineLayoutForRenderPass(RenderPass pass) {
  if (pass == RenderPass::Opaque) {
    return Layouts::Get()->general_pipeline_layout();
  } else if (pass == RenderPass::Shadow) {
    return Layouts::Get()->general_pipeline_layout();
  }
  return nullptr;
}
