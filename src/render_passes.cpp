#include "render_passes.h"

#include "device.h"

namespace {

vk::RenderPass CreateOpaqueRenderPass() {
    Device* device = Device::Get();
    
    vk::AttachmentDescription color_attachment;
    color_attachment.setFormat(device->swapchain_format())
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
    vk::AttachmentReference color_attachment_ref;
    color_attachment_ref.setAttachment(0)
        .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    auto depth_attachment = vk::AttachmentDescription()
        .setFormat(vk::Format::eD32Sfloat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
    auto depth_attachment_ref = vk::AttachmentReference()
        .setAttachment(1)
        .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&color_attachment_ref)
        .setPDepthStencilAttachment(&depth_attachment_ref);

    vk::SubpassDependency dependency;
    dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
        .setDstSubpass(0)
        .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
        .setSrcAccessMask({})
        .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    const std::array<vk::AttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
    const std::array<vk::SubpassDescription, 1> subpasses = {subpass};
    const std::array<vk::SubpassDependency, 1> dependencies = {dependency};

    auto  create_info = vk::RenderPassCreateInfo()
        .setAttachments(attachments)
        .setSubpasses(subpasses)
        .setDependencies(dependencies);

    return device->device().createRenderPass(create_info);
}

static RenderPasses* g_RenderPasses = nullptr;

}

RenderPasses::RenderPasses() {
    g_RenderPasses = this;

    opaque_pass_ = CreateOpaqueRenderPass();
}

RenderPasses::~RenderPasses() {
    g_RenderPasses = nullptr;

    Device::Get()->device().destroyRenderPass(opaque_pass_);
}

RenderPasses* RenderPasses::Get() {
    return g_RenderPasses;
}

vk::RenderPass RenderPasses::GetRenderPass(RenderPass pass) {
    if (pass == RenderPass::Opaque) {
        return opaque_pass_;
    }

    return nullptr;
}
