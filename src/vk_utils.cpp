#include <vk_utils.h>

vk::Pipeline vkutils::PipelineBuilder::build_pipeline(vk::Device device, vk::RenderPass pass)
{
	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.setViewportCount(1);
	viewportState.setPViewports(&_viewport);
	viewportState.setScissorCount(1);
	viewportState.setPScissors(&_scissor);

	vk::PipelineColorBlendStateCreateInfo colorBlending;
	colorBlending.setLogicOpEnable(VK_FALSE);
	colorBlending.setLogicOp(vk::LogicOp::eCopy);
	colorBlending.setAttachmentCount(1);
	colorBlending.setPAttachments(&_colorBlendAttachment);
    
	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.setStageCount((uint32_t) _shaderStages.size());
	pipelineInfo.setPStages(_shaderStages.data());
	pipelineInfo.setPVertexInputState(&_vertexInputInfo);
	pipelineInfo.setPInputAssemblyState(&_inputAssembly);
	pipelineInfo.setPViewportState(&viewportState);
	pipelineInfo.setPRasterizationState(&_rasterizer);
	pipelineInfo.setPMultisampleState(&_multisampling);
	pipelineInfo.setPColorBlendState(&colorBlending);
	pipelineInfo.setLayout(_pipelineLayout);
	pipelineInfo.setRenderPass(pass);
    
    vk::Pipeline newPipeline;
	try
    {
        vk::Result result;
        std::tie(result, newPipeline) = device.createGraphicsPipeline( nullptr, pipelineInfo);
        if(result != vk::Result::eSuccess)
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
void vkutils::DeletionQueue::push_function(std::function<void()>&& function) 
{
    deletors.push_back(function);
}
void vkutils::DeletionQueue::flush() 
{
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
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
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

vk::PresentModeKHR vkutils::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes)
{
    for (const auto &availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox)
        {
            return availablePresentMode;
        }
    }
    return vk::PresentModeKHR::eFifo;
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

vk::ImageView vkutils::createImageView(vk::Device &device, vk::Image &image, vk::Format &format, vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo createInfo({}, image, vk::ImageViewType::e2D, format, {}, vk::ImageSubresourceRange(aspectFlags, 0, 1, 0, 1));
    vk::ImageView imageView;
    try
    {
        imageView = device.createImageView(createInfo);
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception Thrown: " << e.what();
    }
    return imageView;
}