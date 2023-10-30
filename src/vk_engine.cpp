#include <SDL.h>
#include <SDL_vulkan.h>
#include <vk_engine.h>
#include <vk_types.h>
#include <vk_initializers.h>
#include <iostream>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

constexpr bool bUseValidationLayers = true;

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it.
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	_window = SDL_CreateWindow(
		"Vulkan Engine",
		_windowExtent.width,
		_windowExtent.height,
		window_flags);

	init_vulkan();

	init_swapchain();

	init_default_renderpass();

	init_framebuffers();

	init_commands();

	init_sync_structures();

	// everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{

		// make sure the gpu has stopped doing its things
		_device.waitIdle();

		_device.destroyCommandPool(_commandPool);

		// destroy sync objects
		_device.destroyFence(_renderFence);
		_device.destroySemaphore(_renderSemaphore);
		_device.destroySemaphore(_presentSemaphore);

		_device.destroySwapchainKHR(_swapchain);

		_device.destroyRenderPass(_renderPass);

		// destroy swapchain resources
		for (int i = 0; i < _framebuffers.size(); i++)
		{
			_device.destroyFramebuffer(_framebuffers[i]);

			_device.destroyImageView(_swapchainImageViews[i]);
		}

		_instance.destroySurfaceKHR(_surface);

		_device.destroy();
		_instance.destroyDebugUtilsMessengerEXT(_debug_messenger);
		_instance.destroy();

		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{
	// nothing yet
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	// main loop
	while (!bQuit)
	{
		// Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			// close the window when user alt-f4s or clicks the X button
			if (e.type == SDL_EventType::SDL_EVENT_QUIT)
			{
				bQuit = true;
			}
		}

		draw();
	}
}

void VulkanEngine::init_vulkan()
{
	bool validationLayersSupported = true;
	std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
	for (const char* layerName : _validationLayers) {
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound) {
			validationLayersSupported = false;
		}
	}

	if (bUseValidationLayers && !validationLayersSupported) {
		throw std::runtime_error("validation layers requested, but not available!");
	}
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = nullptr;
	SDL_Vulkan_GetInstanceExtensions(&glfwExtensionCount, glfwExtensions);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	vk::ApplicationInfo applicationInfo("VulkanBase", VK_MAKE_VERSION(0, 0 ,1), "VulkanEngine", 1, VK_API_VERSION_1_1);

	try {
		if(bUseValidationLayers){
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instanceCreateInfoChain{
				vk::InstanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo, _validationLayers, extensions),
				vk::DebugUtilsMessengerCreateInfoEXT({}, _messageSeverityFlags, _messageTypeFlags, debugMessageFunc)
			};
			_instance = vk::createInstance(instanceCreateInfoChain.get<vk::InstanceCreateInfo>());
		}else{
			vk::InstanceCreateInfo instanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo, {}, extensions);
			_instance = vk::createInstance(instanceCreateInfo);
		}
	}catch(std::exception& e) {
		std::cerr << "Exception Thrown: " << e.what();
	}

	VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);
}

void VulkanEngine::init_swapchain()
{
}

void VulkanEngine::init_default_renderpass()
{
}

void VulkanEngine::init_framebuffers()
{
}

void VulkanEngine::init_commands()
{
}

void VulkanEngine::init_sync_structures()
{
}