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
    class CameraData{
    public:
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewproj;
    };
    class FrameData {
    public: 
        vk::Semaphore _presentSemaphore, _renderSemaphore;
        vk::Fence _renderFence;	
        vk::CommandPool _commandPool;
        vk::CommandBuffer _mainCommandBuffer;
        AllocatedBuffer cameraBuffer;
	    vk::DescriptorSet globalDescriptor;
    };
    class AllocatedImage {
    public:
        vk::Image _image;
        vma::Allocation _allocation;
    };
    class PushConstants {
    public:
        glm::vec4 data;
        glm::mat4 matrix;
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
        vk::PipelineDepthStencilStateCreateInfo _depthStencil;
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
    class GeometryNode {
    public:
		uint64_t vertexBufferDeviceAddress;
		uint64_t indexBufferDeviceAddress;
	};
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);
    bool checkValidationLayerSupport(std::vector<const char *> &instanceLayers);
    bool isDeviceSuitable(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface, std::vector<const char *> &device_extensions);
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface);
    bool checkDeviceExtensionSupport(vk::PhysicalDevice &physicalDevice, std::vector<const char *> &device_extensions);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice &physicalDevice, vk::SurfaceKHR &surface);
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const vk::PresentModeKHR preferedPresentMode, const std::vector<vk::PresentModeKHR> &availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, vk::Extent2D &currentExtend);
    vk::ImageView createImageView(vk::Device &device, vk::Image &image, vk::Format &format, vk::ImageAspectFlags aspectFlags);
    AllocatedBuffer create_buffer(vma::Allocator &allocator, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::AllocationCreateFlags memoryFlags, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto);
    void copyBuffer(vk::Device &device, vk::CommandPool &pool, vk::Queue queue, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);
}