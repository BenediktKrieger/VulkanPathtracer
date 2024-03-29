#pragma once
#include <Core.h>
#include <optional>
#include <set>
#include <functional>
#include <deque>

namespace vkutils
{
    class Settings {
    public:
        // Renderer Settings
        int renderer;
        // Camera Settings
        glm::vec3 cam_pos;
        float fov;
        glm::vec3 cam_dir;
        uint32_t cam_mode;
        float speed;
        bool auto_exposure;
        float exposure;
        // Pathtracer Settings
        bool accumulate;
        uint32_t min_samples;
        bool limit_samples;
        uint32_t max_samples;
        uint32_t reflection_recursion;
        uint32_t refraction_recursion;
        float ambient_multiplier;
        // Sampling
        bool mips;
        float mips_sensitivity;
        //Tonemapping
        uint32_t tm_operator;
        float tm_param_linear;
        float tm_param_reinhard;
        float tm_params_aces[5];
        float tm_param_uchimura[6];
        float tm_param_lottes[5];
    };
    class ImageStats {
    public:
        uint32_t histogram[256];
        float average;
    };
    class Shadersettings {
    public:
        uint32_t accumulate;
        uint32_t min_samples;
        uint32_t limit_samples;
        uint32_t max_samples;
        uint32_t reflection_recursion;
        uint32_t refraction_recursion;
        float ambient_multiplier;
        uint32_t auto_exposure;
        float exposure;
        uint32_t mips;
        float mips_sensitivity;
        uint32_t tonemapper;
        float tonemapper_param_1;
        float tonemapper_param_2;
        float tonemapper_param_3;
        float tonemapper_param_4;
        float tonemapper_param_5;
        float tonemapper_param_6;
    };
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
        vk::DescriptorSet _computeDescriptor;
        AllocatedImage _storageImage;
        AllocatedBuffer _imageStats;
    };
    class PushConstants {
    public:
        glm::mat4 proj;
        glm::mat4 view;
        glm::mat4 model;
        uint32_t accumulatedFrames;
    };
    class ComputeConstants {
    public:
        float deltaTime;
        uint32_t width;
        uint32_t height;
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
    class Material {
    public:
		uint32_t indexOffset;
		uint32_t vertexOffset;
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
        float emissiveFactor[4];
        float emissiveStrength;
        float transmissionFactor;
	    float ior;
        uint32_t alphaMode;
        glm::mat4 modelMatrix;
	};
    class LightProxy {
    public:
        enum GeoType
        {
            SPHERE,
            AABB,
            EMPTY
        };
        float min[3];
        GeoType geoType;
        float max[3];
        float radius;
        float center[3];
        float radiosity;
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
    vk::CommandBuffer getCommandBuffer(vk::Core &core, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary, uint32_t count = 1);
    vk::ImageView createImageView(vk::Core &core, vk::Image &image, vk::Format &format, vk::ImageAspectFlags aspectFlags);
    AllocatedBuffer createBuffer(vk::Core &core, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto, vma::AllocationCreateFlags memoryFlags = {});
    AllocatedBuffer deviceBufferFromData(vk::Core &core, void* data, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto, vma::AllocationCreateFlags memoryFlags = {});
    AllocatedBuffer hostBufferFromData(vk::Core &core, void* data, vk::DeviceSize size, vk::BufferUsageFlags bufferUsage, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto, vma::AllocationCreateFlags memoryFlags = {});
    AllocatedImage createImage(vk::Core &core, vk::ImageCreateInfo imageInfo, vk::ImageAspectFlags aspectFlags, vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto, vma::AllocationCreateFlags memoryFlags = {});
    vkutils::AllocatedImage imageFromData(vk::Core &core, void* data, vk::ImageCreateInfo imageInfo, vk::ImageAspectFlags aspectFlags, vma::MemoryUsage memoryUsage, vma::AllocationCreateFlags memoryFlags = {});
    void copyBuffer(vk::Core &core, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);
    void copyImageBuffer(vk::Core &core, vk::Buffer srcBuffer, vk::Image dstImage, uint32_t width, uint32_t height);
    void setImageLayout(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageSubresourceRange subresourceRange, vk::PipelineStageFlags srcMask = vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlags dstMask = vk::PipelineStageFlagBits::eAllCommands);
    vk::TransformMatrixKHR getTransformMatrixKHR(glm::mat4 mat);
    uint32_t alignedSize(uint32_t value, uint32_t alignment);
}