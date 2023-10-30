#include <vk_helper.h>

VKAPI_ATTR VkBool32 VKAPI_CALL vkhelper::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
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
    if (0 < pCallbackData->queueLabelCount){
        message << "\t"
                << "Queue Labels:\n";
        for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++){
            message << "\t\t"
                    << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->cmdBufLabelCount){
        message << "\t"
                << "CommandBuffer Labels:\n";
        for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++){
            message << "\t\t"
                    << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->objectCount){
        message << "\t"
                << "Objects:\n";
        for (uint8_t i = 0; i < pCallbackData->objectCount; i++){
            message << "\t\t"
                    << "Object " << i << "\n";
            message << "\t\t\t"
                    << "objectType   = "
                    << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
            message << "\t\t\t"
                    << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
            if (pCallbackData->pObjects[i].pObjectName){
                message << "\t\t\t"
                        << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
            }
        }
    }
    std::cout << message.str() << std::endl;
    return VK_FALSE;
}

bool vkhelper::isDeviceSuitable(VulkanEngine *engine, vk::PhysicalDevice pDevice)
{
    QueueFamilyIndices indices = vkhelper::findQueueFamilies(engine, pDevice);
    bool extensionsSupported = vkhelper::checkDeviceExtensionSupport(engine, pDevice);
    bool swapChainAdequate = false;

    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = vkhelper::querySwapChainSupport(engine, pDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    //auto m_deviceProperties2 = pDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR, vk::PhysicalDeviceAccelerationStructurePropertiesKHR, vk::PhysicalDeviceDescriptorIndexingProperties>();
	auto m_deviceFeatures2 = pDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR, vk::PhysicalDeviceAccelerationStructureFeaturesKHR, vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceDescriptorIndexingFeatures>();
    bool supportsRaytracingFeatures = 
        m_deviceFeatures2.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy && 
        m_deviceFeatures2.get<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>().rayTracingPipeline && 
        m_deviceFeatures2.get<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>().accelerationStructure && 
        m_deviceFeatures2.get<vk::PhysicalDeviceBufferDeviceAddressFeatures>().bufferDeviceAddress && 
        m_deviceFeatures2.get<vk::PhysicalDeviceDescriptorIndexingFeatures>().runtimeDescriptorArray;

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportsRaytracingFeatures;
}

vkhelper::QueueFamilyIndices vkhelper::findQueueFamilies(VulkanEngine *engine, vk::PhysicalDevice pDevice)
{
    vkhelper::QueueFamilyIndices indices;
    std::vector<vk::QueueFamilyProperties> queueFamilies = pDevice.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); i++)
    {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphicsFamily = i;
        }
        if (pDevice.getSurfaceSupportKHR(i, engine->_surface))
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

bool vkhelper::checkDeviceExtensionSupport(VulkanEngine *engine, vk::PhysicalDevice pDevice)
{
    std::vector<vk::ExtensionProperties> availableExtensions = pDevice.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(engine->_deviceExtensions.begin(), engine->_deviceExtensions.end());
    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

vkhelper::SwapChainSupportDetails vkhelper::querySwapChainSupport(VulkanEngine *engine, vk::PhysicalDevice pDevice)
{
    vkhelper::SwapChainSupportDetails details;
    details.capabilities = pDevice.getSurfaceCapabilitiesKHR(engine->_surface);
    details.formats = pDevice.getSurfaceFormatsKHR(engine->_surface);
    details.presentModes = pDevice.getSurfacePresentModesKHR(engine->_surface);
    return details;
}