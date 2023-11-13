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
    std::array<vk::ClearValue, 1> clearValues = {vk::ClearValue(vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f))};
    createInfo.setClearValues(clearValues);
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

vk::PipelineLayoutCreateInfo vkinit::pipeline_layout_create_info()
{
    vk::PipelineLayoutCreateInfo info;
    return info;
}

vk::PipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule)
{
    vk::PipelineShaderStageCreateInfo info;
    info.setStage(stage);
    info.setModule(shaderModule);
    info.setPName("main");
    return info;
}

vk::PipelineVertexInputStateCreateInfo vkinit::vertex_input_state_create_info() {
	vk::PipelineVertexInputStateCreateInfo info;
	info.setVertexBindingDescriptionCount(0);
	info.setVertexAttributeDescriptionCount(0);
	return info;
}

vk::PipelineInputAssemblyStateCreateInfo vkinit::input_assembly_create_info(vk::PrimitiveTopology topology) {
	vk::PipelineInputAssemblyStateCreateInfo info;
	info.setTopology(topology);
	info.setPrimitiveRestartEnable(VK_FALSE);
	return info;
}

VkPipelineRasterizationStateCreateInfo vkinit::rasterization_state_create_info(vk::PolygonMode polygonMode)
{
	vk::PipelineRasterizationStateCreateInfo info;
	info.setDepthClampEnable(VK_FALSE);
	info.setRasterizerDiscardEnable(VK_FALSE);
	info.setPolygonMode(polygonMode);
	info.setLineWidth(1.0f);
	info.setCullMode(vk::CullModeFlagBits::eBack);
	info.setFrontFace(vk::FrontFace::eCounterClockwise);
	info.setDepthBiasEnable(VK_FALSE);
	info.setDepthBiasConstantFactor(0.0f);
	info.setDepthBiasClamp(0.0f);
	info.setDepthBiasSlopeFactor(0.0f);
	return info;
}

vk::PipelineMultisampleStateCreateInfo vkinit::multisampling_state_create_info()
{
	vk::PipelineMultisampleStateCreateInfo info;
	info.setRasterizationSamples(vk::SampleCountFlagBits::e1);
	info.setMinSampleShading(1.0f);
	return info;
}

vk::PipelineColorBlendAttachmentState vkinit::color_blend_attachment_state() {
	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
	colorBlendAttachment.setBlendEnable(VK_FALSE);
	return colorBlendAttachment;
}

vk::ImageCreateInfo vkinit::image_create_info(vk::Format format, vk::ImageUsageFlags usageFlags, vk::Extent3D extent)
{
    vk::ImageCreateInfo info;
	info.setImageType(vk::ImageType::e2D);
	info.setFormat(format);
	info.setExtent(extent);
	info.setMipLevels(1);
	info.setArrayLayers(1);
	info.setSamples(vk::SampleCountFlagBits::e1);
	info.setTiling(vk::ImageTiling::eOptimal);
	info.setUsage(usageFlags);
	return info;
}

vk::ImageViewCreateInfo vkinit::imageview_create_info(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags)
{
	vk::ImageViewCreateInfo info;
	info.setViewType(vk::ImageViewType::e2D);
	info.setImage(image);
	info.setFormat(format);
    info.setSubresourceRange(vk::ImageSubresourceRange(aspectFlags, 0, 1, 0, 1));
	return info;
}

vk::PipelineDepthStencilStateCreateInfo vkinit::depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, vk::CompareOp compareOp)
{
    vk::PipelineDepthStencilStateCreateInfo info;
	info.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE;
	info.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE;
	info.depthCompareOp = bDepthTest ? compareOp : vk::CompareOp::eAlways;
	info.depthBoundsTestEnable = VK_FALSE;
	info.minDepthBounds = 0.0f;
	info.maxDepthBounds = 1.0f;
	info.stencilTestEnable = VK_FALSE;
	return info;
}
