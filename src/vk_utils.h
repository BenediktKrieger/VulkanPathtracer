#pragma once
#include <vk_types.h>
#include <optional>
#include <set>
#include <functional>
#include <deque>


namespace vkutils
{   
    class AllocatedBuffer {
    public:
        vk::Buffer _buffer;
        vma::Allocation _allocation;
    };
    class PipelineBuilder {
    public:
        std::vector<vk::PipelineShaderStageCreateInfo> _shaderStages;
        vk::PipelineVertexInputStateCreateInfo _vertexInputInfo;
        vk::PipelineInputAssemblyStateCreateInfo _inputAssembly;
        vk::Viewport _viewport;
        vk::Rect2D _scissor;
        vk::PipelineRasterizationStateCreateInfo _rasterizer;
        vk::PipelineColorBlendAttachmentState _colorBlendAttachment;
        vk::PipelineMultisampleStateCreateInfo _multisampling;
        vk::PipelineLayout _pipelineLayout;
        vk::Pipeline build_pipeline(vk::Device device, vk::RenderPass pass);
    };
    class DeletionQueue
    {
    public:
        std::deque<std::function<void()>> deletors;
        void push_function(std::function<void()>&& function);
        void flush();
    };
    class QueueFamilyIndices
    {
    public:
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete();
    };
    class SwapChainSupportDetails
    {
    public:
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);
    bool checkValidationLayerSupport(std::vector<const char *> &instanceLayers);
    bool isDeviceSuitable(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface, std::vector<const char *> &device_extensions);
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface);
    bool checkDeviceExtensionSupport(vk::PhysicalDevice &physicalDevice, std::vector<const char *> &device_extensions);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface);
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, vk::Extent2D &currentExtend);
    vk::ImageView createImageView(vk::Device &device, vk::Image &image, vk::Format &format, vk::ImageAspectFlags aspectFlags);
}