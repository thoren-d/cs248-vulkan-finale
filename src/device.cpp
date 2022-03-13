#include "device.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>

// Some Windows header file defines these >:(
#undef min
#undef max

namespace {
    vk::SurfaceFormatKHR ChooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available) {
        vk::SurfaceFormatKHR preferred(vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
        if (std::find(available.begin(), available.end(), preferred) != available.end()) {
            return preferred;
        }
        vk::SurfaceFormatKHR other(vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear);
        if (std::find(available.begin(), available.end(), other) != available.end()) {
            return other;
        }
        return available[0];
    }

    vk::PresentModeKHR ChoosePresentMode(const std::vector<vk::PresentModeKHR>& available) {
        if (std::find(available.begin(), available.end(), vk::PresentModeKHR::eFifoRelaxed) != available.end()) {
            return vk::PresentModeKHR::eFifoRelaxed;
        }
        if (std::find(available.begin(), available.end(), vk::PresentModeKHR::eFifo) != available.end()) {
            return vk::PresentModeKHR::eFifo;
        }
        if (std::find(available.begin(), available.end(), vk::PresentModeKHR::eMailbox) != available.end()) {
            return vk::PresentModeKHR::eMailbox;
        }
        if (std::find(available.begin(), available.end(), vk::PresentModeKHR::eImmediate) != available.end()) {
            return vk::PresentModeKHR::eImmediate;
        }

        std::cerr << "No supported present modes available." << std::endl;
        throw "No supported present modes available.";
    }

    vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

static Device* g_Device = nullptr;
}

Device::Device(GLFWwindow* window):window_(window) {
    g_Device = this;
    InitInstance();
    InitSurface();
    InitLogicalDevice();
    InitSwapchain();
    InitAllocator();
}

Device::~Device() {
    vmaDestroyAllocator(allocator_);
    for (auto image_view : swapchain_image_views_) {
        device_.destroyImageView(image_view);
    }
    device_.destroySwapchainKHR(swapchain_);
    device_.destroy();
    instance_.destroySurfaceKHR(surface_);
    instance_.destroy();
    g_Device = nullptr;
}

void Device::InitInstance() {
    vk::ApplicationInfo app_info(
        "CS248 Final Project", VK_MAKE_VERSION(0, 1, 0),
        "Engine Name", VK_MAKE_VERSION(0, 1, 0),
        VK_API_VERSION_1_0
    );

    std::vector<const char*> required_extensions;
    AddGLFWRequiredInstanceExtensions(required_extensions);

    vk::InstanceCreateInfo create_info;
    create_info.enabledExtensionCount = (uint32_t)required_extensions.size();
    create_info.ppEnabledExtensionNames = required_extensions.data();
    create_info.setPApplicationInfo(&app_info);

    instance_ = vk::createInstance(create_info);
}

void Device::InitSurface() {
    VkSurfaceKHR surface;
    auto res = glfwCreateWindowSurface(instance_, window_, nullptr, &surface);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create surface." << std::endl;
        throw "Failed to create surface.";
    }
    surface_ = surface;
}

vk::PhysicalDevice Device::PickPhysicalDevice() {
    auto devices = instance_.enumeratePhysicalDevices();
    std::optional<vk::PhysicalDevice> chosen;
    for (vk::PhysicalDevice device : devices) {
        if (IsDeviceSuitable(device)) {
            return device;
        }
    }

    std::cerr << "No suitable devices found." << std::endl;
    throw "No suitable devices found.";
}

bool Device::IsDeviceSuitable(vk::PhysicalDevice device) {
    bool has_graphics = false;
    bool has_present = false;

    auto queue_families = device.getQueueFamilyProperties();
    for (size_t i = 0; i < queue_families.size(); i++) {
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            has_graphics = true;
        }
        if (device.getSurfaceSupportKHR((uint32_t)i, surface_)) {
            has_present = true;
        }
    }

    if (device.getSurfaceFormatsKHR(surface_).empty())
        return false;
    if (device.getSurfacePresentModesKHR(surface_).empty())
        return false;

    return has_graphics && has_present;
}

