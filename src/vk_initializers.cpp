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