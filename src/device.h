#ifndef DEVICE_H_
#define DEVICE_H_

#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

class Device {
public:
    Device(GLFWwindow* window);
    ~Device();

    static Device* Get();

    VmaAllocator allocator() {
        return allocator_;
    }

    vk::Device device() {
        return device_;
    }

    vk::PhysicalDevice physical_device() {
        return physical_device_;
    }

    vk::SwapchainKHR swapchain() {
        return swapchain_;
    }

    std::vector<vk::Image>& swapchain_images() {
        return swapchain_images_;
    }

    std::vector<vk::ImageView>& swapchain_image_views() {
        return swapchain_image_views_;
    }

    vk::Format swapchain_format() {
        return swapchain_format_;
    }

    vk::Extent2D swapchain_extent() {
        return swapchain_extent_;
    }

    vk::Queue graphics_queue() {
        return graphics_queue_;
    }

    uint32_t graphics_queue_family() {
        return graphics_queue_family_;
    }

    vk::Queue present_queue() {
        return present_queue_;
    }

    uint32_t present_queue_family() {
        return present_queue_family_;
    }

    vk::SampleCountFlagBits msaa_samples() {
        return msaa_samples_;
    }

    void Present();
private:

    void InitInstance();
    void InitLogicalDevice();
    void InitSurface();
    void InitSwapchain();
    void InitImageViews();
    void InitAllocator();
    void AddGLFWRequiredInstanceExtensions(std::vector<const char*>& list);
    vk::PhysicalDevice PickPhysicalDevice();
    bool IsDeviceSuitable(vk::PhysicalDevice device);

    GLFWwindow* window_;
    vk::Instance instance_;
    vk::PhysicalDevice physical_device_;
    vk::Device device_;
    vk::SurfaceKHR surface_;

    vk::SwapchainKHR swapchain_;
    std::vector<vk::Image> swapchain_images_;
    std::vector<vk::ImageView> swapchain_image_views_;
    vk::Format swapchain_format_;
    vk::Extent2D swapchain_extent_;

    uint32_t graphics_queue_family_;
    vk::Queue graphics_queue_;
    uint32_t present_queue_family_;
    vk::Queue present_queue_;

    VmaAllocator allocator_;

    vk::SampleCountFlagBits msaa_samples_;

};

#endif  // DEVICE_H_