void Device::InitLogicalDevice() {
    physical_device_ = PickPhysicalDevice();

    auto props = physical_device_.getProperties();
    auto counts = props.limits.framebufferColorSampleCounts | props.limits.framebufferDepthSampleCounts;
    msaa_samples_ = [&]() {
        // if (counts & vk::SampleCountFlagBits::e64) return vk::SampleCountFlagBits::e64;
        // if (counts & vk::SampleCountFlagBits::e32) return vk::SampleCountFlagBits::e32;
        // if (counts & vk::SampleCountFlagBits::e16) return vk::SampleCountFlagBits::e16;
        // if (counts & vk::SampleCountFlagBits::e8) return vk::SampleCountFlagBits::e8;
        if (counts & vk::SampleCountFlagBits::e4) return vk::SampleCountFlagBits::e4;
        if (counts & vk::SampleCountFlagBits::e2) return vk::SampleCountFlagBits::e2;
        return vk::SampleCountFlagBits::e1;
    }();

    auto queue_families = physical_device_.getQueueFamilyProperties();
    // Find graphics queue family
    for (size_t i = 0; i < queue_families.size(); i++) {
        if (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            graphics_queue_family_ = (uint32_t)i;
            break;
        }
    }
    // Find present queue family
    for (size_t i = 0; i < queue_families.size(); i++) {
        if (physical_device_.getSurfaceSupportKHR((uint32_t)i, surface_)) {
            present_queue_family_ = (uint32_t)i;
        }
    }


    float priority = 1.0f;
    vk::DeviceQueueCreateInfo graphics_q_create_info;
    graphics_q_create_info.setQueueCount(1)
        .setQueueFamilyIndex(graphics_queue_family_)
        .setPQueuePriorities(&priority);
    vk::DeviceQueueCreateInfo present_q_create_info;
    present_q_create_info.setQueueCount(1)
        .setQueueFamilyIndex(present_queue_family_)
        .setPQueuePriorities(&priority);
    vk::DeviceQueueCreateInfo infos[2] = {graphics_q_create_info, present_q_create_info};

    const std::vector<const char*> required_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    vk::PhysicalDeviceFeatures features = {};
    features.samplerAnisotropy = true;
    features.sampleRateShading = true;

    vk::DeviceCreateInfo create_info;
    create_info.setPQueueCreateInfos(infos)
        .setQueueCreateInfoCount((graphics_queue_family_ == present_queue_family_) ? 1 : 2)
        .setEnabledExtensionCount((uint32_t)required_extensions.size())
        .setPpEnabledExtensionNames(required_extensions.data())
        .setPEnabledFeatures(&features);
    

    device_ = physical_device_.createDevice(create_info);

    graphics_queue_ = device_.getQueue(graphics_queue_family_, 0);
    present_queue_ = device_.getQueue(present_queue_family_, 0);
}

void Device::InitSwapchain() {
    auto capabilities = physical_device_.getSurfaceCapabilitiesKHR(surface_);
    std::vector<vk::SurfaceFormatKHR> formats = physical_device_.getSurfaceFormatsKHR(surface_);
    std::vector<vk::PresentModeKHR> present_modes = physical_device_.getSurfacePresentModesKHR(surface_);

    vk::SurfaceFormatKHR chosen_format = ChooseSurfaceFormat(formats);
    vk::PresentModeKHR chosen_present_mode = ChoosePresentMode(present_modes);
    vk::Extent2D chosen_extent = ChooseSwapExtent(capabilities, window_);
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }

    vk::SwapchainCreateInfoKHR create_info;
    create_info.setSurface(surface_)
        .setMinImageCount(image_count)
        .setImageFormat(chosen_format.format)
        .setImageColorSpace(chosen_format.colorSpace)
        .setImageExtent(chosen_extent)
        .setImageArrayLayers(1)
        .setPresentMode(chosen_present_mode)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setPreTransform(capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setClipped(true);

    if (graphics_queue_family_ != present_queue_family_) {
        uint32_t queue_families[] = {graphics_queue_family_, present_queue_family_};
        create_info.setImageSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndexCount(2)
            .setPQueueFamilyIndices(queue_families);
    } else {
        create_info.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    swapchain_ = device_.createSwapchainKHR(create_info);

    swapchain_images_ = device_.getSwapchainImagesKHR(swapchain_);
    swapchain_extent_ = chosen_extent;
    swapchain_format_ = chosen_format.format;

    InitImageViews();
}

void Device::InitImageViews() {
    for (size_t i = 0; i < swapchain_images_.size(); i++) {
        vk::ImageViewCreateInfo create_info;
        create_info.setImage(swapchain_images_[i])
            .setComponents(vk::ComponentMapping())
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(swapchain_format_)
            .setSubresourceRange(vk::ImageSubresourceRange()
                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                    .setBaseMipLevel(0)
                    .setLevelCount(1)
                    .setBaseArrayLayer(0)
                    .setLayerCount(1));

        swapchain_image_views_.push_back(device_.createImageView(create_info));
    }
}

void Device::AddGLFWRequiredInstanceExtensions(std::vector<const char*>& list) {
    uint32_t glfwExtensionsCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

    if (!glfwExtensions) {
        std::cerr << "Failed to get required extensions list." << std::endl;
        throw "Failed to get required extensions list.";
    }

    for (uint32_t i = 0; i < glfwExtensionsCount; i++) {
        list.push_back(glfwExtensions[i]);
    }
}

void Device::InitAllocator() {
    VmaAllocatorCreateInfo create_info = {};
    VmaVulkanFunctions vulkan_functions = {};
    vulkan_functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    create_info.pVulkanFunctions = &vulkan_functions;
    create_info.vulkanApiVersion = VK_API_VERSION_1_0;
    create_info.physicalDevice = physical_device_;
    create_info.device = device_;
    create_info.instance = instance_;

    if (vmaCreateAllocator(&create_info, &allocator_) != VK_SUCCESS) {
        throw "Failed to create Vulkan allocator.";
    }
}

Device* Device::Get() {
    return g_Device;
}
