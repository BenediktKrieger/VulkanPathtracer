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

		// // make sure the gpu has stopped doing its things
		// _device.waitIdle();

		// vkDestroyCommandPool(_device, _commandPool, nullptr);
		// _device.destroyCommandPool(_commandPool, nullptr);

		// // destroy sync objects
		// vkDestroyFence(_device, _renderFence, nullptr);
		// vkDestroySemaphore(_device, _renderSemaphore, nullptr);
		// vkDestroySemaphore(_device, _presentSemaphore, nullptr);

		// vkDestroySwapchainKHR(_device, _swapchain, nullptr);

		// vkDestroyRenderPass(_device, _renderPass, nullptr);

		// // destroy swapchain resources
		// for (int i = 0; i < _framebuffers.size(); i++)
		// {
		// 	vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);

		// 	vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
		// }

		// vkDestroySurfaceKHR(_instance, _surface, nullptr);

		// vkDestroyDevice(_device, nullptr);
		// vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		// vkDestroyInstance(_instance, nullptr);

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
