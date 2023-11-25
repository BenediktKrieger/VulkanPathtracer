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

	_window = SDL_CreateWindow("Vulkan Engine", _windowExtent.width, _windowExtent.height, window_flags);

	init_vulkan();

	init_swapchain();

	init_default_renderpass();

	init_framebuffers();

	init_commands();

	init_sync_structures();

	load_models();

	init_pipelines();
	
	createShaderBindingTable();
	
	init_descriptors();

	glm::vec3 camPos(40.f);
	glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float) _windowExtent.width / _windowExtent.height, 0.1f, 200.0f);
	projection[1][1] *= -1;

	vkutils::CameraData camData;
	camData.invProj = glm::inverse(projection);
	camData.invView = glm::inverse(view);
	camData.viewproj = projection * view;
	camData.camPos = glm::vec4(camPos, 1.0);
	camData.camDir = glm::vec4(-camPos, 1.0);

	for (int i = 0; i < FRAME_OVERLAP; i++){
		void* data = _allocator.mapMemory(_frames[i]._cameraBuffer._allocation);
			memcpy(data, &camData, sizeof(vkutils::CameraData));
		_allocator.unmapMemory(_frames[i]._cameraBuffer._allocation);
	}

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

	vk::Result waitFencesResult = _device.waitForFences(get_current_frame()._renderFence, true, 1000000000);
	_device.resetFences(get_current_frame()._renderFence);
	_allocator.setCurrentFrameIndex(_frameNumber);
	get_current_frame()._mainCommandBuffer.reset();

	uint32_t swapchainImageIndex;
	vk::Result aquireNextImageResult = _device.acquireNextImageKHR(_swapchain, 1000000000, get_current_frame()._presentSemaphore, nullptr, &swapchainImageIndex);

	vk::CommandBuffer cmd = get_current_frame()._mainCommandBuffer;

	vk::CommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	vk::ImageSubresourceRange subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

	cmd.begin(cmdBeginInfo);

		// vk::RenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);
		// vk::ClearValue colorClear;
		// colorClear.color = vk::ClearColorValue(1.0f, 1.0f, 1.0f, 1.0f);
		// vk::ClearValue depthClear;
		// depthClear.depthStencil = vk::ClearDepthStencilValue(1.f);
		// std::array<vk::ClearValue, 2> clearValues = {colorClear, depthClear};
		// rpInfo.setClearValues(clearValues);

		// cmd.beginRenderPass(rpInfo, vk::SubpassContents::eInline);
		// vk::DeviceSize offset = 0;
		// cmd.bindVertexBuffers(0, 1, &_triangleModel._vertexBuffer._buffer, &offset);
		// cmd.bindIndexBuffer(_triangleModel._indexBuffer._buffer, offset, vk::IndexType::eUint32);
		// cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _rasterizerPipeline);
		// cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _rasterizerPipelineLayout, 0, get_current_frame()._rasterizerDescriptor, {});

		// for (auto node : _triangleModel._linearNodes)
		// {
		// 	for(auto primitive : node->primitives)
		// 	{
		// 		glm::mat4 model = glm::rotate(glm::mat4(1.f), glm::radians(_frameNumber * 0.4f), glm::vec3(0, 1, 0)) * node->getMatrix();
		// 		vkutils::PushConstants constants;
		// 		constants.matrix = model;
		// 		cmd.pushConstants(_rasterizerPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(vkutils::PushConstants), &constants);
		// 		cmd.drawIndexed(primitive->indexCount, 1, primitive->firstIndex, 0, 0);
		// 	}
		// }

		// cmd.endRenderPass();

		const uint32_t handleSizeAligned = vkutils::alignedSize(_raytracingPipelineProperties.shaderGroupHandleSize, _raytracingPipelineProperties.shaderGroupHandleAlignment);

		vk::DeviceOrHostAddressConstKHR raygenAddress;
		vk::BufferDeviceAddressInfo raygenAddressInfo(_raygenShaderBindingTable._buffer);
		vk::StridedDeviceAddressRegionKHR raygenShaderSbtEntry;
		raygenShaderSbtEntry.deviceAddress = _device.getBufferAddress(raygenAddressInfo);;
		raygenShaderSbtEntry.stride = handleSizeAligned;
		raygenShaderSbtEntry.size = handleSizeAligned;

		vk::DeviceOrHostAddressConstKHR missAddress;
		vk::BufferDeviceAddressInfo missAddressInfo(_missShaderBindingTable._buffer);
		vk::StridedDeviceAddressRegionKHR missShaderSbtEntry;
		missShaderSbtEntry.deviceAddress = _device.getBufferAddress(missAddressInfo);
		missShaderSbtEntry.stride = handleSizeAligned;
		missShaderSbtEntry.size = handleSizeAligned;

		vk::DeviceOrHostAddressConstKHR hitAddress;
		vk::BufferDeviceAddressInfo hitAddressInfo(_hitShaderBindingTable._buffer);
		vk::StridedDeviceAddressRegionKHR hitShaderSbtEntry;
		hitShaderSbtEntry.deviceAddress = _device.getBufferAddress(hitAddressInfo);
		hitShaderSbtEntry.stride = handleSizeAligned;
		hitShaderSbtEntry.size = handleSizeAligned;

		vk::StridedDeviceAddressRegionKHR callableShaderSbtEntry;

		/*
			Dispatch the ray tracing commands
		*/
		cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _raytracerPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _raytracerPipelineLayout, 0, 1, &get_current_frame()._raytracerDescriptor, 0, 0);

		cmd.traceRaysKHR(&raygenShaderSbtEntry, &missShaderSbtEntry, &hitShaderSbtEntry, &callableShaderSbtEntry, _windowExtent.width, _windowExtent.height, 1);

		/*
			Copy ray tracing output to swap chain image
		*/

		// Prepare current swap chain image as transfer destination
		vkutils::setImageLayout(cmd, _swapchainImages[swapchainImageIndex], vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, subresourceRange);

		// Prepare ray tracing output image as transfer source
		vkutils::setImageLayout(cmd, get_current_frame()._storageImage._image, vk::ImageLayout::eGeneral,  vk::ImageLayout::eTransferSrcOptimal, subresourceRange);

		vk::ImageCopy copyRegion;
		copyRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
		copyRegion.srcOffset = vk::Offset3D(0, 0, 0 );
		copyRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
		copyRegion.dstOffset = vk::Offset3D(0, 0, 0 );
		copyRegion.extent = vk::Extent3D(_windowExtent.width, _windowExtent.height, 1);
		
		cmd.copyImage(get_current_frame()._storageImage._image, vk::ImageLayout::eTransferSrcOptimal, _swapchainImages[swapchainImageIndex], vk::ImageLayout::eTransferDstOptimal, copyRegion);

		// Transition swap chain image back for presentation
		vkutils::setImageLayout(cmd, _swapchainImages[swapchainImageIndex], vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR, subresourceRange);

		// Transition ray tracing output image back to general layout
		vkutils::setImageLayout(cmd, get_current_frame()._storageImage._image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral, subresourceRange);

	cmd.end();

	vk::SubmitInfo submit = vkinit::submit_info(&cmd);
	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	submit.setWaitDstStageMask(waitStage);
	submit.setWaitSemaphores(get_current_frame()._presentSemaphore);
	submit.setSignalSemaphores(get_current_frame()._renderSemaphore);

	_graphicsQueue.submit(submit, get_current_frame()._renderFence);

	vk::PresentInfoKHR presentInfo = vkinit::present_info();
	presentInfo.setSwapchains(_swapchain);
	presentInfo.setWaitSemaphores(get_current_frame()._renderSemaphore);
	presentInfo.setImageIndices(swapchainImageIndex);

	vk::Result queuePresentResult = _presentQueue.presentKHR(presentInfo);

	_frameNumber++;
	_presentQueue.waitIdle();
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
		}

		draw();
	}
}

