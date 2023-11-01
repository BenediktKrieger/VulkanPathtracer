#include <SDL.h>
#include <SDL_vulkan.h>
#include <vk_engine.h>
#include <vk_initializers.h>
#include <vk_helper.h>
#include <iostream>

constexpr bool bUseValidationLayers = true;

void VulkanEngine::init()
{
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
		_device.waitIdle();
		_device.destroyCommandPool(_commandPool);
		_device.destroyFence(_renderFence);
		_device.destroySemaphore(_renderSemaphore);
		_device.destroySemaphore(_presentSemaphore);
		_device.destroySwapchainKHR(_swapchain);
		_device.destroyRenderPass(_renderPass);
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
	// //check if window is minimized and skip drawing
	// if (SDL_GetWindowFlags(_window) & SDL_WINDOW_MINIMIZED)
	// 	return;

	// //wait until the gpu has finished rendering the last frame. Timeout of 1 second
	// VK_CHECK(vkWaitForFences(_device, 1, &_renderFence, true, 1000000000));
	// VK_CHECK(vkResetFences(_device, 1, &_renderFence));

	// //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	// VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));

	// //request image from the swapchain
	// uint32_t swapchainImageIndex;
	// VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, _presentSemaphore, nullptr, &swapchainImageIndex));

	// //naming it cmd for shorter writing
	// VkCommandBuffer cmd = _mainCommandBuffer;

	// //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	// VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// //make a clear-color from frame number. This will flash with a 120 frame period.
	// VkClearValue clearValue;
	// float flash = abs(sin(_frameNumber / 120.f));
	// clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

	// //start the main renderpass. 
	// //We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	// VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

	// //connect clear values
	// rpInfo.clearValueCount = 1;
	// rpInfo.pClearValues = &clearValue;

	// vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	// //once we start adding rendering commands, they will go here

	// //finalize the render pass
	// vkCmdEndRenderPass(cmd);
	// //finalize the command buffer (we can no longer add commands, but it can now be executed)
	// VK_CHECK(vkEndCommandBuffer(cmd));

	// //prepare the submission to the queue. 
	// //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	// //we will signal the _renderSemaphore, to signal that rendering has finished

	// VkSubmitInfo submit = vkinit::submit_info(&cmd);
	// VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	// submit.pWaitDstStageMask = &waitStage;

	// submit.waitSemaphoreCount = 1;
	// submit.pWaitSemaphores = &_presentSemaphore;

	// submit.signalSemaphoreCount = 1;
	// submit.pSignalSemaphores = &_renderSemaphore;

	// //submit command buffer to the queue and execute it.
	// // _renderFence will now block until the graphic commands finish execution
	// VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFence));

	// //prepare present
	// // this will put the image we just rendered to into the visible window.
	// // we want to wait on the _renderSemaphore for that, 
	// // as its necessary that drawing commands have finished before the image is displayed to the user
	// VkPresentInfoKHR presentInfo = vkinit::present_info();

	// presentInfo.pSwapchains = &_swapchain;
	// presentInfo.swapchainCount = 1;

	// presentInfo.pWaitSemaphores = &_renderSemaphore;
	// presentInfo.waitSemaphoreCount = 1;

	// presentInfo.pImageIndices = &swapchainImageIndex;

	// VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	// //increase the number of frames drawn
	// _frameNumber++;
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
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = _dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
	if (bUseValidationLayers && !vkhelper::checkValidationLayerSupport(_instanceLayers))
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}
	unsigned int sdl_extensions_count = 0;
	SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count, NULL);
	_instanceExtensions.resize(sdl_extensions_count);
	SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count, _instanceExtensions.data());

	vk::ApplicationInfo applicationInfo("Vulkan Pathtracer", VK_MAKE_VERSION(0, 0, 1), "VulkanEngine", 1, VK_API_VERSION_1_1);

	try
	{
		if (bUseValidationLayers)
		{
			_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instanceCreateInfoChain{
				vk::InstanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo, _instanceLayers, _instanceExtensions),
				vk::DebugUtilsMessengerCreateInfoEXT({}, _messageSeverityFlags, _messageTypeFlags, vkhelper::debugCallback)};
			_instance = vk::createInstance(instanceCreateInfoChain.get<vk::InstanceCreateInfo>());
		}
		else
		{
			vk::InstanceCreateInfo instanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo, {}, _instanceExtensions);
			_instance = vk::createInstance(instanceCreateInfo);
		}
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);

	if (bUseValidationLayers)
	{
		vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo({}, _messageSeverityFlags, _messageTypeFlags, vkhelper::debugCallback);
		try
		{
			_debug_messenger = _instance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo, nullptr);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
	}

	SDL_Vulkan_CreateSurface(_window, _instance, reinterpret_cast<VkSurfaceKHR *>(&_surface));

	std::vector<vk::PhysicalDevice> devices = _instance.enumeratePhysicalDevices();
	bool deviceFound = false;
	for (auto &device : devices)
	{
		if (vkhelper::isDeviceSuitable(device, _surface, _deviceExtensions))
		{
			deviceFound = true;
			_chosenGPU = device;
			break;
		}
	}
	if (!deviceFound)
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	vkhelper::QueueFamilyIndices indices = vkhelper::findQueueFamilies(_chosenGPU, _surface);
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamily, 1, &queuePriority);
		queueCreateInfos.push_back(queueCreateInfo);
	}

	vk::PhysicalDeviceFeatures deviceFeatures;
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	// MacOS portability extension
	std::vector<vk::ExtensionProperties> extensionProperties =  _chosenGPU.enumerateDeviceExtensionProperties();
	for(auto extensionProperty : extensionProperties){
		if(std::string(extensionProperty.extensionName.data()) == std::string(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
			_deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
	}

	vk::DeviceCreateInfo createInfo;
	if (bUseValidationLayers)
	{
		createInfo = vk::DeviceCreateInfo({}, queueCreateInfos, _instanceLayers, _deviceExtensions, {});
	}
	else
	{
		createInfo = vk::DeviceCreateInfo({}, queueCreateInfos, {}, _deviceExtensions, {});
	}
	vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR, vk::PhysicalDeviceAccelerationStructureFeaturesKHR, vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceDescriptorIndexingFeatures> deviceCreateInfo = {
		createInfo,
		vk::PhysicalDeviceFeatures2().setFeatures(vk::PhysicalDeviceFeatures().setSamplerAnisotropy(true)),
		vk::PhysicalDeviceRayTracingPipelineFeaturesKHR().setRayTracingPipeline(true),
		vk::PhysicalDeviceAccelerationStructureFeaturesKHR().setAccelerationStructure(true),
		vk::PhysicalDeviceBufferDeviceAddressFeatures().setBufferDeviceAddress(true),
		vk::PhysicalDeviceDescriptorIndexingFeatures().setRuntimeDescriptorArray(true)};

	try
	{
		_device = _chosenGPU.createDevice(deviceCreateInfo.get<vk::DeviceCreateInfo>());
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_graphicsQueue = _device.getQueue(indices.graphicsFamily.value(), 0);
	_presentQueue = _device.getQueue(indices.presentFamily.value(), 0);

	vma::AllocatorCreateInfo allocatorInfo = vma::AllocatorCreateInfo({}, _chosenGPU, _device, {}, {}, {}, {}, {}, _instance, VK_API_VERSION_1_2);
	try
	{
		_allocator = vma::createAllocator(allocatorInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
}

void VulkanEngine::init_swapchain()
{
	vkhelper::SwapChainSupportDetails swapChainSupport = vkhelper::querySwapChainSupport(_chosenGPU, _surface);

	vk::SurfaceFormatKHR surfaceFormat = vkhelper::chooseSwapSurfaceFormat(swapChainSupport.formats);
	vk::PresentModeKHR presentMode = vkhelper::chooseSwapPresentMode(swapChainSupport.presentModes);
	vk::Extent2D extent = vkhelper::chooseSwapExtent(swapChainSupport.capabilities, _windowExtent);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	vkhelper::QueueFamilyIndices indices = vkhelper::findQueueFamilies(_chosenGPU, _surface);
	uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	vk::SwapchainCreateInfoKHR createInfo;
	createInfo.surface = _surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	}
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	try{
		_swapchain = _device.createSwapchainKHR(createInfo);
	}catch(std::exception& e) {
		std::cerr << "Exception Thrown: " << e.what();
	}
	_swapchainImages = _device.getSwapchainImagesKHR(_swapchain);
	_swachainImageFormat = surfaceFormat.format;
	_windowExtent = extent;

	_swapchainImageViews.resize(_swapchainImages.size());
	for (size_t i = 0; i < _swapchainImages.size(); i++) {
		_swapchainImageViews[i] = vkhelper::createImageView(_device, _swapchainImages[i], _swachainImageFormat, vk::ImageAspectFlagBits::eColor);
	}
}

void VulkanEngine::init_default_renderpass()
{
	vk::AttachmentDescription color_attachment;
	color_attachment.setFormat(_swachainImageFormat);
	color_attachment.setSamples(vk::SampleCountFlagBits::e1);
	color_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
	color_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
	color_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
	color_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
	color_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
	color_attachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

	vk::AttachmentReference color_attachment_ref;
	color_attachment_ref.setAttachment(0);
	color_attachment_ref.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

	vk::SubpassDescription subpass;
	subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	subpass.setColorAttachmentCount(1);
	subpass.setPColorAttachments(&color_attachment_ref);

	vk::SubpassDependency dependency = {};
	dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
	dependency.setDstSubpass(0);
	dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

	vk::RenderPassCreateInfo render_pass_info;
	render_pass_info.setAttachmentCount(1);
	render_pass_info.setPAttachments(&color_attachment);
	render_pass_info.setSubpassCount(1);
	render_pass_info.setPSubpasses(&subpass);
	render_pass_info.setDependencyCount(1);
	render_pass_info.setPDependencies(&dependency);

	try{
		_renderPass = _device.createRenderPass(render_pass_info);
	}catch(std::exception& e) {
		std::cerr << "Exception Thrown: " << e.what();
	}
}

void VulkanEngine::init_framebuffers()
{
	vk::FramebufferCreateInfo createInfo = vkinit::framebuffer_create_info(_renderPass, _windowExtent);

	const uint32_t swapchain_imagecount = (uint32_t) _swapchainImages.size();
	_framebuffers.resize(swapchain_imagecount);

	for (unsigned int i = 0; i < swapchain_imagecount; i++) {
		createInfo.setPAttachments(&_swapchainImageViews[i]);
		try{
			_framebuffers[i] = _device.createFramebuffer(createInfo);
		}catch(std::exception& e) {
			std::cerr << "Exception Thrown: " << e.what();
		}
	}
}

void VulkanEngine::init_commands()
{
	vk::CommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	try{
		_commandPool = _device.createCommandPool(commandPoolInfo);
	}catch(std::exception& e) {
		std::cerr << "Exception Thrown: " << e.what();
	}

	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_commandPool, 1, vk::CommandBufferLevel::ePrimary);
	try{
		_mainCommandBuffer = _device.allocateCommandBuffers(cmdAllocInfo).front();
	}catch(std::exception& e) {
		std::cerr << "Exception Thrown: " << e.what();
	}
}

void VulkanEngine::init_sync_structures()
{
	vk::FenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(vk::FenceCreateFlagBits::eSignaled);
	try{
		_renderFence = _device.createFence(fenceCreateInfo);
	}catch(std::exception& e) {
		std::cerr << "Exception Thrown: " << e.what();
	}

	vk::SemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
	try{
		_presentSemaphore = _device.createSemaphore(semaphoreCreateInfo);
		_renderSemaphore = _device.createSemaphore(semaphoreCreateInfo);
	}catch(std::exception& e) {
		std::cerr << "Exception Thrown: " << e.what();
	}
}