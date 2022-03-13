#ifndef RESOURCE_MANAGER_H_
#define RESOURCE_MANAGER_H_

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

class ResourceManager {
public:
  class Buffer {
  public:
    vk::Buffer buffer;
    VmaAllocation allocation;
    size_t size;
    vk::MemoryPropertyFlags flags;

    Buffer() : buffer(nullptr) {}
    Buffer(vk::Buffer b, VmaAllocation a, size_t s, vk::MemoryPropertyFlags f)
        : buffer(b), allocation(a), size(s), flags(f) {}
    Buffer(Buffer &&other)
        : buffer(other.buffer), allocation(other.allocation), size(other.size),
          flags(other.flags) {
      other.buffer = nullptr;
    }
    ~Buffer();

    Buffer &operator=(Buffer &&other) {
      if (buffer) {
        // Destroy self first.
        Buffer self = std::move(*this);
      }
      buffer = other.buffer;
      allocation = other.allocation;
      size = other.size;
      flags = other.flags;
      other.buffer = nullptr;
      return *this;
    }
  };

  class Image {
  public:
    vk::Image image;
    VmaAllocation allocation;
    vk::MemoryPropertyFlags flags;
    uint32_t mip_levels = 1;

    Image() : image(nullptr) {}
    Image(vk::Image i, VmaAllocation a, vk::MemoryPropertyFlags f)
        : image(i), allocation(a), flags(f) {}
    Image(Image &&other)
        : image(other.image), allocation(other.allocation), flags(other.flags) {
      other.image = nullptr;
    }
    ~Image();

    Image &operator=(Image &&other) {
      if (image) {
        // Destroy self first.
        Image self = std::move(*this);
      }
      image = other.image;
      allocation = other.allocation;
      flags = other.flags;
      other.image = nullptr;
      return *this;
    }
  };

  ResourceManager(vk::CommandBuffer transfer_commands);
  ~ResourceManager();

  static ResourceManager *Get();

  Buffer CreateHostBufferWithData(vk::BufferUsageFlags usage, const void *data,
                                  size_t size);
  Buffer CreateDeviceBufferWithData(vk::BufferUsageFlags usage,
                                    const void *data, size_t size);

  Image CreateImageUninitialized(
      vk::ImageUsageFlags usage, vk::Format format, uint32_t width,
      uint32_t height, uint32_t mip_levels = 1,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1);
  Image CreateImageFromData(vk::ImageUsageFlags usage, vk::Format format,
                            uint32_t width, uint32_t height,
                            uint32_t mip_levels, const void *data, size_t size);
  void TransitionImageLayout(vk::Image image, vk::ImageLayout before,
                             vk::ImageLayout after, uint32_t mip_levels);

  void UpdateHostBufferData(Buffer &buffer, const void *data, size_t size);

  void WaitForTransfers();

private:
  void GenerateMipmaps(vk::Image image, int32_t width, int32_t height,
                       uint32_t mip_levels);

  vk::PhysicalDeviceMemoryProperties memory_properties_;
  vk::CommandBuffer transfer_commands_;
  std::vector<Buffer> staging_buffers_;
};

#endif // RESOURCE_MANAGER_H_
