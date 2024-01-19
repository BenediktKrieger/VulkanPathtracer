#include <vk_engine.h>
#include <stb_image.h>

void VulkanEngine::init()
{
	SDL_Init(SDL_INIT_VIDEO);

	_core._window = SDL_CreateWindow("Vulkan Pathtracer", _core._windowExtent.width, _core._windowExtent.height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	_cam = Camera(Camera::Type::eTrackBall, _core._window, _core._windowExtent.width, _core._windowExtent.height, glm::vec3(0.5f), glm::vec3(0.f));

	init_vulkan();

	init_swapchain();

	init_default_renderpass();

	init_framebuffers();

	init_commands();

	init_sync_structures();

	init_gui();

	init_accumulation_image();

	init_hdr_map();

	init_ubo();

	load_models();

	init_pipelines();
	
	createShaderBindingTable();
	
	init_descriptors();

	_isInitialized = true;
	_framebufferResized = false;
}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
	{
		_core._device.waitIdle();
		_mainDeletionQueue.flush();
		_resizeDeletionQueue.flush();
		_core._instance.destroySurfaceKHR(_core._surface);
		_core._allocator.destroy();
		_core._device.destroy();
		_core._instance.destroyDebugUtilsMessengerEXT(_core._debug_messenger);
		_core._instance.destroy();
		SDL_DestroyWindow(_core._window);
	}
}

void VulkanEngine::draw()
{
	if (SDL_GetWindowFlags(_core._window) & SDL_WINDOW_MINIMIZED)
		return;

	vk::Result waitFencesResult = _core._device.waitForFences(get_current_frame()._renderFence, true, UINT64_MAX);
	_core._allocator.setCurrentFrameIndex(_frameNumber);
	get_current_frame()._mainCommandBuffer.reset();

	uint32_t swapchainImageIndex;
	vk::Result aquireNextImageResult;
	try{
		aquireNextImageResult = _core._device.acquireNextImageKHR(_core._swapchain, UINT64_MAX, get_current_frame()._presentSemaphore, nullptr, &swapchainImageIndex);
	}
	catch(const std::exception& e){
		std::cerr << e.what() << '\n';
	}
	if(aquireNextImageResult == vk::Result::eErrorOutOfDateKHR || aquireNextImageResult == vk::Result::eSuboptimalKHR || _framebufferResized){
		recreateSwapchain();
		return;
	}
	else if(aquireNextImageResult != vk::Result::eSuccess){
		throw std::runtime_error("failed to acquire swap chain image!");
	}
	_core._device.resetFences(get_current_frame()._renderFence);

	vk::CommandBuffer cmd = get_current_frame()._mainCommandBuffer;

	vk::CommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	vk::ImageSubresourceRange subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
	updateBuffers();

	cmd.begin(cmdBeginInfo);
		if(_gui.settings.renderer == 0)
		{
			vk::RenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _core._windowExtent, _core._framebuffers[swapchainImageIndex]);
			vk::ClearValue colorClear;
			colorClear.color = vk::ClearColorValue(0.1f, 0.1f, 0.1f, 1.0f);
			vk::ClearValue depthClear;
			depthClear.depthStencil = vk::ClearDepthStencilValue(1.f);
			std::array<vk::ClearValue, 2> clearValues = {colorClear, depthClear};
			rpInfo.setClearValues(clearValues);

			cmd.beginRenderPass(rpInfo, vk::SubpassContents::eInline);
			vk::DeviceSize offset = 0;

			vk::Viewport viewport;
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(_core._windowExtent.width);
			viewport.height = static_cast<float>(_core._windowExtent.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			cmd.setViewport(0, 1, &viewport);

			vk::Rect2D scissor;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent = _core._windowExtent;
			cmd.setScissor(0, scissor);

			cmd.bindVertexBuffers(0, 1, &_scene.vertexBuffer._buffer, &offset);
			cmd.bindIndexBuffer(_scene.indexBuffer._buffer, offset, vk::IndexType::eUint32);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _rasterizerPipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _rasterizerPipelineLayout, 0, get_current_frame()._rasterizerDescriptor, {});
			uint32_t vertexOffset = 0;
			uint32_t indexOffset = 0;
			for (auto model : _scene.models){
				for (auto node : model->_linearNodes)
				{
					for(auto primitive : node->primitives)
					{
						PushConstants.model = node->getMatrix();
						cmd.pushConstants(_rasterizerPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(vkutils::PushConstants), &PushConstants);
						cmd.drawIndexed(primitive->indexCount, 1, indexOffset + primitive->firstIndex, vertexOffset, 0);
					}
				}
				vertexOffset += model->_vertices.size();
				indexOffset += model->_indices.size();
			}
			cmd.endRenderPass();
		}
		else
		{
			const uint32_t handleSizeAligned = vkutils::alignedSize(_raytracingPipelineProperties.shaderGroupHandleSize, _raytracingPipelineProperties.shaderGroupHandleAlignment);

			vk::DeviceOrHostAddressConstKHR raygenAddress;
			vk::BufferDeviceAddressInfo raygenAddressInfo(_raygenShaderBindingTable._buffer);
			vk::StridedDeviceAddressRegionKHR raygenShaderSbtEntry;
			raygenShaderSbtEntry.deviceAddress = _core._device.getBufferAddress(raygenAddressInfo);;
			raygenShaderSbtEntry.stride = handleSizeAligned;
			raygenShaderSbtEntry.size = handleSizeAligned;

			vk::DeviceOrHostAddressConstKHR missAddress;
			vk::BufferDeviceAddressInfo missAddressInfo(_missShaderBindingTable._buffer);
			vk::StridedDeviceAddressRegionKHR missShaderSbtEntry;
			missShaderSbtEntry.deviceAddress = _core._device.getBufferAddress(missAddressInfo);
			missShaderSbtEntry.stride = handleSizeAligned;
			missShaderSbtEntry.size = handleSizeAligned;

			vk::DeviceOrHostAddressConstKHR hitAddress;
			vk::BufferDeviceAddressInfo hitAddressInfo(_hitShaderBindingTable._buffer);
			vk::StridedDeviceAddressRegionKHR hitShaderSbtEntry;
			hitShaderSbtEntry.deviceAddress = _core._device.getBufferAddress(hitAddressInfo);
			hitShaderSbtEntry.stride = handleSizeAligned;
			hitShaderSbtEntry.size = handleSizeAligned;

			vk::StridedDeviceAddressRegionKHR callableShaderSbtEntry;

			cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _raytracerPipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _raytracerPipelineLayout, 0, 1, &get_current_frame()._raytracerDescriptor, 0, 0);
			cmd.pushConstants(_raytracerPipelineLayout, vk::ShaderStageFlagBits::eRaygenKHR, 0, sizeof(vkutils::PushConstants), &PushConstants);
			cmd.traceRaysKHR(&raygenShaderSbtEntry, &missShaderSbtEntry, &hitShaderSbtEntry, &callableShaderSbtEntry, _core._windowExtent.width, _core._windowExtent.height, 1);

			vkutils::setImageLayout(cmd, _core._swapchainImages[swapchainImageIndex], vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, subresourceRange);
			vkutils::setImageLayout(cmd, get_current_frame()._storageImage._image, vk::ImageLayout::eGeneral,  vk::ImageLayout::eTransferSrcOptimal, subresourceRange);

			vk::ImageCopy copyRegion;
			copyRegion.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
			copyRegion.srcOffset = vk::Offset3D(0, 0, 0 );
			copyRegion.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
			copyRegion.dstOffset = vk::Offset3D(0, 0, 0 );
			copyRegion.extent = vk::Extent3D(_core._windowExtent.width, _core._windowExtent.height, 1);
			
			cmd.copyImage(get_current_frame()._storageImage._image, vk::ImageLayout::eTransferSrcOptimal, _core._swapchainImages[swapchainImageIndex], vk::ImageLayout::eTransferDstOptimal, copyRegion);

			vkutils::setImageLayout(cmd, _core._swapchainImages[swapchainImageIndex], vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal, subresourceRange);
			vkutils::setImageLayout(cmd, get_current_frame()._storageImage._image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral, subresourceRange);
			PushConstants.accumulatedFrames++;
		}
		_gui.render(cmd, swapchainImageIndex);
	cmd.end();

	vk::SubmitInfo submit = vkinit::submit_info(&cmd);
	vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	submit.setWaitDstStageMask(waitStage);
	submit.setWaitSemaphores(get_current_frame()._presentSemaphore);
	submit.setSignalSemaphores(get_current_frame()._renderSemaphore);

	_core._graphicsQueue.submit(submit, get_current_frame()._renderFence);

	vk::PresentInfoKHR presentInfo = vkinit::present_info();
	presentInfo.setSwapchains(_core._swapchain);
	presentInfo.setWaitSemaphores(get_current_frame()._renderSemaphore);
	presentInfo.setImageIndices(swapchainImageIndex);

	vk::Result queuePresentResult = _core._presentQueue.presentKHR(presentInfo);
	if (queuePresentResult == vk::Result::eErrorOutOfDateKHR || queuePresentResult == vk::Result::eSuboptimalKHR || _framebufferResized) {
		_framebufferResized = false;
		recreateSwapchain();
	} else if (queuePresentResult != vk::Result::eSuccess) {
		throw std::runtime_error("failed to present swap chain image!");
	}

	_frameNumber++;
}

