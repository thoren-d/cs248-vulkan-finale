#ifndef RENDER_PASSES_H_
#define RENDER_PASSES_H_

#include <vulkan/vulkan.hpp>

enum class RenderPass {
    Shadow,
    Opaque,
};

class RenderPasses {
public:
    RenderPasses();
    ~RenderPasses();

    static RenderPasses* Get();

    vk::RenderPass GetRenderPass(RenderPass pass);

private:
    vk::RenderPass opaque_pass_;
    vk::RenderPass shadow_pass_;
};

#endif  // RENDER_PASSES_H_
