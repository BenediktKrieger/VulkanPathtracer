#pragma once
#include <vk_types.h>

namespace vkinit
{
    vk::FramebufferCreateInfo framebuffer_create_info(vk::RenderPass &renderPass, vk::Extent2D &extent);
    vk::CommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags flags = {});
    vk::CommandBufferAllocateInfo command_buffer_allocate_info(vk::CommandPool pool, uint32_t count, vk::CommandBufferLevel level);
    vk::CommandBufferBeginInfo command_buffer_begin_info(vk::CommandBufferUsageFlags flags = {});
    vk::FenceCreateInfo fence_create_info(vk::FenceCreateFlags flags = {});
    vk::SemaphoreCreateInfo semaphore_create_info(vk::SemaphoreCreateFlags flags = {});
    vk::RenderPassBeginInfo renderpass_begin_info(vk::RenderPass renderPass, vk::Extent2D windowExtent, vk::Framebuffer framebuffer);
    vk::SubmitInfo submit_info(vk::CommandBuffer *cmd);
    vk::PresentInfoKHR present_info();
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info();
    vk::PipelineShaderStageCreateInfo pipeline_shader_stage_create_info(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule);
    vk::PipelineVertexInputStateCreateInfo vertex_input_state_create_info();
    vk::PipelineInputAssemblyStateCreateInfo input_assembly_create_info(vk::PrimitiveTopology topology);
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(vk::PolygonMode polygonMode);
    vk::PipelineMultisampleStateCreateInfo multisampling_state_create_info();
    vk::PipelineColorBlendAttachmentState color_blend_attachment_state();
}
