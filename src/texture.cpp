#include "texture.h"

#include <stb_image.h>

#include "device.h"

Texture::Texture(const std::string &filename, Usage usage) {
  int width;
  int height;
  int channels;
  void* data = nullptr;
  size_t size = 0;
  if (usage == Usage::HDRI) {
    data = stbi_loadf(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    size = width * height * sizeof(float) * 4;
  } else {
    data = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    size = width * height * 4;
  }

  vk::Format format;
  switch (usage) {
    case Usage::Color:
      format = vk::Format::eR8G8B8A8Srgb;
      break;
    case Usage::Normal:
      format = vk::Format::eR8G8B8A8Unorm;
      break;
    case Usage::HDRI:
      format = vk::Format::eR32G32B32A32Sfloat;
      break;
  };

  if (!data) {
    throw "Failed to load texture image.";
  }

  uint32_t mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

  image_ = ResourceManager::Get()->CreateImageFromData(
      vk::ImageUsageFlagBits::eSampled, format, width,
      height, mip_levels, data, size);

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
                  .setLevelCount(mip_levels));
  image_view_ = Device::Get()->device().createImageView(view_create_info);

  auto sampler_info = vk::SamplerCreateInfo()
                          .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                          .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                          .setMagFilter(vk::Filter::eLinear)
                          .setMinFilter(vk::Filter::eLinear)
                          .setMipmapMode(vk::SamplerMipmapMode::eNearest)
                          .setMinLod(0.0f)
                          .setMaxLod(static_cast<float>(mip_levels))
                          .setAnisotropyEnable(true)
                          .setMaxAnisotropy(16.0f);
  sampler_ = Device::Get()->device().createSampler(sampler_info);
}

Texture::~Texture() {
  Device::Get()->device().destroySampler(sampler_);
  Device::Get()->device().destroyImageView(image_view_);
}
