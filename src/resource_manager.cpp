#include "resource_manager.h"

#include <iostream>

#include "device.h"

namespace {

static ResourceManager *g_ResourceManager = nullptr;

}

ResourceManager::Buffer::~Buffer() {
  if (!buffer)
    return;
  // std::cerr << "Destroyed a buffer, hope that was on purpose." << std::endl;
  vmaDestroyBuffer(Device::Get()->allocator(), buffer, allocation);
}

ResourceManager::Image::~Image() {
  if (!image)
    return;

  vmaDestroyImage(Device::Get()->allocator(), image, allocation);
}

ResourceManager::ResourceManager(vk::CommandBuffer transfer_commands) {
  g_ResourceManager = this;
  transfer_commands_ = transfer_commands;

  memory_properties_ = Device::Get()->physical_device().getMemoryProperties();

  vk::CommandBufferBeginInfo begin_info;
  begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  transfer_commands_.begin(begin_info);
}

ResourceManager::~ResourceManager() { g_ResourceManager = nullptr; }

ResourceManager *ResourceManager::Get() { return g_ResourceManager; }

ResourceManager::Buffer
ResourceManager::CreateHostBufferWithData(vk::BufferUsageFlags usage,
                                          const void *data, size_t size) {
  vk::BufferCreateInfo buffer_create_info;
  buffer_create_info.setUsage(usage).setSize(size).setSharingMode(
      vk::SharingMode::eExclusive);

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
  alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
  if (vmaCreateBuffer(Device::Get()->allocator(),
                      &(const VkBufferCreateInfo &)buffer_create_info,
                      &alloc_info, &buffer, &allocation,
                      &allocation_info) != VK_SUCCESS) {
    throw "Failed to create buffer.";
  }
  vk::MemoryPropertyFlags flags =
      memory_properties_.memoryTypes[allocation_info.memoryType].propertyFlags;

  void *mapping = nullptr;
  if (vmaMapMemory(Device::Get()->allocator(), allocation, &mapping) !=
      VK_SUCCESS) {
    throw "Failed to map memory.";
  }
  memcpy(mapping, data, size);
  if (!(flags & vk::MemoryPropertyFlagBits::eHostCoherent)) {
    vmaFlushAllocation(Device::Get()->allocator(), allocation, 0, size);
  }
  vmaUnmapMemory(Device::Get()->allocator(), allocation);

  return Buffer(buffer, allocation, size, flags);
}

ResourceManager::Buffer
ResourceManager::CreateDeviceBufferWithData(vk::BufferUsageFlags usage,
                                            const void *data, size_t size) {
  Buffer staging_buffer = CreateHostBufferWithData(
      vk::BufferUsageFlagBits::eTransferSrc, data, size);

  vk::BufferCreateInfo buffer_create_info;
  buffer_create_info.setUsage(usage | vk::BufferUsageFlagBits::eTransferDst)
      .setSize(size)
      .setSharingMode(vk::SharingMode::eExclusive);

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
  if (vmaCreateBuffer(Device::Get()->allocator(),
                      &(const VkBufferCreateInfo &)buffer_create_info,
                      &alloc_info, &buffer, &allocation,
                      &allocation_info) != VK_SUCCESS) {
    throw "Failed to create buffer.";
  }
  vk::MemoryPropertyFlags flags =
      memory_properties_.memoryTypes[allocation_info.memoryType].propertyFlags;

  Buffer result(buffer, allocation, size, flags);

  transfer_commands_.copyBuffer(staging_buffer.buffer, result.buffer,
                                {vk::BufferCopy(0, 0, size)});

  staging_buffers_.emplace_back(std::move(staging_buffer));
  return std::move(result);
}

ResourceManager::Image
ResourceManager::CreateImageUninitialized(vk::ImageUsageFlags usage,
                                          vk::Format format, uint32_t width,
                                          uint32_t height, uint32_t mip_levels, vk::SampleCountFlagBits sample_count) {
  auto image_create_info = vk::ImageCreateInfo()
                               .setImageType(vk::ImageType::e2D)
                               .setExtent(vk::Extent3D(width, height, 1))
                               .setMipLevels(mip_levels)
                               .setArrayLayers(1)
                               .setFormat(format)
                               .setTiling(vk::ImageTiling::eOptimal)
                               .setInitialLayout(vk::ImageLayout::eUndefined)
                               .setUsage(usage)
                               .setSharingMode(vk::SharingMode::eExclusive)
                               .setSamples(sample_count);

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;

  VkImage image;
  VmaAllocation allocation;
  VmaAllocationInfo allocation_info;
  if (vmaCreateImage(Device::Get()->allocator(),
                     &(const VkImageCreateInfo &)image_create_info,
                     &alloc_create_info, &image, &allocation,
                     &allocation_info) != VK_SUCCESS) {
    throw "Failed to create image";
  }

  return Image(
      image, allocation,
      memory_properties_.memoryTypes[allocation_info.memoryType].propertyFlags);
}