vkutils::FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
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
				vk::DebugUtilsMessengerCreateInfoEXT({}, _messageSeverityFlags, _messageTypeFlags, vkutils::debugCallback)
			};
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
		vk::PhysicalDeviceDescriptorIndexingFeatures().setRuntimeDescriptorArray(true)
	};
	auto _physicalDeviceProperties = _chosenGPU.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR, vk::PhysicalDeviceAccelerationStructurePropertiesKHR, vk::PhysicalDeviceDescriptorIndexingProperties>();
	_raytracingPipelineProperties = _physicalDeviceProperties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
	try
	{
		_device = _chosenGPU.createDevice(deviceCreateInfo.get<vk::DeviceCreateInfo>());
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	VULKAN_HPP_DEFAULT_DISPATCHER.init(_device);
	_graphicsQueue = _device.getQueue(indices.graphicsFamily.value(), 0);
	_presentQueue = _device.getQueue(indices.presentFamily.value(), 0);

	vma::AllocatorCreateInfo allocatorInfo = vma::AllocatorCreateInfo(vma::AllocatorCreateFlagBits::eExtMemoryBudget | vma::AllocatorCreateFlagBits::eBufferDeviceAddress, _chosenGPU, _device, {}, {}, {}, {}, {}, _instance, VK_API_VERSION_1_2);
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
	vk::PresentModeKHR presentMode = vkutils::chooseSwapPresentMode(vk::PresentModeKHR::eFifoRelaxed, swapChainSupport.presentModes);
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
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
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
	_mainDeletionQueue.push_function([=](){
		_device.destroySwapchainKHR(_swapchain, nullptr);
	});

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
	render_pass_info.setSubpasses(subpass);

	try
	{
		_renderPass = _device.createRenderPass(render_pass_info);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_mainDeletionQueue.push_function([=](){ 
		_device.destroyRenderPass(_renderPass, nullptr); 
	});
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
	for (int i = 0; i < FRAME_OVERLAP; i++) {
		try
		{
			_frames[i]._commandPool = _device.createCommandPool(commandPoolInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}

		vk::CommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1, vk::CommandBufferLevel::ePrimary);
		try
		{
			_frames[i]._mainCommandBuffer = _device.allocateCommandBuffers(cmdAllocInfo).front();
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_mainDeletionQueue.push_function([=](){
			_device.destroyCommandPool(_frames[i]._commandPool, nullptr);
		});
	}
}

void VulkanEngine::init_sync_structures()
{
	vk::FenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(vk::FenceCreateFlagBits::eSignaled);
	vk::SemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {     
		try
		{
			_frames[i]._renderFence = _device.createFence(fenceCreateInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_mainDeletionQueue.push_function([=](){
			_device.destroyFence(_frames[i]._renderFence, nullptr); 
		});
		try
		{
			_frames[i]._presentSemaphore = _device.createSemaphore(semaphoreCreateInfo);
			_frames[i]._renderSemaphore = _device.createSemaphore(semaphoreCreateInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_mainDeletionQueue.push_function([=]()
		{
			_device.destroySemaphore(_frames[i]._presentSemaphore, nullptr);
			_device.destroySemaphore(_frames[i]._renderSemaphore, nullptr); 
		});
	}
}

void VulkanEngine::init_pipelines()
{
	// init rasterization pipeline
	// {
	// 	vk::DescriptorSetLayoutBinding camBufferBinding;
	// 	camBufferBinding.binding = 0;
	// 	camBufferBinding.descriptorCount = 1;
	// 	camBufferBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
	// 	camBufferBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

	// 	vk::DescriptorSetLayoutCreateInfo setinfo;
	// 	setinfo.setBindings(camBufferBinding);

	// 	_rasterizerSetLayout = _device.createDescriptorSetLayout(setinfo);

	// 	vk::ShaderModule vertexShader = load_shader_module(vk::ShaderStageFlagBits::eVertex, "/triangle.vert");
	// 	vk::ShaderModule fragShader = load_shader_module(vk::ShaderStageFlagBits::eFragment, "/triangle.frag");

	// 	vk::PipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	// 	pipeline_layout_info.setSetLayouts(_rasterizerSetLayout);
	// 	vk::PushConstantRange push_constants{vk::ShaderStageFlagBits::eVertex, 0, sizeof(vkutils::PushConstants)};
	// 	pipeline_layout_info.setPushConstantRanges(push_constants);

	// 	_rasterizerPipelineLayout = _device.createPipelineLayout(pipeline_layout_info);

	// 	VertexInputDescription vertexDescription = Vertex::get_vertex_description();

	// 	vkutils::PipelineBuilder pipelineBuilder;
	// 	pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eVertex, vertexShader));
	// 	pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eFragment, fragShader));
	// 	pipelineBuilder._vertexInputInfo.setVertexAttributeDescriptions(vertexDescription.attributes);
	// 	pipelineBuilder._vertexInputInfo.setVertexBindingDescriptions(vertexDescription.bindings);
	// 	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(vk::PrimitiveTopology::eTriangleList);
	// 	pipelineBuilder._viewport.x = 0.0f;
	// 	pipelineBuilder._viewport.y = 0.0f;
	// 	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	// 	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	// 	pipelineBuilder._viewport.minDepth = 0.0f;
	// 	pipelineBuilder._viewport.maxDepth = 1.0f;
	// 	pipelineBuilder._scissor.offset = vk::Offset2D(0, 0);
	// 	pipelineBuilder._scissor.extent = _windowExtent;
	// 	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(vk::PolygonMode::eFill);
	// 	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();
	// 	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();
	// 	pipelineBuilder._pipelineLayout = _rasterizerPipelineLayout;
	// 	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, vk::CompareOp::eLessOrEqual);
	// 	_rasterizerPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

	// 	_device.destroyShaderModule(fragShader);
	// 	_device.destroyShaderModule(vertexShader);

	// 	_mainDeletionQueue.push_function([=]() {
	// 		_device.destroyPipeline(_rasterizerPipeline);
	// 		_device.destroyPipelineLayout(_rasterizerPipelineLayout);
	// 		_device.destroyDescriptorSetLayout(_rasterizerSetLayout);
	// 	});
	// }

	// init raytracing pipeline
	{
		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding.binding = 0;
		accelerationStructureLayoutBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		accelerationStructureLayoutBinding.descriptorCount = 1;
		accelerationStructureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		vk::DescriptorSetLayoutBinding resultImageLayoutBinding;
		resultImageLayoutBinding.binding = 1;
		resultImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		resultImageLayoutBinding.descriptorCount = 1;
		resultImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		vk::DescriptorSetLayoutBinding uniformBufferBinding;
		uniformBufferBinding.binding = 2;
		uniformBufferBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		uniformBufferBinding.descriptorCount = 1;
		uniformBufferBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		std::vector<vk::DescriptorSetLayoutBinding> bindings({
			accelerationStructureLayoutBinding,
			resultImageLayoutBinding,
			uniformBufferBinding
		});

		vk::DescriptorSetLayoutCreateInfo setinfo;
		setinfo.setBindings(bindings);

		_raytracerSetLayout = _device.createDescriptorSetLayout(setinfo);

		vk::PipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
		pipeline_layout_info.setSetLayouts(_raytracerSetLayout);

		_raytracerPipelineLayout = _device.createPipelineLayout(pipeline_layout_info);

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
		vk::ShaderModule raygenShader, missShader, hitShader;

		// Ray generation group
		{
			raygenShader = load_shader_module(vk::ShaderStageFlagBits::eRaygenKHR, "/simple.rgen");
			shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eRaygenKHR, raygenShader));
			vk::RayTracingShaderGroupCreateInfoKHR shaderGroup;
			shaderGroup.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = vk::ShaderUnusedKhr;
			shaderGroup.anyHitShader = vk::ShaderUnusedKhr;
			shaderGroup.intersectionShader = vk::ShaderUnusedKhr;
			_shaderGroups.push_back(shaderGroup);
		}

		// Miss group
		{
			missShader = load_shader_module(vk::ShaderStageFlagBits::eMissKHR, "/simple.rmiss");
			shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eMissKHR, missShader));
			vk::RayTracingShaderGroupCreateInfoKHR shaderGroup;
			shaderGroup.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = vk::ShaderUnusedKhr;
			shaderGroup.anyHitShader = vk::ShaderUnusedKhr;
			shaderGroup.intersectionShader = vk::ShaderUnusedKhr;
			_shaderGroups.push_back(shaderGroup);
		}

		// Closest hit group
		{
			hitShader = load_shader_module(vk::ShaderStageFlagBits::eClosestHitKHR, "/simple.rchit");
			shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eClosestHitKHR, hitShader));
			vk::RayTracingShaderGroupCreateInfoKHR shaderGroup;
			shaderGroup.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
			shaderGroup.generalShader = vk::ShaderUnusedKhr;
			shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.anyHitShader = vk::ShaderUnusedKhr;
			shaderGroup.intersectionShader = vk::ShaderUnusedKhr;
			_shaderGroups.push_back(shaderGroup);
		}

		/*
			Create the ray tracing pipeline
		*/
		vk::RayTracingPipelineCreateInfoKHR rayTracingPipelineInfo;
		rayTracingPipelineInfo.setStages(shaderStages);
		rayTracingPipelineInfo.setGroups(_shaderGroups);
		rayTracingPipelineInfo.maxPipelineRayRecursionDepth = 1;
		rayTracingPipelineInfo.layout = _raytracerPipelineLayout;
		
		try
		{
			vk::Result result;
			std::tie(result, _raytracerPipeline) = _device.createRayTracingPipelineKHR({}, {}, rayTracingPipelineInfo);
			if (result != vk::Result::eSuccess)
			{
				throw std::runtime_error("failed to create graphics Pipeline!");
			}
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}

		_device.destroyShaderModule(raygenShader);
		_device.destroyShaderModule(missShader);
		_device.destroyShaderModule(hitShader);

		_mainDeletionQueue.push_function([=]() {
			_device.destroyPipeline(_raytracerPipeline);
			_device.destroyPipelineLayout(_raytracerPipelineLayout);
			_device.destroyDescriptorSetLayout(_raytracerSetLayout);
		});
	}
}

