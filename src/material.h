#ifndef MATERIAL_H_
#define MATERIAL_H_

#include <memory>

#include <vulkan/vulkan.hpp>

#include "render_passes.h"
#include "resource_manager.h"
#include "texture.h"

class Material {
 public:
  virtual ~Material() = default;

  virtual vk::Pipeline GetPipelineForRenderPass(RenderPass pass) = 0;
  virtual vk::PipelineLayout GetPipelineLayoutForRenderPass(
      RenderPass pass) = 0;
  virtual vk::DescriptorSet GetMaterialDescriptorSetForRenderPass(RenderPass pass) = 0;
};

class OpaqueMaterial : public Material {
 public:
  OpaqueMaterial(const std::string& diffuse_map, const std::string& normal_map, float ior, float roughness, float metalness);
  ~OpaqueMaterial();

  vk::Pipeline GetPipelineForRenderPass(RenderPass pass) override;
  vk::PipelineLayout GetPipelineLayoutForRenderPass(RenderPass pass) override;
  vk::DescriptorSet GetMaterialDescriptorSetForRenderPass(RenderPass pass) override {
    return descriptor_set_;
  }

 private:
  struct Pipelines {
    vk::Pipeline opaque_pass;
    vk::Pipeline shadow_pass;

    Pipelines();
    ~Pipelines();

  private:
    void InitOpaquePass();
    void InitShadowPass();
  };
  static std::weak_ptr<Pipelines> s_pipelines_;
  static std::shared_ptr<Pipelines> GetPipelines();


  struct MaterialUniforms {
    float ior;
    float roughness;
    float metalness;
  } params;

  ResourceManager::Buffer uniform_buffer_;
  std::unique_ptr<Texture> diffuse_map_;
  std::unique_ptr<Texture> normal_map_;

  vk::DescriptorSetLayout material_layout_;
  vk::DescriptorPool descriptor_pool_;
  vk::DescriptorSet descriptor_set_;
  std::shared_ptr<Pipelines> pipelines_;
};

vk::Pipeline GetSkyPipeline();

#endif  // MATERIAL_H_
