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
    class AllocatedImage {
    public:
        vk::Image _image;
        vk::ImageView _view;
        vma::Allocation _allocation;
    };
    class CameraData{
    public:
        glm::mat4 invView;
        glm::mat4 invProj;
        glm::mat4 viewproj;
        glm::vec4 camPos;
        glm::vec4 camDir;
    };
    class FrameData {
    public: 
        vk::Semaphore _presentSemaphore, _renderSemaphore;
        vk::Fence _renderFence;	
        vk::CommandPool _commandPool;
        vk::CommandBuffer _mainCommandBuffer;
	    vk::DescriptorSet _rasterizerDescriptor;
        vk::DescriptorSet _raytracerDescriptor;
        AllocatedImage _storageImage;
    };
    class PushConstants {
    public:
        glm::mat4 proj;
        glm::mat4 view;
        glm::mat4 model;
        uint32_t accumulatedFrames;
    };
    class PipelineBuilder {
    public:
        std::vector<vk::PipelineShaderStageCreateInfo> _shaderStages;
        vk::PipelineVertexInputStateCreateInfo _vertexInputInfo;
        vk::PipelineInputAssemblyStateCreateInfo _inputAssembly;
        vk::PipelineRasterizationStateCreateInfo _rasterizer;
        vk::PipelineColorBlendAttachmentState _colorBlendAttachment;
        vk::PipelineMultisampleStateCreateInfo _multisampling;
        vk::PipelineLayout _pipelineLayout;
        vk::PipelineDepthStencilStateCreateInfo _depthStencil;
        std::vector<vk::DynamicState> _dynamicStates;
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
		int32_t indexOffset;
		int32_t vertexOffset;
        int32_t baseColorTexture;
        int32_t metallicRoughnessTexture;
        int32_t normalTexture;
        int32_t occlusionTexture;
        int32_t emissiveTexture;
        int32_t specularGlossinessTexture;
        int32_t diffuseTexture;
        float alphaCutoff;
        float metallicFactor;
        float roughnessFactor;
        float baseColorFactor[4];
        uint32_t alphaMode;
        float pad[3];
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
    AllocatedBuffer createBuffer(vma::Allocator &allocator, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto, vma::AllocationCreateFlags memoryFlags = {});
    AllocatedBuffer deviceBufferFromData(vk::Device &device, vk::CommandPool &commandPool, vk::Queue &queue, vma::Allocator &allocator, void* data, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto, vma::AllocationCreateFlags memoryFlags = {});
    AllocatedBuffer hostBufferFromData(vma::Allocator &allocator, void* data, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto, vma::AllocationCreateFlags memoryFlags = {});
    AllocatedImage createImage(vma::Allocator &allocator, vk::ImageCreateInfo imageInfo, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto, vma::AllocationCreateFlags memoryFlags = {});
    void copyBuffer(vk::Device &device, vk::CommandPool &pool, vk::Queue queue, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);
    void setImageLayout(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageSubresourceRange subresourceRange, vk::PipelineStageFlags srcMask = vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlags dstMask = vk::PipelineStageFlagBits::eAllCommands);
    uint32_t alignedSize(uint32_t value, uint32_t alignment);
}