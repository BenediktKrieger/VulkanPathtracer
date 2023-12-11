#include <vk_utils.h>

vk::Pipeline vkutils::PipelineBuilder::build_pipeline(vk::Device device, vk::RenderPass pass)
{
    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setViewportCount(1);
    viewportState.setScissorCount(1);

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.setLogicOpEnable(VK_FALSE);
    colorBlending.setLogicOp(vk::LogicOp::eCopy);
    colorBlending.setAttachmentCount(1);
    colorBlending.setPAttachments(&_colorBlendAttachment);

    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.setDynamicStates(_dynamicStates);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.setStageCount((uint32_t)_shaderStages.size());
    pipelineInfo.setPStages(_shaderStages.data());
    pipelineInfo.setPVertexInputState(&_vertexInputInfo);
    pipelineInfo.setPInputAssemblyState(&_inputAssembly);
    pipelineInfo.setPDepthStencilState(&_depthStencil);
    pipelineInfo.setPViewportState(&viewportState);
    pipelineInfo.setPRasterizationState(&_rasterizer);
    pipelineInfo.setPMultisampleState(&_multisampling);
    pipelineInfo.setPColorBlendState(&colorBlending);
    pipelineInfo.setLayout(_pipelineLayout);
    pipelineInfo.setPDynamicState(&dynamicState);
    pipelineInfo.setRenderPass(pass);

    vk::Pipeline newPipeline;
    try
    {
        vk::Result result;
        std::tie(result, newPipeline) = device.createGraphicsPipeline({}, pipelineInfo);
        if (result != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create graphics Pipeline!");
        }
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception Thrown: " << e.what();
    }
    return newPipeline;
}

void vkutils::DeletionQueue::push_function(std::function<void()> &&function)
{
    deletors.push_back(function);
}

void vkutils::DeletionQueue::flush()
{
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
    {
        (*it)();
    }
    deletors.clear();
}

bool vkutils::QueueFamilyIndices::isComplete()
{
    return graphicsFamily.has_value() && presentFamily.has_value();
}

VKAPI_ATTR VkBool32 VKAPI_CALL vkutils::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    std::ostringstream message;
    message << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": "
            << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << ":\n";
    message << "\t"
            << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
    message << "\t"
            << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
    message << "\t"
            << "message         = <" << pCallbackData->pMessage << ">\n";
    if (0 < pCallbackData->queueLabelCount)
    {
        message << "\t"
                << "Queue Labels:\n";
        for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++)
        {
            message << "\t\t"
                    << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->cmdBufLabelCount)
    {
        message << "\t"
                << "CommandBuffer Labels:\n";
        for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
        {
            message << "\t\t"
                    << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->objectCount)
    {
        message << "\t"
                << "Objects:\n";
        for (uint8_t i = 0; i < pCallbackData->objectCount; i++)
        {
            message << "\t\t"
                    << "Object " << i << "\n";
            message << "\t\t\t"
                    << "objectType   = "
                    << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
            message << "\t\t\t"
                    << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
            if (pCallbackData->pObjects[i].pObjectName)
            {
                message << "\t\t\t"
                        << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
            }
        }
    }
    std::cout << message.str() << std::endl;
    return VK_FALSE;
}

bool vkutils::checkValidationLayerSupport(std::vector<const char *> &instanceLayers)
{
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
    for (const char *layerName : instanceLayers)
    {
        bool layerFound = false;
        for (const auto &layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }
        if (!layerFound)
        {
            return false;
        }
    }
    return true;
}

bool vkutils::isDeviceSuitable(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface, std::vector<const char *> &device_extensions)
{
    vkutils::QueueFamilyIndices indices = vkutils::findQueueFamilies(physicalDevice, surface);
    bool extensionsSupported = vkutils::checkDeviceExtensionSupport(physicalDevice, device_extensions);
    bool swapChainAdequate = false;

    if (extensionsSupported)
    {
        vkutils::SwapChainSupportDetails swapChainSupport = vkutils::querySwapChainSupport(physicalDevice, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    // auto m_deviceProperties2 = pDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR, vk::PhysicalDeviceAccelerationStructurePropertiesKHR, vk::PhysicalDeviceDescriptorIndexingProperties>();
    auto m_deviceFeatures2 = physicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR, vk::PhysicalDeviceAccelerationStructureFeaturesKHR, vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceDescriptorIndexingFeatures>();
    bool supportsRaytracingFeatures =
        m_deviceFeatures2.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
        m_deviceFeatures2.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline &&
        m_deviceFeatures2.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure &&
        m_deviceFeatures2.get<vk::PhysicalDeviceBufferDeviceAddressFeatures>().bufferDeviceAddress &&
        m_deviceFeatures2.get<vk::PhysicalDeviceDescriptorIndexingFeatures>().runtimeDescriptorArray;

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportsRaytracingFeatures;
}

vkutils::QueueFamilyIndices vkutils::findQueueFamilies(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface)
{
    vkutils::QueueFamilyIndices indices;
    std::vector<vk::QueueFamilyProperties> queueFamilies = physicalDevice.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); i++)
    {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphicsFamily = i;
        }
        if (physicalDevice.getSurfaceSupportKHR(i, surface))
        {
            indices.presentFamily = i;
        }
        if (indices.isComplete())
        {
            break;
        }
    }
    return indices;
}