ResourceManager::Image ResourceManager::CreateImageFromData(
    vk::ImageUsageFlags usage, vk::Format format, uint32_t width,
    uint32_t height, uint32_t mip_levels, const void *data, size_t size) {
  Image result = CreateImageUninitialized(
      usage | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc, format, width, height, mip_levels);

  Buffer staging_buffer = CreateHostBufferWithData(
      vk::BufferUsageFlagBits::eTransferSrc, data, size);

  TransitionImageLayout(result.image, vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal, mip_levels);

  auto buffer_image_copy =
      vk::BufferImageCopy()
          .setBufferImageHeight(0)
          .setBufferOffset(0)
          .setBufferRowLength(0)
          .setImageExtent(vk::Extent3D(width, height, 1))
          .setImageOffset({})
          .setImageSubresource(
              vk::ImageSubresourceLayers()
                  .setAspectMask(vk::ImageAspectFlagBits::eColor)
                  .setBaseArrayLayer(0)
                  .setLayerCount(1)
                  .setMipLevel(0));
  transfer_commands_.copyBufferToImage(staging_buffer.buffer, result.image,
                                       vk::ImageLayout::eTransferDstOptimal,
                                       {buffer_image_copy});

  if (mip_levels > 1) {
    GenerateMipmaps(result.image, width, height, mip_levels);
  } else {
    TransitionImageLayout(result.image, vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal, mip_levels);
  }

  staging_buffers_.emplace_back(std::move(staging_buffer));
  return std::move(result);
}

void ResourceManager::UpdateHostBufferData(Buffer &buffer, const void *data,
                                           size_t size) {
  if (size > buffer.size) {
    throw "Buffer isn't big enough for that data.";
  }

  void *mapping = nullptr;
  vmaMapMemory(Device::Get()->allocator(), buffer.allocation, &mapping);
  memcpy(mapping, data, size);
  if (!(buffer.flags & vk::MemoryPropertyFlagBits::eHostCoherent)) {
    vmaFlushAllocation(Device::Get()->allocator(), buffer.allocation, 0, size);
  }
  vmaUnmapMemory(Device::Get()->allocator(), buffer.allocation);
}

void ResourceManager::WaitForTransfers() {
  if (staging_buffers_.empty()) {
    return;
  }

  transfer_commands_.end();

  vk::SubmitInfo submit_info;
  submit_info.setCommandBufferCount(1).setPCommandBuffers(&transfer_commands_);

  Device::Get()->graphics_queue().submit(submit_info, nullptr);
  Device::Get()->device().waitIdle();

  transfer_commands_.reset({});
  staging_buffers_.clear();

  vk::CommandBufferBeginInfo begin_info;
  begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
  transfer_commands_.begin(begin_info);
}

void ResourceManager::TransitionImageLayout(vk::Image image,
                                            vk::ImageLayout before,
                                            vk::ImageLayout after,
                                            uint32_t mip_levels) {

  vk::ImageAspectFlags aspect;
  if (after == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    aspect = vk::ImageAspectFlagBits::eDepth;
  } else {
    aspect = vk::ImageAspectFlagBits::eColor;
  }

  auto image_barrier = vk::ImageMemoryBarrier()
                           .setOldLayout(before)
                           .setNewLayout(after)
                           .setImage(image)
                           .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                           .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                           .setSubresourceRange(vk::ImageSubresourceRange()
                                                    .setAspectMask(aspect)
                                                    .setBaseArrayLayer(0)
                                                    .setBaseMipLevel(0)
                                                    .setLayerCount(1)
                                                    .setLevelCount(mip_levels));

  vk::PipelineStageFlags src_stage;
  vk::PipelineStageFlags dst_stage;
  if (before == vk::ImageLayout::eUndefined &&
      after == vk::ImageLayout::eTransferDstOptimal) {
    image_barrier.setSrcAccessMask({}).setDstAccessMask(
        vk::AccessFlagBits::eTransferWrite);
    src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    dst_stage = vk::PipelineStageFlagBits::eTransfer;
  } else if (before == vk::ImageLayout::eTransferDstOptimal &&
             after == vk::ImageLayout::eShaderReadOnlyOptimal) {
    image_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    src_stage = vk::PipelineStageFlagBits::eTransfer;
    // TODO: Support reading textures in other shaders?
    dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
  } else if (before == vk::ImageLayout::eUndefined &&
             after == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    image_barrier.setSrcAccessMask({}).setDstAccessMask(
        vk::AccessFlagBits::eDepthStencilAttachmentRead |
        vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    dst_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
  } else {
    throw "Unsupported layout transition.";
  }

  transfer_commands_.pipelineBarrier(src_stage, dst_stage, {}, {}, {},
                                     {image_barrier});
}

void ResourceManager::GenerateMipmaps(vk::Image image, int32_t width, int32_t height, uint32_t mip_levels) {
  auto barrier = vk::ImageMemoryBarrier()
    .setImage(image)
    .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
    .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
    .setSubresourceRange(vk::ImageSubresourceRange()
      .setAspectMask(vk::ImageAspectFlagBits::eColor)
      .setBaseArrayLayer(0)
      .setLayerCount(1)
      .setLevelCount(1));

  int32_t level_width = width;
  int32_t level_height = height;

  for (uint32_t i = 1; i < mip_levels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

    transfer_commands_.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

    int32_t next_width = level_width > 1 ? level_width / 2 : 1;
    int32_t next_height = level_height > 1 ? level_height / 2 : 1;

    auto blit = vk::ImageBlit()
      .setSrcOffsets({vk::Offset3D{0,0,0}, vk::Offset3D{level_width, level_height, 1}})
      .setDstOffsets({vk::Offset3D{0,0,0}, vk::Offset3D{next_width, next_height, 1}})
      .setSrcSubresource(vk::ImageSubresourceLayers()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setMipLevel(i-1))
      .setDstSubresource(vk::ImageSubresourceLayers()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setMipLevel(i));
    transfer_commands_.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
      image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

    level_width = next_width;
    level_height = next_height;

    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    transfer_commands_.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
  }

  barrier.subresourceRange.baseMipLevel = mip_levels - 1;
  barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
  barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  transfer_commands_.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
}
