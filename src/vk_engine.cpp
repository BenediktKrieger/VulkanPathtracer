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

	load_models();

	_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{
		_device.waitIdle();
		_mainDeletionQueue.flush();
		_instance.destroySurfaceKHR(_surface);
		_allocator.destroy();
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
	_allocator.setCurrentFrameIndex(_frameNumber);
	_mainCommandBuffer.reset();

	uint32_t swapchainImageIndex;
	vk::Result aquireNextImageResult = _device.acquireNextImageKHR(_swapchain, 1000000000, _presentSemaphore, nullptr, &swapchainImageIndex);

	vk::CommandBuffer cmd = _mainCommandBuffer;

	vk::CommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	cmd.begin(cmdBeginInfo);

		vk::RenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);
		vk::ClearValue colorClear;
		colorClear.color = vk::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f);
		vk::ClearValue depthClear;
		depthClear.depthStencil = vk::ClearDepthStencilValue(1.f);
		std::array<vk::ClearValue, 2> clearValues = {colorClear, depthClear};
		rpInfo.setClearValues(clearValues);

		cmd.beginRenderPass(rpInfo, vk::SubpassContents::eInline);
		vk::DeviceSize offset = 0;
		cmd.bindVertexBuffers(0, 1, &_triangleModel._vertexBuffer._buffer, &offset);
		cmd.bindIndexBuffer(_triangleModel._indexBuffer._buffer, offset, vk::IndexType::eUint32);
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _modelPipeline);

		glm::vec3 camPos = { 30.f, 30.f, 30.f};
		glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
		glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
		projection[1][1] *= -1;

		for (auto node : _triangleModel._linearNodes)
		{
			for(auto primitive : node->primitives)
			{
				glm::mat4 model = glm::rotate(glm::mat4(1.f), glm::radians(_frameNumber * 0.4f), glm::vec3(0, 1, 0)) * node->getMatrix();
				glm::mat4 mvp_matrix = projection * view * model;
				vkutils::PushConstants constants;
				constants.matrix = mvp_matrix;
				cmd.pushConstants(_modelPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(vkutils::PushConstants), &constants);
				cmd.drawIndexed(primitive->indexCount, 1, primitive->firstIndex, 0, 0);
			}
		}

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

	vma::AllocatorCreateInfo allocatorInfo = vma::AllocatorCreateInfo(vma::AllocatorCreateFlagBits::eExtMemoryBudget, _chosenGPU, _device, {}, {}, {}, {}, {}, _instance, VK_API_VERSION_1_2);
	try
	{
		_allocator = vma::createAllocator(allocatorInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}

	std::vector<vma::Budget> heapBudgets = _allocator.getHeapBudgets();

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

	vk::Extent3D depthImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	_depthFormat = vk::Format::eD32Sfloat;
	vk::ImageCreateInfo dimg_info = vkinit::image_create_info(_depthFormat, vk::ImageUsageFlagBits::eDepthStencilAttachment, depthImageExtent);
	vma::AllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.setUsage(vma::MemoryUsage::eGpuOnly);
	dimg_allocinfo.setRequiredFlags(vk::MemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal));
	std::tie(_depthImage._image, _depthImage._allocation) = _allocator.createImage(dimg_info, dimg_allocinfo);

	vk::ImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthFormat, _depthImage._image, vk::ImageAspectFlagBits::eDepth);

	_depthImageView = _device.createImageView(dview_info);

	_mainDeletionQueue.push_function([=]() {
		_device.destroyImageView(_depthImageView);
		_allocator.destroyImage(_depthImage._image, _depthImage._allocation);
	});
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

	vk::AttachmentDescription depth_attachment;
	depth_attachment.setFormat(_depthFormat);
	depth_attachment.setSamples(vk::SampleCountFlagBits::e1);
	depth_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
	depth_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
	depth_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eClear);
	depth_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
	depth_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
	depth_attachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	vk::AttachmentReference depth_attachment_ref;
	depth_attachment_ref.setAttachment(1);
	depth_attachment_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	vk::SubpassDescription subpass;
	subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
	subpass.setColorAttachmentCount(1);
	subpass.setPColorAttachments(&color_attachment_ref);
	subpass.setPDepthStencilAttachment(&depth_attachment_ref);

	vk::SubpassDependency color_dependency;
	color_dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
	color_dependency.setDstSubpass(0);
	color_dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	color_dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	color_dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

	vk::SubpassDependency depth_dependency;
	depth_dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
	depth_dependency.setDstSubpass(0);
	depth_dependency.setSrcStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests);
	depth_dependency.setDstStageMask(vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests);
	depth_dependency.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);

	std::array<vk::AttachmentDescription, 2> attachments = {color_attachment, depth_attachment};
	std::array<vk::SubpassDependency, 2> dependencies = {color_dependency, depth_dependency};

	vk::RenderPassCreateInfo render_pass_info;
	render_pass_info.setAttachments(attachments);
	render_pass_info.setDependencies(dependencies);
	render_pass_info.setSubpassCount(1);
	render_pass_info.setPSubpasses(&subpass);

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
		std::array<vk::ImageView, 2> attachments = {_swapchainImageViews[i], _depthImageView};
		createInfo.setAttachments(attachments);
		try
		{
			_framebuffers[i] = _device.createFramebuffer(createInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_mainDeletionQueue.push_function([=](){
			_device.destroyFramebuffer(_framebuffers[i], nullptr);
			_device.destroyImageView(_swapchainImageViews[i], nullptr); 
		});
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
	vk::ShaderModule modelVertexShader = load_shader_module(vk::ShaderStageFlagBits::eVertex, "/triangle.vert");
	vk::ShaderModule modelFragShader = load_shader_module(vk::ShaderStageFlagBits::eFragment, "/triangle.frag");

	vk::PipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	std::array<vk::PushConstantRange, 1> push_constants;
	push_constants[0].setOffset(0);
	push_constants[0].setSize(sizeof(vkutils::PushConstants));
	push_constants[0].setStageFlags(vk::ShaderStageFlagBits::eVertex);
	pipeline_layout_info.setPushConstantRanges(push_constants);

	_modelPipelineLayout = _device.createPipelineLayout(pipeline_layout_info, nullptr);

	VertexInputDescription vertexDescription = Vertex::get_vertex_description();

	vkutils::PipelineBuilder pipelineBuilder;
	pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eVertex, modelVertexShader));
	pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eFragment, modelFragShader));
	pipelineBuilder._vertexInputInfo.setVertexAttributeDescriptions(vertexDescription.attributes);
	pipelineBuilder._vertexInputInfo.setVertexBindingDescriptions(vertexDescription.bindings);
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
	pipelineBuilder._pipelineLayout = _modelPipelineLayout;
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, vk::CompareOp::eLessOrEqual);
	_modelPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

	_device.destroyShaderModule(modelFragShader);
	_device.destroyShaderModule(modelVertexShader);

	_mainDeletionQueue.push_function([=]() {
		_device.destroyPipeline(_modelPipeline, nullptr);
		_device.destroyPipelineLayout(_modelPipelineLayout, nullptr);
	});
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

