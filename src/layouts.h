#ifndef LAYOUTS_H_
#define LAYOUTS_H_

#include <vulkan/vulkan.hpp>

class Layouts {
public:
    Layouts();
    ~Layouts();

    static Layouts* Get();

    vk::PipelineLayout general_pipeline_layout() {
        return general_pipeline_layout_;
    }

    vk::PipelineLayout shadow_pipeline_layout() {
        return shadow_pipeline_layout_;
    }

    vk::DescriptorSetLayout material_dsl() {
        return material_dsl_;
    }

    vk::DescriptorSetLayout scene_dsl() {
        return scene_dsl_;
    }

private:

    vk::DescriptorSetLayout material_dsl_;
    vk::DescriptorSetLayout scene_dsl_;
    vk::PipelineLayout general_pipeline_layout_;
    vk::PipelineLayout shadow_pipeline_layout_;
};

#endif // LAYOUTS_H_