bool vkutils::checkDeviceExtensionSupport(vk::PhysicalDevice &physicalDevice, std::vector<const char *> &device_extensions)
{
    std::vector<vk::ExtensionProperties> availableExtensions = physicalDevice.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(device_extensions.begin(), device_extensions.end());
    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

vkutils::SwapChainSupportDetails vkutils::querySwapChainSupport(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface)
{
    vkutils::SwapChainSupportDetails details;
    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    details.formats = physicalDevice.getSurfaceFormatsKHR(surface);
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    return details;
}

vk::SurfaceFormatKHR vkutils::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
{
    for (const auto &availableFormat : availableFormats)
    {
        if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR vkutils::chooseSwapPresentMode(const vk::PresentModeKHR preferedPresentMode, const std::vector<vk::PresentModeKHR> &availablePresentModes)
{
    for (const auto &availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == preferedPresentMode)
        {
            return availablePresentMode;
        }
    }
    return vk::PresentModeKHR::eMailbox;
}

vk::Extent2D vkutils::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, vk::Extent2D &currentExtend)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width = currentExtend.width;
        int height = currentExtend.height;
        ;
        vk::Extent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)};
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

vk::CommandBuffer vkutils::getCommandBuffer(vk::Core &core, vk::CommandBufferLevel level, uint32_t count){
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = core._cmdPool;
    allocInfo.commandBufferCount = 1;
    return core._device.allocateCommandBuffers(allocInfo).front();
}

vk::ImageView vkutils::createImageView(vk::Core &core, vk::Image &image, vk::Format &format, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo createInfo({}, image, vk::ImageViewType::e2D, format, {}, vk::ImageSubresourceRange(aspectFlags, 0, 1, 0, 1));
    vk::ImageView imageView;
    try
    {
        imageView = core._device.createImageView(createInfo);
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception Thrown: " << e.what();
    }
    return imageView;
}

vkutils::AllocatedBuffer vkutils::createBuffer(vk::Core &core, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage, vma::AllocationCreateFlags memoryFlags)
{
    vk::BufferCreateInfo bufferInfo;
	bufferInfo.size = size;
	bufferInfo.usage = bufferUsage;
	
	vma::AllocationCreateInfo bufferAllocInfo;
	bufferAllocInfo.usage = memoryUsage;
	bufferAllocInfo.flags = memoryFlags;

    vkutils::AllocatedBuffer allocatedBuffer;
	std::tie(allocatedBuffer._buffer, allocatedBuffer._allocation) = core._allocator.createBuffer(bufferInfo, bufferAllocInfo);
    return allocatedBuffer;
}

