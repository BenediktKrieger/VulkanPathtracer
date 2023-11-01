#pragma once
#include <vk_types.h>

namespace vkinit {
    vk::FramebufferCreateInfo framebuffer_create_info(vk::RenderPass &renderPass, vk::Extent2D &extent);
    vk::CommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags = {});
    vk::CommandBufferAllocateInfo command_buffer_allocate_info(vk::CommandPool pool, uint32_t count, vk::CommandBufferLevel level);
    vk::CommandBufferBeginInfo command_buffer_begin_info(vk::CommandBufferUsageFlags flags = {});
    vk::FenceCreateInfo fence_create_info(vk::FenceCreateFlags flags = {});
    vk::SemaphoreCreateInfo semaphore_create_info(vk::SemaphoreCreateFlags flags = {});
}

