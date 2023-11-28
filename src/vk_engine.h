#pragma once

#include <vk_types.h>
#include <vector>
#include <vk_utils.h>
#include <vk_model.h>
#include <vk_shaderConverter.h>
#include <Camera.h>

constexpr unsigned int FRAME_OVERLAP = 1;

class VulkanEngine
{
public:
	bool _isInitialized{false};
	bool _framebufferResized{false};
	uint32_t _frameNumber{0};
	int _selectedShader{0};
	vkutils::PushConstants PushConstants;

	vk::Extent2D _windowExtent{1920, 1080};

	struct SDL_Window* _window{nullptr};

	Camera cam;

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
		VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};

	vk::DynamicLoader _dl;
	vk::DebugUtilsMessageSeverityFlagsEXT _messageSeverityFlags = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	vk::DebugUtilsMessageTypeFlagsEXT _messageTypeFlags = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;

	vma::Allocator _allocator;

	vk::Instance _instance;
	vk::DebugUtilsMessengerEXT _debug_messenger;
	vk::PhysicalDevice _chosenGPU;
	vk::PhysicalDeviceRayTracingPipelinePropertiesKHR _raytracingPipelineProperties;
	vk::Device _device;

	vkutils::FrameData _frames[FRAME_OVERLAP];

	vk::Queue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	vk::Queue _presentQueue;
	uint32_t _presentQueueFamily;

	vk::RenderPass _renderPass;

	vk::SurfaceKHR _surface;
	vk::SwapchainKHR _swapchain;
	vk::Format _swachainImageFormat;

	std::vector<vk::Framebuffer> _framebuffers;
	std::vector<vk::Image> _swapchainImages;
	std::vector<vk::ImageView> _swapchainImageViews;

	vk::PipelineLayout _rasterizerPipelineLayout;
	vk::PipelineLayout _raytracerPipelineLayout;
	vkutils::AllocatedBuffer _raygenShaderBindingTable;
	vkutils::AllocatedBuffer _missShaderBindingTable;
	vkutils::AllocatedBuffer _hitShaderBindingTable;

	vk::Pipeline _rasterizerPipeline;
	vk::Pipeline _raytracerPipeline;
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> _shaderGroups;
	
	Model _triangleModel;

	vk::ImageView _depthImageView;
	vkutils::AllocatedImage _depthImage;
	vk::Format _depthFormat;

	vk::DescriptorPool _rasterizerDescriptorPool;
	vk::DescriptorPool _raytracerDescriptorPool;
	vk::DescriptorSetLayout _rasterizerSetLayout;
	vk::DescriptorSetLayout _raytracerSetLayout;

	vkutils::AllocatedBuffer _bottomLevelASBuffer;
	vk::DeviceAddress _bottomLevelDeviceAddress;
	vk::AccelerationStructureKHR _bottomLevelAS;
	
	vkutils::AllocatedBuffer _topLevelASBuffer;
	vk::DeviceAddress _topLevelDeviceAddress;
	vk::AccelerationStructureKHR _topLevelAS;

	vkutils::AllocatedImage _storageImage;
	
	std::vector<vkutils::GeometryNode> _materials;
	vkutils::AllocatedBuffer _materialBuffer;

	vkutils::DeletionQueue _resizeDeletionQueue;
	vkutils::DeletionQueue _mainDeletionQueue;

	void init();
	void cleanup();
	void draw();
	void run();

	vkutils::FrameData& get_current_frame();

private:
	void init_vulkan();

	void init_swapchain();

	void init_default_renderpass();

	void init_framebuffers();

	void init_commands();

	void init_sync_structures();

	void init_descriptors();

	void init_pipelines();

	void load_models();

	void upload_model(Model& model);

	void init_bottom_level_acceleration_structure(Model &model);

	void init_top_level_acceleration_structure();

	vkutils::AllocatedImage createStorageImage();

	void createShaderBindingTable();

	void updateBuffers();

	void recreateSwapchain();

	vk::ShaderModule load_shader_module(vk::ShaderStageFlagBits type, std::string filePath);
};
