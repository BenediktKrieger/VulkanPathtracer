#pragma once

#include <vk_types.h>
#include <vector>

class VulkanEngine
{
public:
	bool _isInitialized{false};
	int _frameNumber{0};

	vk::Extent2D _windowExtent{1920, 1080};

	struct SDL_Window *_window{nullptr};

	vma:: _allocator;

	vk::Instance _instance;
	vk::DebugUtilsMessengerEXT _debug_messenger;
	vk::PhysicalDevice _chosenGPU;
	vk::Device _device;

	vk::Semaphore _presentSemaphore, _renderSemaphore;
	vk::Fence _renderFence;

	vk::Queue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	vk::CommandPool _commandPool;
	vk::CommandBuffer _mainCommandBuffer;

	vk::RenderPass _renderPass;

	vk::SurfaceKHR _surface;
	vk::SwapchainKHR _swapchain;
	vk::Format _swachainImageFormat;

	std::vector<vk::Framebuffer> _framebuffers;
	std::vector<vk::Image> _swapchainImages;
	std::vector<vk::ImageView> _swapchainImageViews;

	// initializes everything in the engine
	void init();

	// shuts down the engine
	void cleanup();

	// draw loop
	void draw();

	// run main loop
	void run();

private:
	void init_vulkan();

	void init_swapchain();

	void init_default_renderpass();

	void init_framebuffers();

	void init_commands();

	void init_sync_structures();
};
