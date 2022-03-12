#include "layouts.h"

#include "device.h"
#include "structures.h"

namespace {

static Layouts *g_Layouts = nullptr;

vk::DescriptorSetLayout CreateDescriptorSetLayout_Scene() {
  vk::DescriptorSetLayoutBinding ubo_binding;
  ubo_binding.setBinding(0)
      .setDescriptorCount(1)
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                     vk::ShaderStageFlagBits::eFragment);

  auto environment_map_binding =
      vk::DescriptorSetLayoutBinding()
          .setBinding(1)
          .setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setStageFlags(vk::ShaderStageFlagBits::eFragment);

  auto shadow_map_binding =
    vk::DescriptorSetLayoutBinding()
        .setBinding(2)
        .setDescriptorCount(NUM_LIGHTS)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

  std::array<vk::DescriptorSetLayoutBinding, 3> bindings = {
      ubo_binding, environment_map_binding, shadow_map_binding};

  vk::DescriptorSetLayoutCreateInfo create_info;
  create_info.setBindingCount(bindings.size()).setPBindings(bindings.data());

  return Device::Get()->device().createDescriptorSetLayout(create_info);
}

vk::DescriptorSetLayout CreateDescriptorSetLayout_Material() {
  auto ubo_binding = vk::DescriptorSetLayoutBinding()
                         .setBinding(0)
                         .setDescriptorCount(1)
                         .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                         .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                                        vk::ShaderStageFlagBits::eFragment);

  auto diffuse_map_binding =
      vk::DescriptorSetLayoutBinding()
          .setBinding(1)
          .setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setStageFlags(vk::ShaderStageFlagBits::eFragment);

  auto normal_map_binding =
      vk::DescriptorSetLayoutBinding()
          .setBinding(2)
          .setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
          .setStageFlags(vk::ShaderStageFlagBits::eFragment);

  std::array<vk::DescriptorSetLayoutBinding, 3> bindings = {
      ubo_binding,
      diffuse_map_binding,
      normal_map_binding,
  };

  auto create_info = vk::DescriptorSetLayoutCreateInfo().setBindings(bindings);

  return Device::Get()->device().createDescriptorSetLayout(create_info);
}

} // namespace

Layouts *Layouts::Get() { return g_Layouts; }

Layouts::Layouts() {
  g_Layouts = this;

  scene_dsl_ = CreateDescriptorSetLayout_Scene();
  material_dsl_ = CreateDescriptorSetLayout_Material();

  std::array<vk::DescriptorSetLayout, 2> descriptor_set_layouts = {
      scene_dsl_, material_dsl_};

  auto push_constant_range =
      vk::PushConstantRange()
          .setOffset(0)
          .setSize(sizeof(glm::mat4))
          .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                         vk::ShaderStageFlagBits::eFragment);

  auto general_pipeline_layout_info =
      vk::PipelineLayoutCreateInfo()
          .setSetLayoutCount(
              static_cast<uint32_t>(descriptor_set_layouts.size()))
          .setPSetLayouts(descriptor_set_layouts.data())
          .setPushConstantRangeCount(1)
          .setPPushConstantRanges(&push_constant_range);
  general_pipeline_layout_ = Device::Get()->device().createPipelineLayout(
      general_pipeline_layout_info);

  auto shadow_pipeline_layout_info =
      vk::PipelineLayoutCreateInfo().setPushConstantRanges(push_constant_range);
  shadow_pipeline_layout_ =
      Device::Get()->device().createPipelineLayout(shadow_pipeline_layout_info);
}

Layouts::~Layouts() {
  g_Layouts = nullptr;
  Device::Get()->device().destroyDescriptorSetLayout(material_dsl_);
  Device::Get()->device().destroyDescriptorSetLayout(scene_dsl_);
  Device::Get()->device().destroyPipelineLayout(general_pipeline_layout_);
  Device::Get()->device().destroyPipelineLayout(shadow_pipeline_layout_);
}
