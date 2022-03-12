#include "texture.h"

#include <stb_image.h>

#include "device.h"

Texture::Texture(const std::string &filename, Usage usage) {
  int width;
  int height;
  int channels;
  stbi_uc *data =
      stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
  size_t size = width * height * 4;

  vk::Format format;
  switch (usage) {
    case Usage::Color:
      format = vk::Format::eR8G8B8A8Srgb;
      break;
    case Usage::Normal:
      format = vk::Format::eR8G8B8A8Unorm;
      break;
    case Usage::HDRI:
      throw "Not implemented.";
  };

  if (!data) {
    throw "Failed to load texture image.";
  }

  image_ = ResourceManager::Get()->CreateImageFromData(
      vk::ImageUsageFlagBits::eSampled, format, width,
      height, data, size);

  auto view_create_info =
      vk::ImageViewCreateInfo()
          .setImage(image_.image)
          .setComponents({})
          .setFormat(format)
          .setViewType(vk::ImageViewType::e2D)
          .setSubresourceRange(
              vk::ImageSubresourceRange()
                  .setAspectMask(vk::ImageAspectFlagBits::eColor)
                  .setBaseArrayLayer(0)
                  .setBaseMipLevel(0)
                  .setLayerCount(1)
                  .setLevelCount(1));
  image_view_ = Device::Get()->device().createImageView(view_create_info);

  auto sampler_info = vk::SamplerCreateInfo()
                          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                          .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                          .setMagFilter(vk::Filter::eLinear)
                          .setMinFilter(vk::Filter::eLinear)
                          .setMipmapMode(vk::SamplerMipmapMode::eLinear);
  sampler_ = Device::Get()->device().createSampler(sampler_info);
}

Texture::~Texture() {
  Device::Get()->device().destroySampler(sampler_);
  Device::Get()->device().destroyImageView(image_view_);
}