void VulkanEngine::load_models()
{
	_triangleModel.load_from_glb(ASSET_PATH"/bistro_exterior.glb");

	upload_model(_triangleModel);
}

void VulkanEngine::upload_model(Model &model)
{
	vk::DeviceSize vertexBufferSize = model._vertices.size() * sizeof(Vertex);
	vk::DeviceSize indexBufferSize = model._indices.size() * sizeof(uint32_t);

	std::cout << "Vertex Count: " << model._vertices.size() << std::endl;

	//allocate vertex staging buffer
	vk::BufferCreateInfo vertexStagingBufferInfo;
	vertexStagingBufferInfo.size = vertexBufferSize;
	vertexStagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
	
	vma::AllocationCreateInfo vertexStagingBufferAllocInfo = {};
	vertexStagingBufferAllocInfo.usage = vma::MemoryUsage::eAuto;
	vertexStagingBufferAllocInfo.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite;

	vk::Result createVertexStagingBufferResult = _allocator.createBuffer(&vertexStagingBufferInfo, &vertexStagingBufferAllocInfo, &model._vertexStagingBuffer._buffer, &model._vertexStagingBuffer._allocation, nullptr);

	void* dataVertex;
	vk::Result mapVertexBufferResult = _allocator.mapMemory(model._vertexStagingBuffer._allocation, &dataVertex);
	memcpy(dataVertex, model._vertices.data(), model._vertices.size() * sizeof(Vertex));
	_allocator.unmapMemory(model._vertexStagingBuffer._allocation);

	//allocate vertex buffer

	vk::BufferCreateInfo vertexBufferInfo;
	vertexBufferInfo.size = vertexBufferSize;
	vertexBufferInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
	
	vma::AllocationCreateInfo vertexBufferAllocInfo = {};
	vertexBufferAllocInfo.usage = vma::MemoryUsage::eAuto;
	vertexBufferAllocInfo.flags = vma::AllocationCreateFlagBits::eDedicatedMemory;
	vertexBufferAllocInfo.priority = 1.f;

	vk::Result createVertexBufferResult = _allocator.createBuffer(&vertexBufferInfo, &vertexBufferAllocInfo, &model._vertexBuffer._buffer, &model._vertexBuffer._allocation, nullptr);

	vkutils::copyBuffer(_device, _commandPool, _graphicsQueue, model._vertexStagingBuffer._buffer, model._vertexBuffer._buffer, vertexBufferSize);

	_mainDeletionQueue.push_function([=]() {
		_allocator.destroyBuffer(model._vertexStagingBuffer._buffer, model._vertexStagingBuffer._allocation);
        _allocator.destroyBuffer(model._vertexBuffer._buffer, model._vertexBuffer._allocation);
    });

	//allocate index staging Buffer

	vk::BufferCreateInfo indexStagingBufferInfo;
	indexStagingBufferInfo.size = indexBufferSize;
	indexStagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

	vma::AllocationCreateInfo indexStagingBufferallocInfo = {};
	indexStagingBufferallocInfo.usage = vma::MemoryUsage::eAuto;
	indexStagingBufferallocInfo.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite;

	vk::Result createIndexStagingBufferResult = _allocator.createBuffer(&indexStagingBufferInfo, &indexStagingBufferallocInfo, &model._indexStagingBuffer._buffer, &model._indexStagingBuffer._allocation, nullptr);

	void* dataIndex;
	vk::Result mapIndexBufferResult = _allocator.mapMemory(model._indexStagingBuffer._allocation, &dataIndex);
	memcpy(dataIndex, model._indices.data(), model._indices.size() * sizeof(uint32_t));
	_allocator.unmapMemory(model._indexStagingBuffer._allocation);
	
	//allocate index Buffer

	vk::BufferCreateInfo indexBufferInfo;
	indexBufferInfo.size = indexBufferSize;
	indexBufferInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;

	vma::AllocationCreateInfo indexBufferallocInfo = {};
	indexBufferallocInfo.usage = vma::MemoryUsage::eAuto;
	indexBufferallocInfo.flags = vma::AllocationCreateFlagBits::eDedicatedMemory;
	indexBufferallocInfo.priority = 1.f;

	vk::Result createIndexBufferResult = _allocator.createBuffer(&indexBufferInfo, &indexBufferallocInfo, &model._indexBuffer._buffer, &model._indexBuffer._allocation, nullptr);

	vkutils::copyBuffer(_device, _commandPool, _graphicsQueue, model._indexStagingBuffer._buffer, model._indexBuffer._buffer, indexBufferSize);
	
	_mainDeletionQueue.push_function([=]() {
    	_allocator.destroyBuffer(model._indexStagingBuffer._buffer, model._indexStagingBuffer._allocation);
		_allocator.destroyBuffer(model._indexBuffer._buffer, model._indexBuffer._allocation);
    });
}