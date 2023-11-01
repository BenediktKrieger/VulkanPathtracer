#include <vk_initializers.h>

vk::FramebufferCreateInfo vkinit::framebuffer_create_info(vk::RenderPass &renderPass, vk::Extent2D &extent)
{
    vk::FramebufferCreateInfo createInfo;
    createInfo.setRenderPass(renderPass);
    createInfo.setAttachmentCount(1);
    createInfo.setWidth(extent.width);
    createInfo.setHeight(extent.height);
    createInfo.setLayers(1);
    return createInfo;
}

vk::CommandPoolCreateInfo vkinit::command_pool_create_info(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags)
{
    vk::CommandPoolCreateInfo createInfo;
    createInfo.setFlags(flags);
    return createInfo;
}

vk::CommandBufferAllocateInfo vkinit::command_buffer_allocate_info(vk::CommandPool pool, uint32_t count, vk::CommandBufferLevel level)
{
    vk::CommandBufferAllocateInfo createInfo;
    createInfo.setCommandPool(pool);
    createInfo.setCommandBufferCount(count);
    createInfo.setLevel(level);
    return createInfo;
}

vk::CommandBufferBeginInfo vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlags flags)
{
    vk::CommandBufferBeginInfo createInfo;
    createInfo.setFlags(flags);
    return createInfo;
}

vk::FenceCreateInfo vkinit::fence_create_info(vk::FenceCreateFlags flags)
{
    vk::FenceCreateInfo createInfo;
    createInfo.setFlags(flags);
    return createInfo;
}

vk::SemaphoreCreateInfo vkinit::semaphore_create_info(vk::SemaphoreCreateFlags flags)
{
    vk::SemaphoreCreateInfo createInfo;
    createInfo.setFlags(flags);
    return createInfo;
}

vk::RenderPassBeginInfo vkinit::renderpass_begin_info(vk::RenderPass renderPass, vk::Extent2D windowExtent, vk::Framebuffer framebuffer)
{
    vk::RenderPassBeginInfo createInfo;
    createInfo.setRenderPass(renderPass);
    createInfo.setRenderArea(vk::Rect2D({0, 0}, windowExtent));
    createInfo.setClearValueCount(1);
    createInfo.setFramebuffer(framebuffer);
    return createInfo;
}

vk::SubmitInfo vkinit::submit_info(vk::CommandBuffer *cmd)
{
    vk::SubmitInfo createInfo;
    createInfo.setCommandBufferCount(1);
    createInfo.setPCommandBuffers(cmd);

    return createInfo;
}

vk::PresentInfoKHR vkinit::present_info()
{
    vk::PresentInfoKHR createInfo;
    return createInfo;
}