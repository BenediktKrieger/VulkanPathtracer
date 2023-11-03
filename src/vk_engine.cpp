#include <SDL.h>
#include <SDL_vulkan.h>
#include <vk_engine.h>
#include <vk_initializers.h>
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

	init_pipelines();

	// everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{
		_device.waitIdle();
		_mainDeletionQueue.flush();
		_instance.destroySurfaceKHR(_surface);
		_device.destroy();
		_instance.destroyDebugUtilsMessengerEXT(_debug_messenger);
		_instance.destroy();
		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{
	if (SDL_GetWindowFlags(_window) & SDL_WINDOW_MINIMIZED)
		return;

	vk::Result waitFencesResult = _device.waitForFences(1, &_renderFence, true, 1000000000);
	vk::Result resetFencesResult = _device.resetFences(1, &_renderFence);
	_mainCommandBuffer.reset();

	uint32_t swapchainImageIndex;
	vk::Result aquireNextImageResult = _device.acquireNextImageKHR(_swapchain, 1000000000, _presentSemaphore, nullptr, &swapchainImageIndex);

	vk::CommandBuffer cmd = _mainCommandBuffer;

	vk::CommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	cmd.begin(cmdBeginInfo);

	vk::ClearValue clearValue;
	float flash = abs(sin(_frameNumber / 144.f));
	clearValue.setColor(vk::ClearColorValue(0.0f, 0.0f, flash, 1.0f));

	vk::RenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

	rpInfo.setClearValueCount(1);
	rpInfo.setPClearValues(&clearValue);

	cmd.beginRenderPass(rpInfo, vk::SubpassContents::eInline);

	if (_selectedShader == 0)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _trianglePipeline);
	}
	else
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _redTrianglePipeline);
	}
	cmd.draw(3, 1, 0, 0);

	cmd.endRenderPass();

	cmd.end();

	vk::SubmitInfo submit = vkinit::submit_info(&cmd);
	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	submit.setPWaitDstStageMask(&waitStage);
	submit.setWaitSemaphoreCount(1);
	submit.setPWaitSemaphores(&_presentSemaphore);
	submit.setSignalSemaphoreCount(1);
	submit.setPSignalSemaphores(&_renderSemaphore);

	vk::Result queueSubmitResult = _graphicsQueue.submit(1, &submit, _renderFence);

	vk::PresentInfoKHR presentInfo = vkinit::present_info();
	presentInfo.setPSwapchains(&_swapchain);
	presentInfo.setSwapchainCount(1);
	presentInfo.setPWaitSemaphores(&_renderSemaphore);
	presentInfo.setWaitSemaphoreCount(1);
	presentInfo.setPImageIndices(&swapchainImageIndex);

	vk::Result queuePresentResult = _presentQueue.presentKHR(presentInfo);

	_frameNumber++;
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
			if (e.type == SDL_EVENT_QUIT)
			{
				bQuit = true;
			}
			else if (e.type == SDL_EVENT_KEY_DOWN)
			{
				if (e.key.keysym.sym == SDLK_SPACE)
				{
					_selectedShader += 1;
					if (_selectedShader > 1)
					{
						_selectedShader = 0;
					}
				}
			}
		}

		draw();
	}
}