void VulkanEngine::init_descriptors()
{
	//init rasterizing descriptors
	// {
	// 	std::vector<vk::DescriptorPoolSize> poolSizes =
	// 	{
	// 		{vk::DescriptorType::eUniformBuffer, 1 }
	// 	};

	// 	vk::DescriptorPoolCreateInfo pool_info;
	// 	pool_info.setMaxSets(2);
	// 	pool_info.setPoolSizes(poolSizes);

	// 	_rasterizerDescriptorPool = _device.createDescriptorPool(pool_info);
		
	// 	for (int i = 0; i < FRAME_OVERLAP; i++)
	// 	{
	// 		// _frames[i]._cameraBuffer = vkutils::createBuffer(_allocator, sizeof(vkutils::CameraData), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

	// 		vk::DescriptorSetAllocateInfo allocInfo;
	// 		allocInfo.descriptorPool = _rasterizerDescriptorPool;
	// 		allocInfo.setSetLayouts(_rasterizerSetLayout);

	// 		_frames[i]._rasterizerDescriptor = _device.allocateDescriptorSets(allocInfo).front();

	// 		vk::DescriptorBufferInfo binfo;
	// 		binfo.buffer = _frames[i]._cameraBuffer._buffer;
	// 		binfo.offset = 0;
	// 		binfo.range = sizeof(vkutils::CameraData);

	// 		vk::WriteDescriptorSet setWrite;
	// 		setWrite.dstBinding = 0;
	// 		setWrite.dstSet = _frames[i]._rasterizerDescriptor;
	// 		setWrite.descriptorCount = 1;
	// 		setWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
	// 		setWrite.setBufferInfo(binfo);

	// 		_device.updateDescriptorSets(setWrite, {});
	// 	}

	// 	_mainDeletionQueue.push_function([&]() {
	// 		_device.destroyDescriptorPool(_rasterizerDescriptorPool);
	// 		for (int i = 0; i < FRAME_OVERLAP; i++)
	// 		{
	// 			_allocator.destroyBuffer(_frames[i]._cameraBuffer._buffer, _frames[i]._cameraBuffer._allocation);
	// 		}
	// 	});
	// }
	//init raytracing descriptors
	{
		std::vector<vk::DescriptorPoolSize> poolSizes = {
			{ vk::DescriptorType::eAccelerationStructureKHR, 1 },
			{ vk::DescriptorType::eStorageImage, 1 },
			{ vk::DescriptorType::eUniformBuffer, 1 }
		};
		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setMaxSets(2);
		pool_info.setPoolSizes(poolSizes);

		_raytracerDescriptorPool = _device.createDescriptorPool(pool_info);

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			_frames[i]._storageImage = createStorageImage();
			_frames[i]._cameraBuffer = vkutils::createBuffer(_allocator, sizeof(vkutils::CameraData), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eAutoPreferDevice, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = _raytracerDescriptorPool;
			allocInfo.setSetLayouts(_raytracerSetLayout);

			_frames[i]._raytracerDescriptor = _device.allocateDescriptorSets(allocInfo).front();

			vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
			descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
			descriptorAccelerationStructureInfo.pAccelerationStructures = &_topLevelAS;
			vk::WriteDescriptorSet accelerationStructureWrite;
			accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
			accelerationStructureWrite.dstSet = _frames[i]._raytracerDescriptor;
			accelerationStructureWrite.dstBinding = 0;
			accelerationStructureWrite.descriptorCount = 1;
			accelerationStructureWrite.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;

			vk::DescriptorImageInfo storageImageDescriptor;
			storageImageDescriptor.imageView = _frames[i]._storageImage._view;
			storageImageDescriptor.imageLayout = vk::ImageLayout::eGeneral;
			vk::WriteDescriptorSet resultImageWrite;
			resultImageWrite.dstSet = _frames[i]._raytracerDescriptor;
			resultImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
			resultImageWrite.dstBinding = 1;
			resultImageWrite.pImageInfo = &storageImageDescriptor;
			resultImageWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo uboDescriptor;
			uboDescriptor.buffer = _frames[i]._cameraBuffer._buffer;
			uboDescriptor.offset = 0;
			uboDescriptor.range = sizeof(vkutils::CameraData);
			vk::WriteDescriptorSet uniformBufferWrite;
			uniformBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			uniformBufferWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
			uniformBufferWrite.dstBinding = 2;
			uniformBufferWrite.pBufferInfo = &uboDescriptor;
			uniformBufferWrite.descriptorCount = 1;

			std::vector<vk::WriteDescriptorSet> setWrites = {
				accelerationStructureWrite,
				resultImageWrite,
				uniformBufferWrite
			};
			_device.updateDescriptorSets(setWrites, {});
		}

		_mainDeletionQueue.push_function([&]() {
			_device.destroyDescriptorPool(_raytracerDescriptorPool);
			for (int i = 0; i < FRAME_OVERLAP; i++)
			{
				_allocator.destroyBuffer(_frames[i]._cameraBuffer._buffer, _frames[i]._cameraBuffer._allocation);
				_allocator.destroyImage(_frames[i]._storageImage._image, _frames[i]._storageImage._allocation);
				_device.destroyImageView(_frames[i]._storageImage._view);
			}
		});
	}
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
	init_bottom_level_acceleration_structure(_triangleModel);
	init_top_level_acceleration_structure();
}