vkutils::AllocatedBuffer vkutils::deviceBufferFromData(vk::Core &core, void* data, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage, vma::AllocationCreateFlags memoryFlags)
{
    vkutils::AllocatedBuffer stagingBuffer = vkutils::createBuffer(core, size, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

    void* mapped = core._allocator.mapMemory(stagingBuffer._allocation);
	    memcpy(mapped, data, size);
	core._allocator.unmapMemory(stagingBuffer._allocation);

    vkutils::AllocatedBuffer buffer = vkutils::createBuffer(core, size, vk::BufferUsageFlagBits::eTransferDst | bufferUsage, memoryUsage, memoryFlags);
    vkutils::copyBuffer(core, stagingBuffer._buffer, buffer._buffer, size);
    core._allocator.destroyBuffer(stagingBuffer._buffer, stagingBuffer._allocation);

    return buffer;
}

vkutils::AllocatedBuffer vkutils::hostBufferFromData(vk::Core &core, void* data, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage, vma::AllocationCreateFlags memoryFlags)
{
    vkutils::AllocatedBuffer buffer = vkutils::createBuffer(core, size, bufferUsage, memoryUsage, memoryFlags);

    void* mapped = core._allocator.mapMemory(buffer._allocation);
	    memcpy(mapped, data, size);
	core._allocator.unmapMemory(buffer._allocation);

    return buffer;
}

vkutils::AllocatedImage vkutils::createImage(vk::Core &core, vk::ImageCreateInfo imageInfo, vk::ImageAspectFlags aspectFlags, vma::MemoryUsage memoryUsage, vma::AllocationCreateFlags memoryFlags)
{
    vma::AllocationCreateInfo imageAllocInfo;
	imageAllocInfo.usage = memoryUsage;
	imageAllocInfo.flags = memoryFlags;

    vkutils::AllocatedImage allocatedImage;
	std::tie(allocatedImage._image, allocatedImage._allocation) = core._allocator.createImage(imageInfo, imageAllocInfo);
    allocatedImage._view = vkutils::createImageView(core, allocatedImage._image, imageInfo.format, aspectFlags);

    return allocatedImage;
}

vkutils::AllocatedImage vkutils::imageFromData(vk::Core &core, void* data, vk::ImageCreateInfo imageInfo, vk::ImageAspectFlags aspectFlags, vma::MemoryUsage memoryUsage, vma::AllocationCreateFlags memoryFlags)
{
    vk::DeviceSize pixelSize = 4;
    switch (imageInfo.format)
    {
        case vk::Format::eR32G32B32A32Sfloat:
            pixelSize = 16;
            break;
        case vk::Format::eR32G32B32Sfloat:
            pixelSize = 12;
            break;
    }
    vkutils::AllocatedBuffer srcBuffer = hostBufferFromData(core, data, imageInfo.extent.width * imageInfo.extent.height * pixelSize, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eAutoPreferHost, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
    
    imageInfo.usage |= vk::ImageUsageFlagBits::eTransferDst;
    vkutils::AllocatedImage dstImage = createImage(core, imageInfo, aspectFlags, memoryUsage, memoryFlags);

    copyImageBuffer(core, srcBuffer._buffer, dstImage._image, imageInfo.extent.width, imageInfo.extent.height);

    core._allocator.destroyBuffer(srcBuffer._buffer, srcBuffer._allocation);
    return dstImage;
}

void vkutils::copyBuffer(vk::Core &core, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = core._cmdPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer commandBuffer;
    std::vector<vk::CommandBuffer> commandBuffers = core._device.allocateCommandBuffers(allocInfo);
    commandBuffer = commandBuffers[0];

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);

        vk::BufferCopy copyRegion{};
        copyRegion.size = size;
        commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    core._graphicsQueue.submit(submitInfo, nullptr);
    core._graphicsQueue.waitIdle();

    core._device.freeCommandBuffers(core._cmdPool, 1, &commandBuffer);
}

void vkutils::copyImageBuffer(vk::Core &core, vk::Buffer srcBuffer, vk::Image dstImage, uint32_t width, uint32_t height)
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = core._cmdPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer cmd = core._device.allocateCommandBuffers(allocInfo).front();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmd.begin(beginInfo);

        setImageLayout(cmd, dstImage, vk::ImageLayout::eUndefined,  vk::ImageLayout::eTransferDstOptimal, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        vk::BufferImageCopy copyRegion;
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageOffset = vk::Offset3D{0, 0, 0};
        copyRegion.imageExtent = vk::Extent3D{width, height, 1};
        cmd.copyBufferToImage(srcBuffer, dstImage,vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

        setImageLayout(cmd, dstImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    core._graphicsQueue.submit(submitInfo, nullptr);
    core._graphicsQueue.waitIdle();

    core._device.freeCommandBuffers(core._cmdPool, 1, &cmd);
}

void vkutils::setImageLayout(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageSubresourceRange subresourceRange, vk::PipelineStageFlags srcMask, vk::PipelineStageFlags dstMask)
{
    vk::ImageMemoryBarrier barrier;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.image               = image;
    barrier.subresourceRange    = subresourceRange;

    switch (oldLayout)
    {
        case vk::ImageLayout::eUndefined:
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            break;

        case vk::ImageLayout::ePreinitialized:
            barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
            break;

        case vk::ImageLayout::eColorAttachmentOptimal:
            barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;

        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;

        case vk::ImageLayout::eTransferSrcOptimal:
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
            break;

        case vk::ImageLayout::eTransferDstOptimal:
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;

        case vk::ImageLayout::eShaderReadOnlyOptimal:
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        default:
            break;
    }

    switch (newLayout)
    {
        case vk::ImageLayout::eTransferDstOptimal:
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;

        case vk::ImageLayout::eTransferSrcOptimal:
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
            break;

        case vk::ImageLayout::eColorAttachmentOptimal:
            barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
            break;

        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            barrier.dstAccessMask = barrier.dstAccessMask | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;

        case vk::ImageLayout::eShaderReadOnlyOptimal:
            if (barrier.srcAccessMask == vk::AccessFlagBits::eNone)
            {
                barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
            }
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        default:
            break;
    }
    cmd.pipelineBarrier(srcMask, dstMask, {}, {}, {}, barrier);
}

uint32_t vkutils::alignedSize(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}
