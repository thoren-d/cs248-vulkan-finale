#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <string>

#include <vulkan/vulkan.hpp>

#include "resource_manager.h"

class Texture {
public:
    Texture(const std::string& filename);
    ~Texture();

    vk::ImageView image_view() {
        return image_view_;
    }

    vk::Sampler sampler() {
        return sampler_;
    }

private:
    ResourceManager::Image image_;
    vk::ImageView image_view_;
    vk::Sampler sampler_;
};

#endif  // TEXTURE_H_