void VulkanEngine::init_vulkan()
{
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = _dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
	if (bUseValidationLayers && !vkutils::checkValidationLayerSupport(_instanceLayers))
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}
	unsigned int sdl_extensions_count = 0;
	SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count, NULL);
	_instanceExtensions.resize(sdl_extensions_count);
	SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count, _instanceExtensions.data());

	vk::ApplicationInfo applicationInfo("Vulkan Pathtracer", VK_MAKE_VERSION(0, 0, 1), "VulkanEngine", 1, VK_API_VERSION_1_2);

	try
	{
		if (bUseValidationLayers)
		{
			_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instanceCreateInfoChain{
				vk::InstanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo, _instanceLayers, _instanceExtensions),
				vk::DebugUtilsMessengerCreateInfoEXT({}, _messageSeverityFlags, _messageTypeFlags, vkutils::debugCallback)};
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
		vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo({}, _messageSeverityFlags, _messageTypeFlags, vkutils::debugCallback);
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
		if (vkutils::isDeviceSuitable(device, _surface, _deviceExtensions))
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

	vkutils::QueueFamilyIndices indices = vkutils::findQueueFamilies(_chosenGPU, _surface);
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
	std::vector<vk::ExtensionProperties> extensionProperties = _chosenGPU.enumerateDeviceExtensionProperties();
	for (auto extensionProperty : extensionProperties)
	{
		if (std::string(extensionProperty.extensionName.data()) == std::string(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
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
	vkutils::SwapChainSupportDetails swapChainSupport = vkutils::querySwapChainSupport(_chosenGPU, _surface);

	vk::SurfaceFormatKHR surfaceFormat = vkutils::chooseSwapSurfaceFormat(swapChainSupport.formats);
	vk::PresentModeKHR presentMode = vkutils::chooseSwapPresentMode(swapChainSupport.presentModes);
	vk::Extent2D extent = vkutils::chooseSwapExtent(swapChainSupport.capabilities, _windowExtent);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	vkutils::QueueFamilyIndices indices = vkutils::findQueueFamilies(_chosenGPU, _surface);
	uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	vk::SwapchainCreateInfoKHR createInfo;
	createInfo.surface = _surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
	}
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	try
	{
		_swapchain = _device.createSwapchainKHR(createInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_mainDeletionQueue.push_function([=]()
									 { _device.destroySwapchainKHR(_swapchain, nullptr); });

	_swapchainImages = _device.getSwapchainImagesKHR(_swapchain);
	_swachainImageFormat = surfaceFormat.format;
	_windowExtent = extent;

	_swapchainImageViews.resize(_swapchainImages.size());
	for (size_t i = 0; i < _swapchainImages.size(); i++)
	{
		_swapchainImageViews[i] = vkutils::createImageView(_device, _swapchainImages[i], _swachainImageFormat, vk::ImageAspectFlagBits::eColor);
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

	try
	{
		_renderPass = _device.createRenderPass(render_pass_info);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_mainDeletionQueue.push_function([=]()
									 { _device.destroyRenderPass(_renderPass, nullptr); });
}

void VulkanEngine::init_framebuffers()
{
	vk::FramebufferCreateInfo createInfo = vkinit::framebuffer_create_info(_renderPass, _windowExtent);

	const uint32_t swapchain_imagecount = (uint32_t)_swapchainImages.size();
	_framebuffers.resize(swapchain_imagecount);

	for (unsigned int i = 0; i < swapchain_imagecount; i++)
	{
		createInfo.setPAttachments(&_swapchainImageViews[i]);
		try
		{
			_framebuffers[i] = _device.createFramebuffer(createInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_mainDeletionQueue.push_function([=]()
										 {
			_device.destroyFramebuffer(_framebuffers[i], nullptr);
			_device.destroyImageView(_swapchainImageViews[i], nullptr); });
	}
}

void VulkanEngine::init_commands()
{
	vk::CommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	try
	{
		_commandPool = _device.createCommandPool(commandPoolInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}

	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_commandPool, 1, vk::CommandBufferLevel::ePrimary);
	try
	{
		_mainCommandBuffer = _device.allocateCommandBuffers(cmdAllocInfo).front();
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}

	_mainDeletionQueue.push_function([=]()
									 { _device.destroyCommandPool(_commandPool, nullptr); });
}

void VulkanEngine::init_sync_structures()
{
	vk::FenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(vk::FenceCreateFlagBits::eSignaled);
	try
	{
		_renderFence = _device.createFence(fenceCreateInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_mainDeletionQueue.push_function([=]()
									 { _device.destroyFence(_renderFence, nullptr); });

	vk::SemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
	try
	{
		_presentSemaphore = _device.createSemaphore(semaphoreCreateInfo);
		_renderSemaphore = _device.createSemaphore(semaphoreCreateInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_mainDeletionQueue.push_function([=]()
									 {
		_device.destroySemaphore(_presentSemaphore, nullptr);
		_device.destroySemaphore(_renderSemaphore, nullptr); });
}

void VulkanEngine::init_pipelines()
{
	vk::ShaderModule triangleVertexShader = load_shader_module(vk::ShaderStageFlagBits::eVertex, "/colored_triangle.vert");
	vk::ShaderModule triangleFragShader = load_shader_module(vk::ShaderStageFlagBits::eFragment, "/colored_triangle.frag");
	vk::ShaderModule redTriangleVertShader = load_shader_module(vk::ShaderStageFlagBits::eVertex, "/triangle.vert");
	vk::ShaderModule redTriangleFragShader = load_shader_module(vk::ShaderStageFlagBits::eFragment, "/triangle.frag");

	vk::PipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();

	_trianglePipelineLayout = _device.createPipelineLayout(pipeline_layout_info, nullptr);

	vkutils::PipelineBuilder pipelineBuilder;
	pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eVertex, triangleVertexShader));
	pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eFragment, triangleFragShader));
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(vk::PrimitiveTopology::eTriangleList);
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;
	pipelineBuilder._scissor.offset = vk::Offset2D(0, 0);
	pipelineBuilder._scissor.extent = _windowExtent;
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(vk::PolygonMode::eFill);
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();
	pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
	_trianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

	pipelineBuilder._shaderStages.clear();
	pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eVertex, redTriangleVertShader));
	pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eFragment, redTriangleFragShader));
	_redTrianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

	_device.destroyShaderModule(redTriangleVertShader);
	_device.destroyShaderModule(redTriangleFragShader);
	_device.destroyShaderModule(triangleFragShader);
	_device.destroyShaderModule(triangleVertexShader);

	_mainDeletionQueue.push_function([=]()
									 {
		_device.destroyPipeline(_redTrianglePipeline, nullptr);
		_device.destroyPipeline(_trianglePipeline, nullptr);
		_device.destroyPipelineLayout(_trianglePipelineLayout, nullptr); });
}

vk::ShaderModule VulkanEngine::load_shader_module(vk::ShaderStageFlagBits type, std::string filePath)
{
	glslang::InitializeProcess();
	std::vector<uint32_t> shaderCodeSPIRV;
	vkshader::GLSLtoSPV(type, filePath, shaderCodeSPIRV);
	vk::ShaderModuleCreateInfo createInfo({}, shaderCodeSPIRV);
	vk::ShaderModule shaderModule;
	try
	{
		shaderModule = _device.createShaderModule(createInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	glslang::FinalizeProcess();

	return shaderModule;
}