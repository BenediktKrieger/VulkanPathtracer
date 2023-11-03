#pragma once

#include <vk_types.h>
#include <vector>
#include <vk_utils.h>

class VulkanEngine
{
public:
	bool _isInitialized{false};
	uint32_t _frameNumber{0};
	int _selectedShader{0};

	vk::Extent2D _windowExtent{1920, 1080};

	struct SDL_Window *_window{nullptr};

	std::vector<const char *> _instanceLayers = {
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_LUNARG_monitor"};
	std::vector<const char *> _instanceExtensions = {};
	std::vector<const char *> _deviceExtensions = {
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
	vk::Device _device;

	vk::Semaphore _presentSemaphore, _renderSemaphore;
	vk::Fence _renderFence;

	vk::Queue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	vk::Queue _presentQueue;
	uint32_t _presentQueueFamily;

	vk::CommandPool _commandPool;
	vk::CommandBuffer _mainCommandBuffer;

	vk::RenderPass _renderPass;

	vk::SurfaceKHR _surface;
	vk::SwapchainKHR _swapchain;
	vk::Format _swachainImageFormat;

	std::vector<vk::Framebuffer> _framebuffers;
	std::vector<vk::Image> _swapchainImages;
	std::vector<vk::ImageView> _swapchainImageViews;

	vk::PipelineLayout _trianglePipelineLayout;

	vk::Pipeline _trianglePipeline;
	vk::Pipeline _redTrianglePipeline;

	vkutils::DeletionQueue _mainDeletionQueue;

	void init();
	void cleanup();
	void draw();
	void run();

private:
	void init_vulkan();

	void init_swapchain();

	void init_default_renderpass();

	void init_framebuffers();

	void init_commands();

	void init_sync_structures();

	void init_pipelines();

	vk::ShaderModule load_shader_module(vk::ShaderStageFlagBits type, std::string filePath);
};