void VulkanEngine::upload_model(Model &model)
{
	vk::DeviceSize vertexBufferSize = model._vertices.size() * sizeof(Vertex);
	vk::DeviceSize indexBufferSize = model._indices.size() * sizeof(uint32_t);

	std::cout << "Vertex Count: " << model._vertices.size() << std::endl;
	//Allocate vertex staging buffer in RAM
	vkutils::AllocatedBuffer _vertexStagingBuffer = vkutils::createBuffer(_allocator, vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc , vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

	//map Vertices
	void* dataVertex = _allocator.mapMemory(_vertexStagingBuffer._allocation);
	memcpy(dataVertex, model._vertices.data(), model._vertices.size() * sizeof(Vertex));
	_allocator.unmapMemory(_vertexStagingBuffer._allocation);

	//Allocate vertex buffer in VRAM
	model._vertexBuffer = vkutils::createBuffer(_allocator, vertexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);

	//upload Buffer to VRAM 
	vkutils::copyBuffer(_device, _frames[0]._commandPool, _graphicsQueue, _vertexStagingBuffer._buffer, model._vertexBuffer._buffer, vertexBufferSize);

	//Allocate index staging Buffer in RAM
	vkutils::AllocatedBuffer _indexStagingBuffer = vkutils::createBuffer(_allocator, indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

	//map Indices
	void* dataIndex = _allocator.mapMemory(_indexStagingBuffer._allocation);
	memcpy(dataIndex, model._indices.data(), model._indices.size() * sizeof(uint32_t));
	_allocator.unmapMemory(_indexStagingBuffer._allocation);
	
	//Allocate index Buffer in VRAM
	model._indexBuffer = vkutils::createBuffer(_allocator, indexBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);

	//upload Buffer to VRAM
	vkutils::copyBuffer(_device, _frames[0]._commandPool, _graphicsQueue, _indexStagingBuffer._buffer, model._indexBuffer._buffer, indexBufferSize);
	
	_allocator.destroyBuffer(_vertexStagingBuffer._buffer, _vertexStagingBuffer._allocation);
	_allocator.destroyBuffer(_indexStagingBuffer._buffer, _indexStagingBuffer._allocation);
	_mainDeletionQueue.push_function([=]() {
		_allocator.destroyBuffer(model._vertexBuffer._buffer, model._vertexBuffer._allocation);
		_allocator.destroyBuffer(model._indexBuffer._buffer, model._indexBuffer._allocation);
    });
}

void VulkanEngine::init_bottom_level_acceleration_structure(Model &model)
{
	std::vector<vk::TransformMatrixKHR> transformMatrices;
	for (auto node : model._linearNodes) {
		for (auto primitive : node->primitives) {
			if (primitive->indexCount > 0) {
				vk::TransformMatrixKHR transformMatrix{};
				auto m = glm::mat3x4(glm::transpose(node->getMatrix()));
				memcpy(&transformMatrix, (void*)&m, sizeof(glm::mat3x4));
				transformMatrices.push_back(transformMatrix);
			}
		}
	}
	vk::DeviceSize transformBufferSize = transformMatrices.size() * sizeof(vk::TransformMatrixKHR);
	vkutils::AllocatedBuffer _transformStagingBuffer = vkutils::createBuffer(_allocator, transformBufferSize, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
	void* dataTransform = _allocator.mapMemory(_transformStagingBuffer._allocation);
	memcpy(dataTransform, transformMatrices.data(), transformBufferSize);
	_allocator.unmapMemory(_transformStagingBuffer._allocation);
	_transformBuffer = vkutils::createBuffer(_allocator, transformBufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);
	vkutils::copyBuffer(_device, _frames[0]._commandPool, _graphicsQueue, _transformStagingBuffer._buffer, _transformBuffer._buffer, transformBufferSize);

	_allocator.destroyBuffer(_transformStagingBuffer._buffer, _transformStagingBuffer._allocation);
	_mainDeletionQueue.push_function([=]() {
		_allocator.destroyBuffer(_transformBuffer._buffer, _transformBuffer._allocation);
	});

	// Build BLAS
	uint32_t maxPrimCount{ 0 };
	std::vector<uint32_t> maxPrimitiveCounts{};
	std::vector<vk::AccelerationStructureGeometryKHR> geometries{};
	std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangeInfos{};
	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos{};
	vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress;
	vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress;
	vk::DeviceOrHostAddressConstKHR transformBufferDeviceAddress;
	vk::BufferDeviceAddressInfo vertexBufferAdressInfo(model._vertexBuffer._buffer);
	vk::BufferDeviceAddressInfo indexBufferAdressInfo(model._indexBuffer._buffer);
	vk::BufferDeviceAddressInfo transformBufferAdressInfo(_transformBuffer._buffer);
	std::vector<vkutils::GeometryNode> geometryNodes{};
	for (auto node : model._linearNodes) {
		for (auto primitive : node->primitives) {
			if (primitive->indexCount > 0) {
				//Device Addresses
				vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress;
				vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress;
				vk::DeviceOrHostAddressConstKHR transformBufferDeviceAddress;
				vertexBufferDeviceAddress.deviceAddress = _device.getBufferAddress(vertexBufferAdressInfo);
				indexBufferDeviceAddress.deviceAddress = _device.getBufferAddress(indexBufferAdressInfo) + primitive->firstIndex * sizeof(uint32_t);
				transformBufferDeviceAddress.deviceAddress = _device.getBufferAddress(transformBufferAdressInfo) + static_cast<uint32_t>(geometries.size()) * sizeof(vk::TransformMatrixKHR);

				//Create Geometry for every gltf primitive (node)
				vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
				triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
				triangles.maxVertex = static_cast<uint32_t>(model._vertices.size());
				triangles.vertexStride = sizeof(Vertex);
				triangles.indexType = vk::IndexType::eUint32;
				triangles.vertexData = vertexBufferDeviceAddress;
				triangles.indexData = indexBufferDeviceAddress;
				triangles.transformData = transformBufferDeviceAddress;

				vk::AccelerationStructureGeometryKHR geometry;
				geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
				geometry.geometry.triangles = triangles;

				geometries.push_back(geometry);
				maxPrimitiveCounts.push_back(primitive->indexCount / 3);
				maxPrimCount += primitive->indexCount / 3;

				vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo;
				buildRangeInfo.firstVertex = 0;
				buildRangeInfo.primitiveOffset = 0;
				buildRangeInfo.primitiveCount = primitive->indexCount / 3;
				buildRangeInfo.transformOffset = 0;
				buildRangeInfos.push_back(buildRangeInfo);
				
				//push Material in same order to Reference it
				vkutils::GeometryNode geometryNode{};
				geometryNode.vertexBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
				geometryNode.indexBufferDeviceAddress = indexBufferDeviceAddress.deviceAddress;
				geometryNodes.push_back(geometryNode);
			}
		}
	}
	for (auto& rangeInfo : buildRangeInfos) {
		pBuildRangeInfos.push_back(&rangeInfo);
	}

	// Get size info
	vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo;
	accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
	accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	accelerationStructureBuildGeometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
	accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();

	auto accelerationStructureBuildSizesInfo = _device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, accelerationStructureBuildGeometryInfo, maxPrimitiveCounts);

	//Build BLAS Buffer
	_bottomLevelASBuffer = vkutils::createBuffer(_allocator, accelerationStructureBuildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, vma::MemoryUsage::eAutoPreferDevice);

	//Get BLAS Handle
	vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo;
	accelerationStructureCreateInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
	accelerationStructureCreateInfo.buffer = _bottomLevelASBuffer._buffer;
	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	_bottomLevelAS = _device.createAccelerationStructureKHR(accelerationStructureCreateInfo);

	_mainDeletionQueue.push_function([=]() {
		_allocator.destroyBuffer(_bottomLevelASBuffer._buffer, _bottomLevelASBuffer._allocation);
		_device.destroyAccelerationStructureKHR(_bottomLevelAS);
	});

	// Create ScratchBuffer
	vkutils::AllocatedBuffer scratchBuffer = vkutils::createBuffer(_allocator, accelerationStructureBuildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice);
	vk::BufferDeviceAddressInfo scratchBufferAdressInfo(scratchBuffer._buffer);
	vk::DeviceOrHostAddressConstKHR scratchBufferAddress;
	scratchBufferAddress.deviceAddress = _device.getBufferAddress(scratchBufferAdressInfo);

	accelerationStructureBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
	accelerationStructureBuildGeometryInfo.dstAccelerationStructure = _bottomLevelAS;
	accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBufferAddress.deviceAddress;

	// Create Single-Use CommandBuffer and Build Acceleration Structure on GPU
	vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = _frames[0]._commandPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer commandBuffer = _device.allocateCommandBuffers(allocInfo).front();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);
		commandBuffer.buildAccelerationStructuresKHR(1, &accelerationStructureBuildGeometryInfo, pBuildRangeInfos.data());
    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(commandBuffer);
    _graphicsQueue.submit(submitInfo);
    _graphicsQueue.waitIdle();
    _device.freeCommandBuffers(_frames[0]._commandPool, commandBuffer);

	vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo;
	accelerationDeviceAddressInfo.accelerationStructure = _bottomLevelAS;
	_bottomLevelDeviceAddress = _device.getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);

	//delete Scratch Buffer
	_allocator.destroyBuffer(scratchBuffer._buffer, scratchBuffer._allocation);
}

