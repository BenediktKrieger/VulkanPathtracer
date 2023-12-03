#pragma once

#include <vk_types.h>
#include <vector>

namespace vk {
    class Core
    {
    public:
        std::vector<const char *> _instanceLayers = {
            "VK_LAYER_KHRONOS_validation",
            "VK_LAYER_LUNARG_monitor"};
        std::vector<const char *> _instanceExtensions = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
        };
        std::vector<const char *> _deviceExtensions = {
            VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
        };
        vk::DebugUtilsMessageSeverityFlagsEXT _messageSeverityFlags = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        vk::DebugUtilsMessageTypeFlagsEXT _messageTypeFlags = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

        vk::Extent2D _windowExtent{1920, 1080};
        struct SDL_Window* _window{nullptr};
        vk::DynamicLoader _dl;
        vma::Allocator _allocator;
        vk::DebugUtilsMessengerEXT _debug_messenger;

        vk::Instance _instance;
        vk::PhysicalDevice _chosenGPU;
        vk::Device _device;

        vk::SurfaceKHR _surface;
        vk::SwapchainKHR _swapchain;
        vk::Format _swapchainImageFormat;
        std::vector<vk::Framebuffer> _framebuffers;
        std::vector<vk::Image> _swapchainImages;
        std::vector<vk::ImageView> _swapchainImageViews;

        vk::Queue _graphicsQueue;
        uint32_t _graphicsQueueFamily;
        vk::Queue _presentQueue;
        uint32_t _presentQueueFamily;

        vk::CommandPool _cmdPool;

        Core();
        ~Core();
    };
}