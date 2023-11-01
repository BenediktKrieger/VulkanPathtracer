#pragma once
#include <vk_engine.h>
#include <vk_types.h>
#include <optional>
#include <set>

namespace vkhelper {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);
    bool checkValidationLayerSupport(VulkanEngine *engine);
    bool isDeviceSuitable(VulkanEngine* engine, vk::PhysicalDevice pDevice);
    QueueFamilyIndices findQueueFamilies(VulkanEngine* engine, vk::PhysicalDevice pDevice);
    bool checkDeviceExtensionSupport(VulkanEngine* engine, vk::PhysicalDevice pDevice);
    SwapChainSupportDetails querySwapChainSupport(VulkanEngine* engine, vk::PhysicalDevice pDevice);
}