void VulkanEngine::init_top_level_acceleration_structure()
{
	VkTransformMatrixKHR transformMatrixC = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f 
	};
	vk::TransformMatrixKHR transformMatrix(transformMatrixC);


	vk::AccelerationStructureInstanceKHR instance(transformMatrix, 0, 0xFF, 0, vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable, _bottomLevelDeviceAddress);

	vkutils::AllocatedBuffer instancesBuffer = vkutils::createBuffer(_allocator, sizeof(vk::AccelerationStructureInstanceKHR), vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
	void* dataInstance = _allocator.mapMemory(instancesBuffer._allocation);
	memcpy(dataInstance, &instance, sizeof(vk::AccelerationStructureInstanceKHR));
	_allocator.unmapMemory(instancesBuffer._allocation);

	vk::DeviceOrHostAddressConstKHR instanceDataDeviceAddress;
	vk::BufferDeviceAddressInfo instanceBufferAdressInfo(instancesBuffer._buffer);
	instanceDataDeviceAddress.deviceAddress = _device.getBufferAddress(instanceBufferAdressInfo);

	vk::AccelerationStructureGeometryInstancesDataKHR instances(VK_FALSE, instanceDataDeviceAddress);

	vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
	accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
	accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
	accelerationStructureGeometry.geometry.instances = instances;

	vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo;
	accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
	accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
	accelerationStructureBuildGeometryInfo.setGeometries(accelerationStructureGeometry);

	uint32_t primitive_count = 1;

	auto accelerationStructureBuildSizesInfo = _device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, accelerationStructureBuildGeometryInfo, primitive_count);

	_topLevelASBuffer = vkutils::createBuffer(_allocator, accelerationStructureBuildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, vma::MemoryUsage::eAutoPreferDevice);

	vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo;
	accelerationStructureCreateInfo.buffer = _topLevelASBuffer._buffer;
	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
	accelerationStructureCreateInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;

	_topLevelAS = _device.createAccelerationStructureKHR(accelerationStructureCreateInfo);

	_mainDeletionQueue.push_function([=]() {
		_allocator.destroyBuffer(_topLevelASBuffer._buffer, _topLevelASBuffer._allocation);
		_device.destroyAccelerationStructureKHR(_topLevelAS);
	});

	vkutils::AllocatedBuffer scratchBuffer = vkutils::createBuffer(_allocator, accelerationStructureBuildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice);
	vk::BufferDeviceAddressInfo scratchBufferAdressInfo(scratchBuffer._buffer);
	vk::DeviceOrHostAddressConstKHR scratchBufferAddress;
	scratchBufferAddress.deviceAddress = _device.getBufferAddress(scratchBufferAdressInfo);

	accelerationStructureBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
	accelerationStructureBuildGeometryInfo.dstAccelerationStructure = _topLevelAS;
	accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBufferAddress.deviceAddress;

	vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo;
	accelerationStructureBuildRangeInfo.primitiveCount = 1;
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;

	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

	vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = _frames[0]._commandPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer cmd = _device.allocateCommandBuffers(allocInfo).front();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmd.begin(beginInfo);
		cmd.buildAccelerationStructuresKHR(1, &accelerationStructureBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());
    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(cmd);
    _graphicsQueue.submit(submitInfo);
    _graphicsQueue.waitIdle();
    _device.freeCommandBuffers(_frames[0]._commandPool, cmd);

	vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo;
	accelerationDeviceAddressInfo.accelerationStructure = _topLevelAS;

	_topLevelDeviceAddress = _device.getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);

	_allocator.destroyBuffer(scratchBuffer._buffer, scratchBuffer._allocation);
	_allocator.destroyBuffer(instancesBuffer._buffer, instancesBuffer._allocation);
}

