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
    bool checkValidationLayerSupport(std::vector<const char*> &instanceLayers);
    bool isDeviceSuitable(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface, std::vector<const char *> &device_extensions);
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface);
    bool checkDeviceExtensionSupport(vk::PhysicalDevice &physicalDevice, std::vector<const char *> &device_extensions);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface);
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, vk::Extent2D &currentExtend);
    vk::ImageView createImageView(vk::Device &device, vk::Image &image, vk::Format &format, vk::ImageAspectFlags aspectFlags);
}