void VulkanEngine::run()
{
	SDL_Event e;
	while (true)
	{
		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			{
				return;
			}
			else if (e.type == SDL_EVENT_KEY_DOWN)
			{
				if (e.key.keysym.sym == SDLK_r)
				{
					_gui.settings.renderer += 1;
					if (_gui.settings.renderer > 1)
					{
						_gui.settings.renderer = 0;
					}
				}
			}
			else if(e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
			{
				int w, h;
				SDL_GetWindowSizeInPixels(_core._window, &w, &h);
				if(_core._windowExtent.width != w || _core._windowExtent.height != h) {
					_framebufferResized = true;
				}
			}
			
			_gui.handleInput(&e);
			auto& io = ImGui::GetIO();
			if(!(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT || e.type == SDL_EVENT_MOUSE_WHEEL) || !io.WantCaptureMouse){
				_cam.handleInputEvent(&e);
			}
		}
		_cam.update();
		_gui.update();
		draw();
	}
}

vkutils::FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
}

void VulkanEngine::init_vulkan()
{
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = _core._dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
	if (_core.useValidationLayers && !vkutils::checkValidationLayerSupport(_core._instanceLayers))
	{
		throw std::runtime_error("validation layers requested, but not available!");
	}
	unsigned int sdl_extensions_count = 0;
	SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count, NULL);
	_core._instanceExtensions.resize(sdl_extensions_count);
	SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count, _core._instanceExtensions.data());

	vk::ApplicationInfo applicationInfo("Vulkan Pathtracer", VK_MAKE_VERSION(0, 0, 1), "VulkanEngine", 1, VK_API_VERSION_1_2);

	try
	{
		if (_core.useValidationLayers)
		{
			_core._instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instanceCreateInfoChain{
				vk::InstanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo, _core._instanceLayers, _core._instanceExtensions),
				vk::DebugUtilsMessengerCreateInfoEXT({}, _core._messageSeverityFlags, _core._messageTypeFlags, vkutils::debugCallback)
			};
			_core._instance = vk::createInstance(instanceCreateInfoChain.get<vk::InstanceCreateInfo>());
		}
		else
		{
			vk::InstanceCreateInfo instanceCreateInfo(vk::InstanceCreateFlags(), &applicationInfo, {}, _core._instanceExtensions);
			_core._instance = vk::createInstance(instanceCreateInfo);
		}
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	VULKAN_HPP_DEFAULT_DISPATCHER.init(_core._instance);

	if (_core.useValidationLayers)
	{
		vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo({}, _core._messageSeverityFlags, _core._messageTypeFlags, vkutils::debugCallback);
		try
		{
			_core._debug_messenger = _core._instance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo, nullptr);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
	}

	SDL_Vulkan_CreateSurface(_core._window, _core._instance, reinterpret_cast<VkSurfaceKHR *>(&_core._surface));

	std::vector<vk::PhysicalDevice> devices = _core._instance.enumeratePhysicalDevices();
	bool deviceFound = false;
	for (auto &device : devices)
	{
		if (vkutils::isDeviceSuitable(device, _core._surface, _core._deviceExtensions))
		{
			deviceFound = true;
			_core._chosenGPU = device;
			break;
		}
	}
	if (!deviceFound)
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	vkutils::QueueFamilyIndices indices = vkutils::findQueueFamilies(_core._chosenGPU, _core._surface);
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamily, 1, &queuePriority);
		queueCreateInfos.push_back(queueCreateInfo);
	}

	// MacOS portability extension
	std::vector<vk::ExtensionProperties> extensionProperties = _core._chosenGPU.enumerateDeviceExtensionProperties();
	for (auto extensionProperty : extensionProperties)
	{
		if (std::string(extensionProperty.extensionName.data()) == std::string(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
			_core._deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
	}

	vk::DeviceCreateInfo createInfo;
	if (_core.useValidationLayers)
	{
		createInfo = vk::DeviceCreateInfo({}, queueCreateInfos, _core._instanceLayers, _core._deviceExtensions, {});
	}
	else
	{
		createInfo = vk::DeviceCreateInfo({}, queueCreateInfos, {}, _core._deviceExtensions, {});
	}
	vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR, vk::PhysicalDeviceAccelerationStructureFeaturesKHR, vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceDescriptorIndexingFeatures> deviceCreateInfo = {
		createInfo,
		vk::PhysicalDeviceFeatures2().setFeatures(vk::PhysicalDeviceFeatures().setSamplerAnisotropy(true).setShaderInt64(true)),
		vk::PhysicalDeviceRayTracingPipelineFeaturesKHR().setRayTracingPipeline(true),
		vk::PhysicalDeviceAccelerationStructureFeaturesKHR().setAccelerationStructure(true),
		vk::PhysicalDeviceBufferDeviceAddressFeatures().setBufferDeviceAddress(true),
		vk::PhysicalDeviceDescriptorIndexingFeatures().setRuntimeDescriptorArray(true)
	};
	auto _physicalDeviceProperties = _core._chosenGPU.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR, vk::PhysicalDeviceAccelerationStructurePropertiesKHR, vk::PhysicalDeviceDescriptorIndexingProperties>();
	_raytracingPipelineProperties = _physicalDeviceProperties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
	try
	{
		_core._device = _core._chosenGPU.createDevice(deviceCreateInfo.get<vk::DeviceCreateInfo>());
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	VULKAN_HPP_DEFAULT_DISPATCHER.init(_core._device);
	_core._graphicsQueue = _core._device.getQueue(indices.graphicsFamily.value(), 0);
	_core._presentQueue = _core._device.getQueue(indices.presentFamily.value(), 0);

	vma::AllocatorCreateInfo allocatorInfo = vma::AllocatorCreateInfo(vma::AllocatorCreateFlagBits::eExtMemoryBudget | vma::AllocatorCreateFlagBits::eBufferDeviceAddress, _core._chosenGPU, _core._device, {}, {}, {}, {}, {}, _core._instance, VK_API_VERSION_1_2);
	try
	{
		_core._allocator = vma::createAllocator(allocatorInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}

	std::vector<vma::Budget> heapBudgets = _core._allocator.getHeapBudgets();

}

void VulkanEngine::init_swapchain()
{
	vkutils::SwapChainSupportDetails swapChainSupport = vkutils::querySwapChainSupport(_core._chosenGPU, _core._surface);

	vk::SurfaceFormatKHR surfaceFormat = vkutils::chooseSwapSurfaceFormat(swapChainSupport.formats);
	vk::PresentModeKHR presentMode = vkutils::chooseSwapPresentMode(vk::PresentModeKHR::eMailbox, swapChainSupport.presentModes);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	vkutils::QueueFamilyIndices indices = vkutils::findQueueFamilies(_core._chosenGPU, _core._surface);
	uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	vk::SwapchainCreateInfoKHR createInfo;
	createInfo.surface = _core._surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = _core._windowExtent;
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
		_core._swapchain = _core._device.createSwapchainKHR(createInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_resizeDeletionQueue.push_function([=](){
		_core._device.destroySwapchainKHR(_core._swapchain);
	});

	_core._swapchainImageFormat = surfaceFormat.format;

	_core._swapchainImages = _core._device.getSwapchainImagesKHR(_core._swapchain);

	_core._swapchainImageViews.resize(_core._swapchainImages.size());
	for (size_t i = 0; i < _core._swapchainImages.size(); i++)
	{
		_core._swapchainImageViews[i] = vkutils::createImageView(_core, _core._swapchainImages[i], _core._swapchainImageFormat, vk::ImageAspectFlagBits::eColor);
	}

	_depthFormat = vk::Format::eD32Sfloat;
	vk::ImageCreateInfo dimg_info = vkinit::image_create_info(_depthFormat, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::Extent3D{_core._windowExtent.width, _core._windowExtent.height, 1});
	_depthImage = vkutils::createImage(_core, dimg_info, vk::ImageAspectFlagBits::eDepth, vma::MemoryUsage::eAutoPreferDevice);

	_resizeDeletionQueue.push_function([=]() {
		_core._device.destroyImageView(_depthImage._view);
		_core._allocator.destroyImage(_depthImage._image, _depthImage._allocation);
	});
}

void VulkanEngine::init_framebuffers()
{
	vk::FramebufferCreateInfo createInfo = vkinit::framebuffer_create_info(_renderPass, _core._windowExtent);

	const uint32_t swapchain_imagecount = (uint32_t)_core._swapchainImages.size();
	_core._framebuffers.resize(swapchain_imagecount);

	for (unsigned int i = 0; i < swapchain_imagecount; i++)
	{
		std::array<vk::ImageView, 2> attachments = {_core._swapchainImageViews[i], _depthImage._view};
		createInfo.setAttachments(attachments);
		try
		{
			_core._framebuffers[i] = _core._device.createFramebuffer(createInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_resizeDeletionQueue.push_function([=](){
			_core._device.destroyFramebuffer(_core._framebuffers[i]);
			_core._device.destroyImageView(_core._swapchainImageViews[i]);
		});
	}
}

void VulkanEngine::init_default_renderpass()
{
	vk::AttachmentDescription color_attachment;
	color_attachment.setFormat(_core._swapchainImageFormat);
	color_attachment.setSamples(vk::SampleCountFlagBits::e1);
	color_attachment.setLoadOp(vk::AttachmentLoadOp::eClear);
	color_attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
	color_attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
	color_attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
	color_attachment.setInitialLayout(vk::ImageLayout::eUndefined);
	color_attachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

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
		_renderPass = _core._device.createRenderPass(render_pass_info);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_resizeDeletionQueue.push_function([=](){ 
		_core._device.destroyRenderPass(_renderPass, nullptr); 
	});
}

void VulkanEngine::init_commands()
{
	vk::CommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_core._graphicsQueueFamily);

	try
	{
		_core._cmdPool = _core._device.createCommandPool(commandPoolInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	_mainDeletionQueue.push_function([=](){
		_core._device.destroyCommandPool(_core._cmdPool, nullptr);
	});

	commandPoolInfo = vkinit::command_pool_create_info(_core._graphicsQueueFamily, vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		try
		{
			_frames[i]._commandPool = _core._device.createCommandPool(commandPoolInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}

		vk::CommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1, vk::CommandBufferLevel::ePrimary);
		try
		{
			_frames[i]._mainCommandBuffer = _core._device.allocateCommandBuffers(cmdAllocInfo).front();
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_mainDeletionQueue.push_function([=](){
			_core._device.destroyCommandPool(_frames[i]._commandPool, nullptr);
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
			_frames[i]._renderFence = _core._device.createFence(fenceCreateInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_resizeDeletionQueue.push_function([=](){
			_core._device.destroyFence(_frames[i]._renderFence, nullptr); 
		});
		try
		{
			_frames[i]._presentSemaphore = _core._device.createSemaphore(semaphoreCreateInfo);
			_frames[i]._renderSemaphore = _core._device.createSemaphore(semaphoreCreateInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
		_resizeDeletionQueue.push_function([=]()
		{
			_core._device.destroySemaphore(_frames[i]._presentSemaphore, nullptr);
			_core._device.destroySemaphore(_frames[i]._renderSemaphore, nullptr); 
		});
	}
}

void VulkanEngine::init_gui()
{
	_gui = vk::GUI(&_core);
	_mainDeletionQueue.push_function([=]() {
		_gui.destroy();
	});
}

void VulkanEngine::init_accumulation_image()
{
	_accumulationImage = createStorageImage(vk::Format::eR32G32B32A32Sfloat, 3840, 2160);
	_mainDeletionQueue.push_function([=]() {
		_core._allocator.destroyImage(_accumulationImage._image, _accumulationImage._allocation);
		_core._device.destroyImageView(_accumulationImage._view); 
	});
}

void VulkanEngine::init_hdr_map()
{
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::eLinear;
	samplerInfo.minFilter = vk::Filter::eLinear;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eMirroredRepeat;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eMirroredRepeat;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eMirroredRepeat;
	samplerInfo.compareOp = vk::CompareOp::eNever;
	samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
	samplerInfo.maxLod = 1;
	samplerInfo.maxAnisotropy = 8.0f;
	samplerInfo.anisotropyEnable = true;
	_envMapSampler = _core._device.createSampler(samplerInfo);

	int texWidth, texHeight, texChannels;
	stbi_set_flip_vertically_on_load(true);
    float* pixels = stbi_loadf(ASSET_PATH"/environment_maps/sea.hdr", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	stbi_set_flip_vertically_on_load(false);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

	vk::Format format = vk::Format::eR32G32B32A32Sfloat;
	uint32_t width = texWidth;
	uint32_t height = texHeight;

	auto formatProperties = _core._chosenGPU.getFormatProperties(format);
	if(!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc) || !(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
	{
		throw std::runtime_error("unsported Image Format!");
	}
	vk::ImageCreateInfo imageCreateInfo;
	imageCreateInfo.imageType = vk::ImageType::e2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageCreateInfo.extent = vk::Extent3D{ width, height, 1 };
	imageCreateInfo.usage = vk::ImageUsageFlagBits::eSampled;
	_hdrMap = vkutils::imageFromData(_core, pixels, imageCreateInfo, vk::ImageAspectFlagBits::eColor, vma::MemoryUsage::eAutoPreferDevice);

	_mainDeletionQueue.push_function([=]() {
		_core._allocator.destroyImage(_hdrMap._image, _hdrMap._allocation);
		_core._device.destroyImageView(_hdrMap._view);
		_core._device.destroySampler(_envMapSampler);
	});
}

void VulkanEngine::init_ubo() {
	_settingsUBO.accumulate = _gui.settings.accumulate;
    _settingsUBO.samples = _gui.settings.samples;
    _settingsUBO.reflection_recursion = _gui.settings.reflection_recursion;
    _settingsUBO.refraction_recursion = _gui.settings.refraction_recursion;

	_settingsBuffer = vkutils::hostBufferFromData(_core, &_settingsUBO, sizeof(vkutils::Shadersettings), vk::BufferUsageFlagBits::eUniformBuffer, vma::MemoryUsage::eAutoPreferDevice, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

	_mainDeletionQueue.push_function([=]() {
		_core._allocator.destroyBuffer(_settingsBuffer._buffer, _settingsBuffer._allocation);
	});
}

void VulkanEngine::init_pipelines()
{
	// init rasterization pipeline
	{
		vk::DescriptorSetLayoutBinding materialBufferBinding;
		materialBufferBinding.binding = 0;
		materialBufferBinding.descriptorCount = 1;
		materialBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		materialBufferBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

		vk::DescriptorSetLayoutCreateInfo setinfo;
		setinfo.setBindings(materialBufferBinding);

		_rasterizerSetLayout = _core._device.createDescriptorSetLayout(setinfo);

		vk::ShaderModule vertexShader = load_shader_module(vk::ShaderStageFlagBits::eVertex, "/triangle.vert");
		vk::ShaderModule fragShader = load_shader_module(vk::ShaderStageFlagBits::eFragment, "/triangle.frag");

		vk::PipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
		pipeline_layout_info.setSetLayouts(_rasterizerSetLayout);
		vk::PushConstantRange push_constants{vk::ShaderStageFlagBits::eVertex, 0, sizeof(vkutils::PushConstants)};
		pipeline_layout_info.setPushConstantRanges(push_constants);

		_rasterizerPipelineLayout = _core._device.createPipelineLayout(pipeline_layout_info);

		VertexInputDescription vertexDescription = Vertex::get_vertex_description();

		std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicState;
        dynamicState.setDynamicStates(dynamicStates);

		vkutils::PipelineBuilder pipelineBuilder;
		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eVertex, vertexShader));
		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eFragment, fragShader));
		pipelineBuilder._vertexInputInfo.setVertexAttributeDescriptions(vertexDescription.attributes);
		pipelineBuilder._vertexInputInfo.setVertexBindingDescriptions(vertexDescription.bindings);
		pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(vk::PrimitiveTopology::eTriangleList);
		pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(vk::PolygonMode::eFill);
		pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();
		pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();
		pipelineBuilder._pipelineLayout = _rasterizerPipelineLayout;
		pipelineBuilder._dynamicStates = std::vector<vk::DynamicState> {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, vk::CompareOp::eLessOrEqual);
		_rasterizerPipeline = pipelineBuilder.build_pipeline(_core._device, _renderPass);

		_core._device.destroyShaderModule(fragShader);
		_core._device.destroyShaderModule(vertexShader);

		_mainDeletionQueue.push_function([=]() {
			_core._device.destroyPipeline(_rasterizerPipeline);
			_core._device.destroyPipelineLayout(_rasterizerPipelineLayout);
			_core._device.destroyDescriptorSetLayout(_rasterizerSetLayout);
		});
	}

	// init raytracing pipeline
	{
		vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
		accelerationStructureLayoutBinding.binding = 0;
		accelerationStructureLayoutBinding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
		accelerationStructureLayoutBinding.descriptorCount = 1;
		accelerationStructureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR;

		vk::DescriptorSetLayoutBinding resultImageLayoutBinding;
		resultImageLayoutBinding.binding = 1;
		resultImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		resultImageLayoutBinding.descriptorCount = 1;
		resultImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		vk::DescriptorSetLayoutBinding accumulationImageLayoutBinding;
		accumulationImageLayoutBinding.binding = 2;
		accumulationImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		accumulationImageLayoutBinding.descriptorCount = 1;
		accumulationImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		vk::DescriptorSetLayoutBinding indexBufferBinding;
		indexBufferBinding.binding = 3;
		indexBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		indexBufferBinding.descriptorCount = 1;
		indexBufferBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

		vk::DescriptorSetLayoutBinding vertexBufferBinding;
		vertexBufferBinding.binding = 4;
		vertexBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		vertexBufferBinding.descriptorCount = 1;
		vertexBufferBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

		vk::DescriptorSetLayoutBinding materialBufferBinding;
		materialBufferBinding.binding = 5;
		materialBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		materialBufferBinding.descriptorCount = 1;
		materialBufferBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

		vk::DescriptorSetLayoutBinding hdrMapLayoutBinding{};
        hdrMapLayoutBinding.binding = 6;
        hdrMapLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        hdrMapLayoutBinding.descriptorCount = 1;
        hdrMapLayoutBinding.pImmutableSamplers = nullptr;
        hdrMapLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eMissKHR;

		vk::DescriptorSetLayoutBinding settingsBufferBinding;
		settingsBufferBinding.binding = 7;
		settingsBufferBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
		settingsBufferBinding.descriptorCount = 1;
		settingsBufferBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eMissKHR;

		vk::DescriptorSetLayoutBinding textureLayoutBinding{};
        textureLayoutBinding.binding = 8;
        textureLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        textureLayoutBinding.descriptorCount = static_cast<uint32_t>(_scene.textures.size());
        textureLayoutBinding.pImmutableSamplers = nullptr;
        textureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

		std::vector<vk::DescriptorSetLayoutBinding> bindings({
			accelerationStructureLayoutBinding,
			resultImageLayoutBinding,
			accumulationImageLayoutBinding,
			indexBufferBinding,
			vertexBufferBinding,
			materialBufferBinding,
			hdrMapLayoutBinding,
			settingsBufferBinding,
			textureLayoutBinding
		});

		vk::DescriptorSetLayoutCreateInfo setinfo;
		setinfo.setBindings(bindings);

		_raytracerSetLayout = _core._device.createDescriptorSetLayout(setinfo);

		vk::PipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
		pipeline_layout_info.setSetLayouts(_raytracerSetLayout);
		vk::PushConstantRange push_constants{vk::ShaderStageFlagBits::eRaygenKHR, 0, sizeof(vkutils::PushConstants)};
		pipeline_layout_info.setPushConstantRanges(push_constants);

		_raytracerPipelineLayout = _core._device.createPipelineLayout(pipeline_layout_info);

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
		vk::ShaderModule raygenShader, missShader, missShadow, hitShader, aHitShader;

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

		// Miss group - Shadow
		{
			missShadow = load_shader_module(vk::ShaderStageFlagBits::eMissKHR, "/shadow.rmiss");
			shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eMissKHR, missShadow));
			vk::RayTracingShaderGroupCreateInfoKHR shaderGroup;
			shaderGroup.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
			shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.closestHitShader = vk::ShaderUnusedKhr;
			shaderGroup.anyHitShader = vk::ShaderUnusedKhr;
			shaderGroup.intersectionShader = vk::ShaderUnusedKhr;
			_shaderGroups.push_back(shaderGroup);
		}

		// Hit group - Triangles
		{
			hitShader = load_shader_module(vk::ShaderStageFlagBits::eClosestHitKHR, "/simple.rchit");
			shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eClosestHitKHR, hitShader));
			aHitShader = load_shader_module(vk::ShaderStageFlagBits::eAnyHitKHR, "/simple.rahit");
			shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eAnyHitKHR, aHitShader));
			vk::RayTracingShaderGroupCreateInfoKHR shaderGroup;
			shaderGroup.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
			shaderGroup.generalShader = vk::ShaderUnusedKhr;
			shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 2;
			shaderGroup.anyHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
			shaderGroup.intersectionShader = vk::ShaderUnusedKhr;
			_shaderGroups.push_back(shaderGroup);
		}

		// Create the ray tracing pipeline
		vk::RayTracingPipelineCreateInfoKHR rayTracingPipelineInfo;
		rayTracingPipelineInfo.setStages(shaderStages);
		rayTracingPipelineInfo.setGroups(_shaderGroups);
		rayTracingPipelineInfo.maxPipelineRayRecursionDepth = 31;
		rayTracingPipelineInfo.layout = _raytracerPipelineLayout;
		
		try
		{
			vk::Result result;
			std::tie(result, _raytracerPipeline) = _core._device.createRayTracingPipelineKHR({}, {}, rayTracingPipelineInfo);
			if (result != vk::Result::eSuccess)
			{
				throw std::runtime_error("failed to create graphics Pipeline!");
			}
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}

		_core._device.destroyShaderModule(raygenShader);
		_core._device.destroyShaderModule(missShader);
		_core._device.destroyShaderModule(missShadow);
		_core._device.destroyShaderModule(hitShader);
		_core._device.destroyShaderModule(aHitShader);

		_mainDeletionQueue.push_function([=]() {
			_core._device.destroyPipeline(_raytracerPipeline);
			_core._device.destroyPipelineLayout(_raytracerPipelineLayout);
			_core._device.destroyDescriptorSetLayout(_raytracerSetLayout);
		});
	}
}

void VulkanEngine::init_descriptors()
{
	//init rasterizing descriptors
	{
		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			{vk::DescriptorType::eStorageBuffer, 1 }
		};

		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setMaxSets(2);
		pool_info.setPoolSizes(poolSizes);

		_rasterizerDescriptorPool = _core._device.createDescriptorPool(pool_info);
		
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = _rasterizerDescriptorPool;
			allocInfo.setSetLayouts(_rasterizerSetLayout);

			_frames[i]._rasterizerDescriptor = _core._device.allocateDescriptorSets(allocInfo).front();

			vk::DescriptorBufferInfo binfo;
			binfo.buffer = _scene.materialBuffer._buffer;
			binfo.offset = 0;
			binfo.range = _scene.materials.size() * sizeof(vkutils::Material);

			vk::WriteDescriptorSet setWrite;
			setWrite.dstBinding = 0;
			setWrite.dstSet = _frames[i]._rasterizerDescriptor;
			setWrite.descriptorCount = 1;
			setWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			setWrite.setBufferInfo(binfo);

			_core._device.updateDescriptorSets(setWrite, {});
		}
	}
	//init raytracing descriptors
	{
		std::vector<vk::DescriptorPoolSize> poolSizes = {
			{ vk::DescriptorType::eAccelerationStructureKHR, 1 },
			{ vk::DescriptorType::eStorageImage, 1 },
			{ vk::DescriptorType::eStorageImage, 1 },
			{ vk::DescriptorType::eStorageBuffer, 1 },
			{ vk::DescriptorType::eStorageBuffer, 1 },
			{ vk::DescriptorType::eStorageBuffer, 1 },
			{ vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(_scene.textures.size())}
		};
		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setMaxSets(2);
		pool_info.setPoolSizes(poolSizes);

		_raytracerDescriptorPool = _core._device.createDescriptorPool(pool_info);

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			_frames[i]._storageImage = createStorageImage(_core._swapchainImageFormat, _core._windowExtent.width, _core._windowExtent.height);

			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = _raytracerDescriptorPool;
			allocInfo.setSetLayouts(_raytracerSetLayout);

			_frames[i]._raytracerDescriptor = _core._device.allocateDescriptorSets(allocInfo).front();

			vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
			descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
			descriptorAccelerationStructureInfo.pAccelerationStructures = &_scene.tlas;
			vk::WriteDescriptorSet accelerationStructureWrite;
			accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
			accelerationStructureWrite.dstSet = _frames[i]._raytracerDescriptor;
			accelerationStructureWrite.dstBinding = 0;
			accelerationStructureWrite.descriptorCount = 1;
			accelerationStructureWrite.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;

			vk::DescriptorImageInfo resultImageDescriptor;
			resultImageDescriptor.imageView = _frames[i]._storageImage._view;
			resultImageDescriptor.imageLayout = vk::ImageLayout::eGeneral;
			vk::WriteDescriptorSet resultImageWrite;
			resultImageWrite.dstSet = _frames[i]._raytracerDescriptor;
			resultImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
			resultImageWrite.dstBinding = 1;
			resultImageWrite.pImageInfo = &resultImageDescriptor;
			resultImageWrite.descriptorCount = 1;

			vk::DescriptorImageInfo accumulationImageDescriptor;
			accumulationImageDescriptor.imageView = _accumulationImage._view;
			accumulationImageDescriptor.imageLayout = vk::ImageLayout::eGeneral;
			vk::WriteDescriptorSet accumulationImageWrite;
			accumulationImageWrite.dstSet = _frames[i]._raytracerDescriptor;
			accumulationImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
			accumulationImageWrite.dstBinding = 2;
			accumulationImageWrite.pImageInfo = &accumulationImageDescriptor;
			accumulationImageWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo indexDescriptor;
			indexDescriptor.buffer = _scene.indexBuffer._buffer;
			indexDescriptor.offset = 0;
			indexDescriptor.range = _scene.indices.size() * sizeof(uint32_t);
			vk::WriteDescriptorSet indexBufferWrite;
			indexBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			indexBufferWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			indexBufferWrite.dstBinding = 3;
			indexBufferWrite.pBufferInfo = &indexDescriptor;
			indexBufferWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo vertexDescriptor;
			vertexDescriptor.buffer = _scene.vertexBuffer._buffer;
			vertexDescriptor.offset = 0;
			vertexDescriptor.range = _scene.vertices.size() * sizeof(Vertex);
			vk::WriteDescriptorSet vertexBufferWrite;
			vertexBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			vertexBufferWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			vertexBufferWrite.dstBinding = 4;
			vertexBufferWrite.pBufferInfo = &vertexDescriptor;
			vertexBufferWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo uboDescriptor;
			uboDescriptor.buffer = _scene.materialBuffer._buffer;
			uboDescriptor.offset = 0;
			uboDescriptor.range = _scene.materials.size() * sizeof(vkutils::Material);
			vk::WriteDescriptorSet uniformBufferWrite;
			uniformBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			uniformBufferWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			uniformBufferWrite.dstBinding = 5;
			uniformBufferWrite.pBufferInfo = &uboDescriptor;
			uniformBufferWrite.descriptorCount = 1;

			vk::DescriptorImageInfo hdrImageDescriptor;
			hdrImageDescriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			hdrImageDescriptor.imageView = _hdrMap._view;
			hdrImageDescriptor.sampler = _envMapSampler;
			vk::WriteDescriptorSet hdrImageWrite;
			hdrImageWrite.dstSet = _frames[i]._raytracerDescriptor;
			hdrImageWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			hdrImageWrite.dstBinding = 6;
			hdrImageWrite.pImageInfo = &hdrImageDescriptor;
			hdrImageWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo settingsUboDescriptor;
			settingsUboDescriptor.buffer = _settingsBuffer._buffer;
			settingsUboDescriptor.offset = 0;
			settingsUboDescriptor.range = sizeof(vkutils::Shadersettings);
			vk::WriteDescriptorSet settingsUniformBufferWrite;
			settingsUniformBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			settingsUniformBufferWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
			settingsUniformBufferWrite.dstBinding = 7;
			settingsUniformBufferWrite.pBufferInfo = &settingsUboDescriptor;
			settingsUniformBufferWrite.descriptorCount = 1;

			vk::WriteDescriptorSet textureImageWrite;
            textureImageWrite.dstSet = _frames[i]._raytracerDescriptor;
            textureImageWrite.dstBinding = 8;
            textureImageWrite.dstArrayElement = 0;
            textureImageWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
			std::vector<vk::DescriptorImageInfo> imageInfos = _scene.getTextureDescriptors();
            textureImageWrite.setImageInfo(imageInfos);

			std::vector<vk::WriteDescriptorSet> setWrites = {
				accelerationStructureWrite,
				resultImageWrite,
				accumulationImageWrite,
				indexBufferWrite,
				vertexBufferWrite,
				uniformBufferWrite,
				hdrImageWrite,
				settingsUniformBufferWrite,
				textureImageWrite
			};
			_core._device.updateDescriptorSets(setWrites, {});
		}
	}
	_mainDeletionQueue.push_function([&]() {
		_core._device.destroyDescriptorPool(_raytracerDescriptorPool);
		_core._device.destroyDescriptorPool(_rasterizerDescriptorPool);
	});
	_resizeDeletionQueue.push_function([&]() {
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			_core._allocator.destroyImage(_frames[i]._storageImage._image, _frames[i]._storageImage._allocation);
			_core._device.destroyImageView(_frames[i]._storageImage._view);
		}
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
		shaderModule = _core._device.createShaderModule(createInfo);
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
	_scene = Scene(_core);
	_scene.add(ASSET_PATH"/models/san_miguel.glb");
	//_scene.add(ASSET_PATH"/models/bathroom.glb");
	_scene.buildAccelerationStructure();
	_mainDeletionQueue.push_function([&]() {
		_scene.destroy();
	});
}

void VulkanEngine::updateBuffers() { 
	// write camdata to push constant struct
	glm::mat4 view = _cam.getView();
	glm::mat4 projection = glm::perspective(glm::radians(_fov), (float) _core._windowExtent.width / _core._windowExtent.height, 0.1f, 1000.0f);
	projection[1][1] *= -1;
	if(_gui.settings.renderer == 0)
	{
		PushConstants.proj = projection;
		PushConstants.view = view;
		PushConstants.model = glm::mat4(1.f);
	}
	else
	{
		PushConstants.proj = glm::inverse(projection);
		PushConstants.view = glm::inverse(view);
		PushConstants.model = glm::mat4(1.f);
	}
	if(_cam.changed) {
		PushConstants.accumulatedFrames = 0;
		_cam.changed = false;
	}
	if(_settingsUBO.accumulate != _gui.settings.accumulate > 0 || _settingsUBO.samples != _gui.settings.samples || _settingsUBO.reflection_recursion != _gui.settings.reflection_recursion || _settingsUBO.refraction_recursion != _gui.settings.refraction_recursion || _gui.settings.fov != _fov || _settingsUBO.ambient_multiplier != _gui.settings.ambient_multiplier){
		_cam.changed = true;
	}
	// shwo cam pos
	glm::vec3 cam_pos = _cam.getPosition();
	glm::vec3 cam_dir = _cam.getDirection();
	_gui.settings.cam_pos[0] = cam_pos.x;
	_gui.settings.cam_pos[1] = cam_pos.y;
	_gui.settings.cam_pos[2] = cam_pos.z;
	_gui.settings.cam_dir[0] = cam_dir.x;
	_gui.settings.cam_dir[1] = cam_dir.y;
	_gui.settings.cam_dir[2] = cam_dir.z;
	//write settings to gpu buffer
	_fov = _gui.settings.fov;
    _settingsUBO.accumulate = _gui.settings.accumulate ? 1 : 0;
    _settingsUBO.samples = _gui.settings.samples;
    _settingsUBO.reflection_recursion = _gui.settings.reflection_recursion;
    _settingsUBO.refraction_recursion = _gui.settings.refraction_recursion;
	_settingsUBO.ambient_multiplier = _gui.settings.ambient_multiplier;
	void* mapped = _core._allocator.mapMemory(_settingsBuffer._allocation);
	    memcpy(mapped, &_settingsUBO, sizeof(vkutils::Shadersettings));
	_core._allocator.unmapMemory(_settingsBuffer._allocation);
}

// void VulkanEngine::upload_model(Model &model)
// {
// 	vk::DeviceSize vertexBufferSize = model._vertices.size() * sizeof(Vertex);
// 	vk::DeviceSize indexBufferSize = model._indices.size() * sizeof(uint32_t);

// 	std::cout << "Vertex Count: " << model._vertices.size() << std::endl;
// 	model._vertexBuffer = vkutils::deviceBufferFromData(_core, (void*) model._vertices.data(), vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);
// 	model._indexBuffer = vkutils::deviceBufferFromData(_core, (void*) model._indices.data(), indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);
// }

// void VulkanEngine::init_bottom_level_acceleration_structure(Model &model)
// {
// 	std::vector<vk::TransformMatrixKHR> transformMatrices;
// 	for (auto node : model._linearNodes) {
// 		for (auto primitive : node->primitives) {
// 			if (primitive->indexCount > 0) {
// 				vk::TransformMatrixKHR transformMatrix{};
// 				auto m = glm::mat3x4(glm::transpose(node->getMatrix()));
// 				memcpy(&transformMatrix, (void*)&m, sizeof(glm::mat3x4));
// 				transformMatrices.push_back(transformMatrix);
// 			}
// 		}
// 	}

// 	vk::DeviceSize transformBufferSize = transformMatrices.size() * sizeof(vk::TransformMatrixKHR);
// 	vkutils::AllocatedBuffer transformBuffer = vkutils::deviceBufferFromData(_core, transformMatrices.data(), transformBufferSize, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);

// 	// Build BLAS
// 	uint32_t maxPrimCount{ 0 };
// 	std::vector<uint32_t> maxPrimitiveCounts{};
// 	std::vector<vk::AccelerationStructureGeometryKHR> geometries{};
// 	std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangeInfos{};
// 	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos{};
// 	vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress;
// 	vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress;
// 	vk::DeviceOrHostAddressConstKHR transformBufferDeviceAddress;
// 	vk::BufferDeviceAddressInfo vertexBufferAdressInfo(model._vertexBuffer._buffer);
// 	vk::BufferDeviceAddressInfo indexBufferAdressInfo(model._indexBuffer._buffer);
// 	vk::BufferDeviceAddressInfo transformBufferAdressInfo(transformBuffer._buffer);
// 	for (auto node : model._linearNodes) {
// 		for (auto primitive : node->primitives) {
// 			if (primitive->indexCount > 0) {
// 				//Device Addresses
// 				vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress;
// 				vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress;
// 				vk::DeviceOrHostAddressConstKHR transformBufferDeviceAddress;
// 				vertexBufferDeviceAddress.deviceAddress = _core._device.getBufferAddress(vertexBufferAdressInfo);
// 				indexBufferDeviceAddress.deviceAddress = _core._device.getBufferAddress(indexBufferAdressInfo) + primitive->firstIndex * sizeof(uint32_t);
// 				transformBufferDeviceAddress.deviceAddress = _core._device.getBufferAddress(transformBufferAdressInfo) + static_cast<uint32_t>(geometries.size()) * sizeof(vk::TransformMatrixKHR);

// 				//Create Geometry for every gltf primitive (node)
// 				vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
// 				triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
// 				triangles.maxVertex = static_cast<uint32_t>(model._vertices.size());
// 				triangles.vertexStride = sizeof(Vertex);
// 				triangles.indexType = vk::IndexType::eUint32;
// 				triangles.vertexData = vertexBufferDeviceAddress;
// 				triangles.indexData = indexBufferDeviceAddress;
// 				triangles.transformData = transformBufferDeviceAddress;

// 				vk::AccelerationStructureGeometryKHR geometry;
// 				geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
// 				geometry.geometry.triangles = triangles;
// 				if(primitive->material.alphaMode != Material::ALPHAMODE_OPAQUE)
// 				{
// 					geometry.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
// 				}
// 				else
// 				{
// 					geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
// 				}

// 				geometries.push_back(geometry);
// 				maxPrimitiveCounts.push_back(primitive->indexCount / 3);
// 				maxPrimCount += primitive->indexCount / 3;

// 				vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo;
// 				buildRangeInfo.firstVertex = 0;
// 				buildRangeInfo.primitiveOffset = 0;
// 				buildRangeInfo.primitiveCount = primitive->indexCount / 3;
// 				buildRangeInfo.transformOffset = 0;
// 				buildRangeInfos.push_back(buildRangeInfo);
				
// 				//push Material in same order to Reference it
// 				vkutils::Material material{};
// 				material.indexOffset = primitive->firstIndex;
// 				material.vertexOffset = 0;
// 				material.baseColorTexture = primitive->material.baseColorTexture;
// 				material.diffuseTexture = primitive->material.diffuseTexture;
// 				material.emissiveTexture = primitive->material.emissiveTexture;
// 				material.metallicRoughnessTexture = primitive->material.metallicRoughnessTexture;
// 				material.normalTexture = primitive->material.normalTexture;
// 				material.occlusionTexture = primitive->material.occlusionTexture;
// 				material.metallicFactor = primitive->material.metallicFactor;
// 				material.roughnessFactor = primitive->material.roughnessFactor;
// 				material.alphaMode = primitive->material.alphaMode;
// 				material.alphaCutoff = primitive->material.alphaCutoff;
// 				material.baseColorFactor[0] = primitive->material.baseColorFactor.x;
// 				material.baseColorFactor[1] = primitive->material.baseColorFactor.y;
// 				material.baseColorFactor[2] = primitive->material.baseColorFactor.z;
// 				material.baseColorFactor[3] = primitive->material.baseColorFactor.w;
// 				material.emissiveFactor[0] = primitive->material.emissiveFactor.x;
// 				material.emissiveFactor[1] = primitive->material.emissiveFactor.y;
// 				material.emissiveFactor[2] = primitive->material.emissiveFactor.z;
// 				material.emissiveFactor[3] = primitive->material.emissiveFactor.w;
// 				material.emissiveStrength = primitive->material.emissiveStrength;
// 				material.transmissionFactor = primitive->material.transmissionFactor;
// 				material.ior = primitive->material.ior;
// 				_materials.push_back(material);
// 			}
// 		}
// 	}
// 	for (auto& rangeInfo : buildRangeInfos) {
// 		pBuildRangeInfos.push_back(&rangeInfo);
// 	}

// 	// Get size info
// 	vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo;
// 	accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
// 	accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
// 	accelerationStructureBuildGeometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
// 	accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();

// 	auto accelerationStructureBuildSizesInfo = _core._device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, accelerationStructureBuildGeometryInfo, maxPrimitiveCounts);

// 	//Build BLAS Buffer
// 	_bottomLevelASBuffer = vkutils::createBuffer(_core, accelerationStructureBuildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, vma::MemoryUsage::eAutoPreferDevice);

// 	vk::DeviceSize geometryNodesBufferSize = static_cast<uint32_t>(_materials.size()) * sizeof(vkutils::Material);
// 	_materialBuffer =  vkutils::deviceBufferFromData(_core, _materials.data(), geometryNodesBufferSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice);

// 	//Get BLAS Handle
// 	vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo;
// 	accelerationStructureCreateInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
// 	accelerationStructureCreateInfo.buffer = _bottomLevelASBuffer._buffer;
// 	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
// 	_bottomLevelAS = _core._device.createAccelerationStructureKHR(accelerationStructureCreateInfo);

// 	_mainDeletionQueue.push_function([=]() {
// 		_core._allocator.destroyBuffer(_materialBuffer._buffer, _materialBuffer._allocation);
// 		_core._allocator.destroyBuffer(_bottomLevelASBuffer._buffer, _bottomLevelASBuffer._allocation);
// 		_core._device.destroyAccelerationStructureKHR(_bottomLevelAS);
// 	});

// 	// Create ScratchBuffer
// 	vkutils::AllocatedBuffer scratchBuffer = vkutils::createBuffer(_core, accelerationStructureBuildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice, vma::AllocationCreateFlagBits::eDedicatedMemory);
// 	vk::BufferDeviceAddressInfo scratchBufferAdressInfo(scratchBuffer._buffer);
// 	vk::DeviceOrHostAddressConstKHR scratchBufferAddress;
// 	scratchBufferAddress.deviceAddress = _core._device.getBufferAddress(scratchBufferAdressInfo);

// 	accelerationStructureBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
// 	accelerationStructureBuildGeometryInfo.dstAccelerationStructure = _bottomLevelAS;
// 	accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBufferAddress.deviceAddress;

// 	// Create Single-Use CommandBuffer and Build Acceleration Structure on GPU
// 	vk::CommandBufferAllocateInfo allocInfo{};
//     allocInfo.level = vk::CommandBufferLevel::ePrimary;
//     allocInfo.commandPool = _core._cmdPool;
//     allocInfo.commandBufferCount = 1;

//     vk::CommandBuffer commandBuffer = _core._device.allocateCommandBuffers(allocInfo).front();

//     vk::CommandBufferBeginInfo beginInfo{};
//     beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

//     commandBuffer.begin(beginInfo);
// 		commandBuffer.buildAccelerationStructuresKHR(1, &accelerationStructureBuildGeometryInfo, pBuildRangeInfos.data());
//     commandBuffer.end();

//     vk::SubmitInfo submitInfo{};
//     submitInfo.setCommandBuffers(commandBuffer);
//     _core._graphicsQueue.submit(submitInfo);
//     _core._graphicsQueue.waitIdle();
//     _core._device.freeCommandBuffers(_core._cmdPool, commandBuffer);

// 	vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo;
// 	accelerationDeviceAddressInfo.accelerationStructure = _bottomLevelAS;
// 	_bottomLevelDeviceAddress = _core._device.getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);

// 	//delete Scratch Buffer
// 	_core._allocator.destroyBuffer(scratchBuffer._buffer, scratchBuffer._allocation);
// 	_core._allocator.destroyBuffer(transformBuffer._buffer, transformBuffer._allocation);
// }

// void VulkanEngine::init_top_level_acceleration_structure()
// {
// 	vk::TransformMatrixKHR transformMatrix = std::array<std::array<float, 4>, 3>{
// 		1.0f, 0.0f, 0.0f, 0.0f,
// 		0.0f, 1.0f, 0.0f, 0.0f,
// 		0.0f, 0.0f, 1.0f, 0.0f 
// 	};


// 	vk::AccelerationStructureInstanceKHR instance(transformMatrix, 0, 0xFF, 0, vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable, _bottomLevelDeviceAddress);

// 	vkutils::AllocatedBuffer instancesBuffer = vkutils::deviceBufferFromData(_core, &instance, sizeof(vk::AccelerationStructureInstanceKHR), vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);

// 	vk::DeviceOrHostAddressConstKHR instanceDataDeviceAddress;
// 	vk::BufferDeviceAddressInfo instanceBufferAdressInfo(instancesBuffer._buffer);
// 	instanceDataDeviceAddress.deviceAddress = _core._device.getBufferAddress(instanceBufferAdressInfo);

// 	vk::AccelerationStructureGeometryInstancesDataKHR instances(VK_FALSE, instanceDataDeviceAddress);

// 	vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
// 	accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
// 	accelerationStructureGeometry.geometry.instances = instances;

// 	vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo;
// 	accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
// 	accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
// 	accelerationStructureBuildGeometryInfo.setGeometries(accelerationStructureGeometry);

// 	uint32_t primitive_count = 1;

// 	auto accelerationStructureBuildSizesInfo = _core._device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, accelerationStructureBuildGeometryInfo, primitive_count);

// 	_topLevelASBuffer = vkutils::createBuffer(_core, accelerationStructureBuildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, vma::MemoryUsage::eAutoPreferDevice, vma::AllocationCreateFlagBits::eDedicatedMemory);

// 	vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo;
// 	accelerationStructureCreateInfo.buffer = _topLevelASBuffer._buffer;
// 	accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
// 	accelerationStructureCreateInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;

// 	_topLevelAS = _core._device.createAccelerationStructureKHR(accelerationStructureCreateInfo);

// 	_mainDeletionQueue.push_function([=]() {
// 		_core._allocator.destroyBuffer(_topLevelASBuffer._buffer, _topLevelASBuffer._allocation);
// 		_core._device.destroyAccelerationStructureKHR(_topLevelAS);
// 	});

// 	vkutils::AllocatedBuffer scratchBuffer = vkutils::createBuffer(_core, accelerationStructureBuildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice, vma::AllocationCreateFlagBits::eDedicatedMemory);
// 	vk::BufferDeviceAddressInfo scratchBufferAdressInfo(scratchBuffer._buffer);
// 	vk::DeviceOrHostAddressConstKHR scratchBufferAddress;
// 	scratchBufferAddress.deviceAddress = _core._device.getBufferAddress(scratchBufferAdressInfo);

// 	accelerationStructureBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
// 	accelerationStructureBuildGeometryInfo.dstAccelerationStructure = _topLevelAS;
// 	accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBufferAddress.deviceAddress;

// 	vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo;
// 	accelerationStructureBuildRangeInfo.primitiveCount = 1;
// 	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
// 	accelerationStructureBuildRangeInfo.firstVertex = 0;
// 	accelerationStructureBuildRangeInfo.transformOffset = 0;

// 	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

// 	vk::CommandBufferAllocateInfo allocInfo{};
//     allocInfo.level = vk::CommandBufferLevel::ePrimary;
//     allocInfo.commandPool = _core._cmdPool;
//     allocInfo.commandBufferCount = 1;

//     vk::CommandBuffer cmd = _core._device.allocateCommandBuffers(allocInfo).front();

//     vk::CommandBufferBeginInfo beginInfo{};
//     beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

//     cmd.begin(beginInfo);
// 		cmd.buildAccelerationStructuresKHR(1, &accelerationStructureBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());
//     cmd.end();

//     vk::SubmitInfo submitInfo{};
//     submitInfo.setCommandBuffers(cmd);
//     _core._graphicsQueue.submit(submitInfo);
//     _core._graphicsQueue.waitIdle();
//     _core._device.freeCommandBuffers(_core._cmdPool, cmd);

// 	vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo;
// 	accelerationDeviceAddressInfo.accelerationStructure = _topLevelAS;

// 	_topLevelDeviceAddress = _core._device.getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);

// 	_core._allocator.destroyBuffer(scratchBuffer._buffer, scratchBuffer._allocation);
// 	_core._allocator.destroyBuffer(instancesBuffer._buffer, instancesBuffer._allocation);
// }

vkutils::AllocatedImage VulkanEngine::createStorageImage(vk::Format format, uint32_t width, uint32_t height)
{
	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.format = format;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	vkutils::AllocatedImage storageImage = vkutils::createImage(_core, imageInfo, vk::ImageAspectFlagBits::eColor, vma::MemoryUsage::eAutoPreferDevice);

    vk::CommandBuffer cmd = vkutils::getCommandBuffer(_core);
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(beginInfo);
		vkutils::setImageLayout(cmd, storageImage._image, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    cmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(cmd);
    _core._graphicsQueue.submit(submitInfo);
    _core._graphicsQueue.waitIdle();
    _core._device.freeCommandBuffers(_core._cmdPool, cmd);

	return storageImage;
}

void VulkanEngine::createShaderBindingTable() {
	const uint32_t handleSize = _raytracingPipelineProperties.shaderGroupHandleSize;
	const uint32_t handleSizeAligned = vkutils::alignedSize(handleSize, _raytracingPipelineProperties.shaderGroupHandleAlignment);
	const uint32_t groupCount = static_cast<uint32_t>(_shaderGroups.size());
	const uint32_t sbtSize = groupCount * handleSizeAligned;

	auto shaderHandleStorage = _core._device.getRayTracingShaderGroupHandlesKHR<uint8_t>(_raytracerPipeline, (uint32_t) 0, groupCount, (size_t) sbtSize);

	const vk::BufferUsageFlags bufferUsageFlags = vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst;
	const vma::MemoryUsage memoryUsage = vma::MemoryUsage::eAutoPreferDevice;
	_raygenShaderBindingTable = vkutils::createBuffer(_core, handleSize, bufferUsageFlags, memoryUsage);
	_missShaderBindingTable =  vkutils::createBuffer(_core, handleSize * 2, bufferUsageFlags, memoryUsage);
	_hitShaderBindingTable =  vkutils::createBuffer(_core, handleSize, bufferUsageFlags, memoryUsage);

	vkutils::AllocatedBuffer raygenShaderBindingTableStaging = vkutils::createBuffer(_core, handleSize, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
	vkutils::AllocatedBuffer missShaderBindingTableStaging = vkutils::createBuffer(_core, handleSize * 2, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);
	vkutils::AllocatedBuffer hitShaderBindingTableStaging = vkutils::createBuffer(_core, handleSize, vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eAuto, vma::AllocationCreateFlagBits::eHostAccessSequentialWrite);

	// Copy handles
	void* dataRaygen = _core._allocator.mapMemory(raygenShaderBindingTableStaging._allocation);
		memcpy(dataRaygen, shaderHandleStorage.data(), handleSize);
	_core._allocator.unmapMemory(raygenShaderBindingTableStaging._allocation);

	void* dataMiss = _core._allocator.mapMemory(missShaderBindingTableStaging._allocation);
		memcpy(dataMiss, shaderHandleStorage.data() + handleSizeAligned, handleSize  * 2);
	_core._allocator.unmapMemory(missShaderBindingTableStaging._allocation);

	void* dataHit = _core._allocator.mapMemory(hitShaderBindingTableStaging._allocation);
		memcpy(dataHit, shaderHandleStorage.data() + handleSizeAligned * 3, handleSize);
	_core._allocator.unmapMemory(hitShaderBindingTableStaging._allocation);

    vkutils::copyBuffer(_core, raygenShaderBindingTableStaging._buffer, _raygenShaderBindingTable._buffer, handleSize);
	vkutils::copyBuffer(_core, missShaderBindingTableStaging._buffer, _missShaderBindingTable._buffer, handleSize * 2);
	vkutils::copyBuffer(_core, hitShaderBindingTableStaging._buffer, _hitShaderBindingTable._buffer, handleSize);
    _core._allocator.destroyBuffer(raygenShaderBindingTableStaging._buffer, raygenShaderBindingTableStaging._allocation);
	_core._allocator.destroyBuffer(missShaderBindingTableStaging._buffer, missShaderBindingTableStaging._allocation);
	_core._allocator.destroyBuffer(hitShaderBindingTableStaging._buffer, hitShaderBindingTableStaging._allocation);

	_mainDeletionQueue.push_function([=]() {
		_core._allocator.destroyBuffer(_raygenShaderBindingTable._buffer, _raygenShaderBindingTable._allocation);
		_core._allocator.destroyBuffer(_missShaderBindingTable._buffer, _missShaderBindingTable._allocation);
		_core._allocator.destroyBuffer(_hitShaderBindingTable._buffer, _hitShaderBindingTable._allocation);
	});
}

void VulkanEngine::recreateSwapchain() {
	_core._device.waitIdle();
	_framebufferResized = false;
	int w, h;
	SDL_GetWindowSizeInPixels(_core._window, &w, &h);
	_core._windowExtent = vk::Extent2D{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
	_cam.updateSize(_core._windowExtent.width, _core._windowExtent.height);

	_resizeDeletionQueue.flush();
	_gui.destroyFramebuffer();

	init_swapchain();
	init_default_renderpass();
	init_framebuffers();
	_gui.initFrambuffers();
	init_sync_structures();
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_frames[i]._storageImage = createStorageImage(_core._swapchainImageFormat, _core._windowExtent.width, _core._windowExtent.height);

		vk::DescriptorImageInfo resultImageDescriptor;
		resultImageDescriptor.imageView = _frames[i]._storageImage._view;
		resultImageDescriptor.imageLayout = vk::ImageLayout::eGeneral;
		vk::WriteDescriptorSet resultImageWrite;
		resultImageWrite.dstSet = _frames[i]._raytracerDescriptor;
		resultImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
		resultImageWrite.dstBinding = 1;
		resultImageWrite.pImageInfo = &resultImageDescriptor;
		resultImageWrite.descriptorCount = 1;

		std::vector<vk::WriteDescriptorSet> setWrites = {
			resultImageWrite
		};
		_core._device.updateDescriptorSets(setWrites, {});
	}
	_resizeDeletionQueue.push_function([&]() {
		for (int i = 0; i < FRAME_OVERLAP; i++) {
			_core._allocator.destroyImage(_frames[i]._storageImage._image, _frames[i]._storageImage._allocation);
			_core._device.destroyImageView(_frames[i]._storageImage._view);
		}
	});
}