vkutils::AllocatedImage VulkanEngine::createStorageImage()
{
	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.format = _swachainImageFormat;
	imageInfo.extent.width = _windowExtent.width;
	imageInfo.extent.height = _windowExtent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	vkutils::AllocatedImage storageImage = vkutils::createImage(_allocator, imageInfo, vma::MemoryUsage::eAutoPreferDevice);

	vk::ImageSubresourceRange subresourceRange;
	subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	vk::ImageViewCreateInfo colorImageView;
	colorImageView.viewType = vk::ImageViewType::e2D;
	colorImageView.format = _swachainImageFormat;
	colorImageView.subresourceRange = subresourceRange;
	colorImageView.image = storageImage._image;
	storageImage._view = _device.createImageView(colorImageView);

	vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandPool = _frames[0]._commandPool;
    allocInfo.commandBufferCount = 1;

    vk::CommandBuffer cmd = _device.allocateCommandBuffers(allocInfo).front();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmd.begin(beginInfo);
		vkutils::setImageLayout(cmd, storageImage._image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(cmd);
    _graphicsQueue.submit(submitInfo);
    _graphicsQueue.waitIdle();
    _device.freeCommandBuffers(_frames[0]._commandPool, cmd);

	return storageImage;
}

void VulkanEngine::createShaderBindingTable() {
	const uint32_t handleSize = _raytracingPipelineProperties.shaderGroupHandleSize;
	const uint32_t handleSizeAligned = vkutils::alignedSize(handleSize, _raytracingPipelineProperties.shaderGroupHandleAlignment);
	const uint32_t groupCount = static_cast<uint32_t>(_shaderGroups.size());
	const uint32_t sbtSize = groupCount * handleSizeAligned;

	auto shaderHandleStorage = _device.getRayTracingShaderGroupHandlesKHR<uint8_t>(_raytracerPipeline, (uint32_t) 0, groupCount, (size_t) sbtSize);

	const vk::BufferUsageFlags bufferUsageFlags = vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
	const vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAuto;
	const vma::AllocationCreateFlags allocationFlags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite;
	_raygenShaderBindingTable = vkutils::createBuffer(_allocator, handleSize, bufferUsageFlags, memoryUsage, allocationFlags);
	_missShaderBindingTable =  vkutils::createBuffer(_allocator, handleSize, bufferUsageFlags, memoryUsage, allocationFlags);
	_hitShaderBindingTable =  vkutils::createBuffer(_allocator, handleSize, bufferUsageFlags, memoryUsage, allocationFlags);

	// Copy handles
	void* dataRaygen = _allocator.mapMemory(_raygenShaderBindingTable._allocation);
		memcpy(dataRaygen, shaderHandleStorage.data(), handleSize);
	_allocator.unmapMemory(_raygenShaderBindingTable._allocation);

	void* dataMiss = _allocator.mapMemory(_missShaderBindingTable._allocation);
		memcpy(dataMiss, shaderHandleStorage.data() + handleSizeAligned, handleSize);
	_allocator.unmapMemory(_missShaderBindingTable._allocation);

	void* dataHit = _allocator.mapMemory(_hitShaderBindingTable._allocation);
		memcpy(dataHit, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);
	_allocator.unmapMemory(_hitShaderBindingTable._allocation);

	_mainDeletionQueue.push_function([=]() {
		_allocator.destroyBuffer(_raygenShaderBindingTable._buffer, _raygenShaderBindingTable._allocation);
		_allocator.destroyBuffer(_missShaderBindingTable._buffer, _missShaderBindingTable._allocation);
		_allocator.destroyBuffer(_hitShaderBindingTable._buffer, _hitShaderBindingTable._allocation);
	});
}