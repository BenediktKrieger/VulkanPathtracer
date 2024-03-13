#include <vk_engine.h>
#include <stb_image.h>
#include <thread>

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

	auto now = std::chrono::steady_clock::now();
	_deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(now - _lastTime).count() / 1000000.0f;
	_lastTime = now;

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

			cmd.bindVertexBuffers(0, 1, &_currentScene->vertexBuffer._buffer, &offset);
			cmd.bindIndexBuffer(_currentScene->indexBuffer._buffer, offset, vk::IndexType::eUint32);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _rasterizerPipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _rasterizerPipelineLayout, 0, get_current_frame()._rasterizerDescriptor, {});
			uint32_t vertexOffset = 0;
			uint32_t indexOffset = 0;
			uint32_t modelMatrixIndex = 0;
			for (auto model : _currentScene->models){
				glm::mat4 modelMatrix = _currentScene->modelMatrices[modelMatrixIndex];
				for (auto node : model->_linearNodes)
				{
					for(auto primitive : node->primitives)
					{
						PushConstants.model = modelMatrix * node->getMatrix();
						cmd.pushConstants(_rasterizerPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(vkutils::PushConstants), &PushConstants);
						cmd.drawIndexed(primitive->indexCount, 1, indexOffset + primitive->firstIndex, vertexOffset, 0);
					}
				}
				vertexOffset += (uint32_t) model->_vertices.size();
				indexOffset += (uint32_t) model->_indices.size();
				modelMatrixIndex += 1;
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

			// raytracing pipeline dispatch
			cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, _raytracerPipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, _raytracerPipelineLayout, 0, 1, &get_current_frame()._raytracerDescriptor, 0, 0);
			cmd.pushConstants(_raytracerPipelineLayout, vk::ShaderStageFlagBits::eRaygenKHR, 0, sizeof(vkutils::PushConstants), &PushConstants);
			cmd.traceRaysKHR(&raygenShaderSbtEntry, &missShaderSbtEntry, &hitShaderSbtEntry, &callableShaderSbtEntry, _core._windowExtent.width, _core._windowExtent.height, 1);

			// compute pipeline dispatch
			ComputeConstants.deltaTime = static_cast<float>(_deltaTime);
			ComputeConstants.width = _core._windowExtent.width;
			ComputeConstants.height = _core._windowExtent.height;
			uint32_t groupCountX = static_cast<uint32_t>(ceil(_core._windowExtent.width/16.f));
			uint32_t groupCountY = static_cast<uint32_t>(ceil(_core._windowExtent.height/16.f));
			uint32_t groupCountZ = 1;

			vk::MemoryBarrier test(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::PipelineStageFlagBits::eComputeShader, {}, test, nullptr, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, _computePipelines[0]);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _computePipelineLayout, 0, 1, &get_current_frame()._computeDescriptor, 0, 0);
			cmd.pushConstants(_computePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(vkutils::ComputeConstants), &ComputeConstants);
			cmd.dispatch(groupCountX, groupCountY, groupCountZ);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, test, nullptr, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, _computePipelines[1]);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _computePipelineLayout, 0, 1, &get_current_frame()._computeDescriptor, 0, 0);
			cmd.pushConstants(_computePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(vkutils::ComputeConstants), &ComputeConstants);
			cmd.dispatch(1, 1, 1);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, test, nullptr, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, _computePipelines[2]);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _computePipelineLayout, 0, 1, &get_current_frame()._computeDescriptor, 0, 0);
			cmd.pushConstants(_computePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(vkutils::ComputeConstants), &ComputeConstants);
			cmd.dispatch(groupCountX, groupCountY, groupCountZ);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eBottomOfPipe, {}, test, nullptr, nullptr);

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
	vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR, vk::PhysicalDeviceAccelerationStructureFeaturesKHR, vk::PhysicalDeviceBufferDeviceAddressFeatures, vk::PhysicalDeviceDescriptorIndexingFeatures, vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT> deviceCreateInfo = {
		createInfo,
		vk::PhysicalDeviceFeatures2().setFeatures(vk::PhysicalDeviceFeatures().setSamplerAnisotropy(true).setShaderInt64(true)),
		vk::PhysicalDeviceRayTracingPipelineFeaturesKHR().setRayTracingPipeline(true),
		vk::PhysicalDeviceAccelerationStructureFeaturesKHR().setAccelerationStructure(true),
		vk::PhysicalDeviceBufferDeviceAddressFeatures().setBufferDeviceAddress(true),
		vk::PhysicalDeviceDescriptorIndexingFeatures().setRuntimeDescriptorArray(true),
		vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT().setShaderBufferFloat32Atomics(true).setShaderBufferFloat32AtomicAdd(true)
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

		vk::DescriptorSetLayoutBinding accumulationImageLayoutBinding;
		accumulationImageLayoutBinding.binding = 1;
		accumulationImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		accumulationImageLayoutBinding.descriptorCount = 1;
		accumulationImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR;

		vk::DescriptorSetLayoutBinding indexBufferBinding;
		indexBufferBinding.binding = 2;
		indexBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		indexBufferBinding.descriptorCount = 1;
		indexBufferBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

		vk::DescriptorSetLayoutBinding vertexBufferBinding;
		vertexBufferBinding.binding = 3;
		vertexBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		vertexBufferBinding.descriptorCount = 1;
		vertexBufferBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

		vk::DescriptorSetLayoutBinding materialBufferBinding;
		materialBufferBinding.binding = 4;
		materialBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		materialBufferBinding.descriptorCount = 1;
		materialBufferBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

		vk::DescriptorSetLayoutBinding lightBufferBinding;
		lightBufferBinding.binding = 5;
		lightBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		lightBufferBinding.descriptorCount = 1;
		lightBufferBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR;

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
        textureLayoutBinding.descriptorCount = static_cast<uint32_t>(_currentScene->textures.size());
        textureLayoutBinding.pImmutableSamplers = nullptr;
        textureLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eAnyHitKHR;

		std::vector<vk::DescriptorSetLayoutBinding> bindings({
			accelerationStructureLayoutBinding,
			accumulationImageLayoutBinding,
			indexBufferBinding,
			vertexBufferBinding,
			materialBufferBinding,
			lightBufferBinding,
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
			hitShader = load_shader_module(vk::ShaderStageFlagBits::eClosestHitKHR, "/MIPS.rchit");
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

	// init compute Pipeline
	{
		vk::DescriptorSetLayoutBinding histogramBufferBinding;
		histogramBufferBinding.binding = 0;
		histogramBufferBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		histogramBufferBinding.descriptorCount = 1;
		histogramBufferBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding resultImageLayoutBinding;
		resultImageLayoutBinding.binding = 1;
		resultImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		resultImageLayoutBinding.descriptorCount = 1;
		resultImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		vk::DescriptorSetLayoutBinding accumulationImageLayoutBinding;
		accumulationImageLayoutBinding.binding = 2;
		accumulationImageLayoutBinding.descriptorType = vk::DescriptorType::eStorageImage;
		accumulationImageLayoutBinding.descriptorCount = 1;
		accumulationImageLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eCompute;

		std::vector<vk::DescriptorSetLayoutBinding> bindings({
			histogramBufferBinding,
			resultImageLayoutBinding,
			accumulationImageLayoutBinding
		});

		vk::DescriptorSetLayoutCreateInfo setinfo;
		setinfo.setBindings(bindings);
		_computeSetLayout = _core._device.createDescriptorSetLayout(setinfo);

		vk::ShaderModule histogramShaderModule = load_shader_module(vk::ShaderStageFlagBits::eCompute, "/luminanceHistogram.comp");
		vk::PipelineShaderStageCreateInfo histogramShaderStageInfo = vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eCompute, histogramShaderModule);

		vk::ShaderModule averageShaderModule = load_shader_module(vk::ShaderStageFlagBits::eCompute, "/luminanceAverage.comp");
		vk::PipelineShaderStageCreateInfo averageShaderStageInfo = vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eCompute, averageShaderModule);

		vk::ShaderModule postprocessingShaderModule = load_shader_module(vk::ShaderStageFlagBits::eCompute, "/postprocessing.comp");
		vk::PipelineShaderStageCreateInfo postprocessingShaderStageInfo = vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eCompute, postprocessingShaderModule);

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, _computeSetLayout);
		vk::PushConstantRange push_constants{vk::ShaderStageFlagBits::eCompute, 0, sizeof(vkutils::ComputeConstants)};
		pipelineLayoutInfo.setPushConstantRanges(push_constants);

		_computePipelineLayout = _core._device.createPipelineLayout(pipelineLayoutInfo);
		
		vk::ComputePipelineCreateInfo pipelineInfos[3];
		pipelineInfos[0] = vk::ComputePipelineCreateInfo({}, histogramShaderStageInfo, _computePipelineLayout);
		pipelineInfos[1] = vk::ComputePipelineCreateInfo({}, averageShaderStageInfo, _computePipelineLayout);
		pipelineInfos[2] = vk::ComputePipelineCreateInfo({}, postprocessingShaderStageInfo, _computePipelineLayout);
		try
		{
			for (size_t i = 0; i < 3; i++)
			{
				vk::Result result;
				std::tie(result, _computePipelines[i]) = _core._device.createComputePipeline({}, pipelineInfos[i]);
				if (result != vk::Result::eSuccess)
				{
					throw std::runtime_error("failed to create compute Pipeline!");
				}
			}
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}

		_core._device.destroyShaderModule(histogramShaderModule);
		_core._device.destroyShaderModule(averageShaderModule);
		_core._device.destroyShaderModule(postprocessingShaderModule);

		_mainDeletionQueue.push_function([=]() {
			for(auto pipeline : _computePipelines){
				_core._device.destroyPipeline(pipeline);
			}
			_core._device.destroyPipelineLayout(_computePipelineLayout);
			_core._device.destroyDescriptorSetLayout(_computeSetLayout);
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
		pool_info.setMaxSets(3);
		pool_info.setPoolSizes(poolSizes);

		_rasterizerDescriptorPool = _core._device.createDescriptorPool(pool_info);

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = _rasterizerDescriptorPool;
			allocInfo.setSetLayouts(_rasterizerSetLayout);

			_frames[i]._rasterizerDescriptor = _core._device.allocateDescriptorSets(allocInfo).front();

			vk::DescriptorBufferInfo binfo;
			binfo.buffer = _currentScene->materialBuffer._buffer;
			binfo.offset = 0;
			binfo.range = _currentScene->materials.size() * sizeof(vkutils::Material);

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
			{ vk::DescriptorType::eStorageBuffer, 1 },
			{ vk::DescriptorType::eStorageBuffer, 1 },
			{ vk::DescriptorType::eStorageBuffer, 1 },
			{ vk::DescriptorType::eCombinedImageSampler, static_cast<uint32_t>(_currentScene->textures.size())}
		};
		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setMaxSets(3);
		pool_info.setPoolSizes(poolSizes);

		_raytracerDescriptorPool = _core._device.createDescriptorPool(pool_info);

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = _raytracerDescriptorPool;
			allocInfo.setSetLayouts(_raytracerSetLayout);

			_frames[i]._raytracerDescriptor = _core._device.allocateDescriptorSets(allocInfo).front();

			vk::WriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo;
			descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
			descriptorAccelerationStructureInfo.pAccelerationStructures = &_currentScene->tlas;
			vk::WriteDescriptorSet accelerationStructureWrite;
			accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
			accelerationStructureWrite.dstSet = _frames[i]._raytracerDescriptor;
			accelerationStructureWrite.dstBinding = 0;
			accelerationStructureWrite.descriptorCount = 1;
			accelerationStructureWrite.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;

			vk::DescriptorImageInfo accumulationImageDescriptor;
			accumulationImageDescriptor.imageView = _accumulationImage._view;
			accumulationImageDescriptor.imageLayout = vk::ImageLayout::eGeneral;
			vk::WriteDescriptorSet accumulationImageWrite;
			accumulationImageWrite.dstSet = _frames[i]._raytracerDescriptor;
			accumulationImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
			accumulationImageWrite.dstBinding = 1;
			accumulationImageWrite.pImageInfo = &accumulationImageDescriptor;
			accumulationImageWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo indexDescriptor;
			indexDescriptor.buffer = _currentScene->indexBuffer._buffer;
			indexDescriptor.offset = 0;
			indexDescriptor.range = _currentScene->indices.size() * sizeof(uint32_t);
			vk::WriteDescriptorSet indexBufferWrite;
			indexBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			indexBufferWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			indexBufferWrite.dstBinding = 2;
			indexBufferWrite.pBufferInfo = &indexDescriptor;
			indexBufferWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo vertexDescriptor;
			vertexDescriptor.buffer = _currentScene->vertexBuffer._buffer;
			vertexDescriptor.offset = 0;
			vertexDescriptor.range = _currentScene->vertices.size() * sizeof(Vertex);
			vk::WriteDescriptorSet vertexBufferWrite;
			vertexBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			vertexBufferWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			vertexBufferWrite.dstBinding = 3;
			vertexBufferWrite.pBufferInfo = &vertexDescriptor;
			vertexBufferWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo uboDescriptor;
			uboDescriptor.buffer = _currentScene->materialBuffer._buffer;
			uboDescriptor.offset = 0;
			uboDescriptor.range = _currentScene->materials.size() * sizeof(vkutils::Material);
			vk::WriteDescriptorSet uniformBufferWrite;
			uniformBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			uniformBufferWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			uniformBufferWrite.dstBinding = 4;
			uniformBufferWrite.pBufferInfo = &uboDescriptor;
			uniformBufferWrite.descriptorCount = 1;

			vk::DescriptorBufferInfo lightsDescriptor;
			lightsDescriptor.buffer = _currentScene->lightBuffer._buffer;
			lightsDescriptor.offset = 0;
			lightsDescriptor.range = _currentScene->lights.size() * sizeof(vkutils::LightProxy);
			vk::WriteDescriptorSet lightBufferWrite;
			lightBufferWrite.dstSet = _frames[i]._raytracerDescriptor;
			lightBufferWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			lightBufferWrite.dstBinding = 5;
			lightBufferWrite.pBufferInfo = &lightsDescriptor;
			lightBufferWrite.descriptorCount = 1;

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
			std::vector<vk::DescriptorImageInfo> imageInfos{};
			for (auto& texture : _currentScene->textures)
			{
				imageInfos.push_back(texture.descriptor);
			}
            textureImageWrite.setImageInfo(imageInfos);

			std::vector<vk::WriteDescriptorSet> setWrites = {
				accelerationStructureWrite,
				accumulationImageWrite,
				indexBufferWrite,
				vertexBufferWrite,
				uniformBufferWrite,
				lightBufferWrite,
				hdrImageWrite,
				settingsUniformBufferWrite,
				textureImageWrite
			};
			_core._device.updateDescriptorSets(setWrites, {});
		}
	}
	//init compute descriptors
	{
		std::vector<vk::DescriptorPoolSize> poolSizes =
		{
			{vk::DescriptorType::eStorageImage, 1 },
			{vk::DescriptorType::eStorageImage, 1 }
		};

		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setMaxSets(3);
		pool_info.setPoolSizes(poolSizes);

		_computeDescriptorPool = _core._device.createDescriptorPool(pool_info);

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			_frames[i]._storageImage = createStorageImage(_core._swapchainImageFormat, _core._windowExtent.width, _core._windowExtent.height);
			vkutils::ImageStats imageInfo;
			imageInfo.average = 0.3f;
			for (auto &&bin : imageInfo.histogram) {
				bin = 0;
			}
			_frames[i]._imageStats = vkutils::deviceBufferFromData(_core, &imageInfo, sizeof(vkutils::ImageStats), vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice);

			vk::DescriptorSetAllocateInfo allocInfo;
			allocInfo.descriptorPool = _computeDescriptorPool;
			allocInfo.setSetLayouts(_computeSetLayout);
			
			_frames[i]._computeDescriptor = _core._device.allocateDescriptorSets(allocInfo).front();
			
			vk::DescriptorBufferInfo histogramDescriptor;
			histogramDescriptor.buffer = _frames[i]._imageStats._buffer;
			histogramDescriptor.offset = 0;
			histogramDescriptor.range = sizeof(vkutils::ImageStats);
			vk::WriteDescriptorSet histogramWrite;
			histogramWrite.dstBinding = 0;
			histogramWrite.dstSet = _frames[i]._computeDescriptor;
			histogramWrite.descriptorCount = 1;
			histogramWrite.descriptorType = vk::DescriptorType::eStorageBuffer;
			histogramWrite.setBufferInfo(histogramDescriptor);

			vk::DescriptorImageInfo resultImageDescriptor;
			resultImageDescriptor.imageView = _frames[i]._storageImage._view;
			resultImageDescriptor.imageLayout = vk::ImageLayout::eGeneral;
			vk::WriteDescriptorSet resultImageWrite;
			resultImageWrite.dstSet = _frames[i]._computeDescriptor;
			resultImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
			resultImageWrite.dstBinding = 1;
			resultImageWrite.pImageInfo = &resultImageDescriptor;
			resultImageWrite.descriptorCount = 1;

			vk::DescriptorImageInfo accumulationImageDescriptor;
			accumulationImageDescriptor.imageView = _accumulationImage._view;
			accumulationImageDescriptor.imageLayout = vk::ImageLayout::eGeneral;
			vk::WriteDescriptorSet accumulationImageWrite;
			accumulationImageWrite.dstSet = _frames[i]._computeDescriptor;
			accumulationImageWrite.descriptorType = vk::DescriptorType::eStorageImage;
			accumulationImageWrite.dstBinding = 2;
			accumulationImageWrite.pImageInfo = &accumulationImageDescriptor;
			accumulationImageWrite.descriptorCount = 1;

			std::vector<vk::WriteDescriptorSet> setWrites = {
				histogramWrite,
				resultImageWrite,
				accumulationImageWrite
			};
			_core._device.updateDescriptorSets(setWrites, {});
		}
	}
	_mainDeletionQueue.push_function([&]() {
		_core._device.destroyDescriptorPool(_raytracerDescriptorPool);
		_core._device.destroyDescriptorPool(_rasterizerDescriptorPool);
		_core._device.destroyDescriptorPool(_computeDescriptorPool);
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			_core._allocator.destroyBuffer(_frames[i]._imageStats._buffer, _frames[i]._imageStats._allocation);
		}
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
	shaderc_shader_kind shaderKind = shaderc_glsl_infer_from_source;
	switch (type) {
		case vk::ShaderStageFlagBits::eVertex:
			shaderKind = shaderc_glsl_vertex_shader;
			break;
		case vk::ShaderStageFlagBits::eFragment:
			shaderKind = shaderc_glsl_fragment_shader;
			break;
		case vk::ShaderStageFlagBits::eCompute:
			shaderKind = shaderc_glsl_compute_shader;
			break;
		case vk::ShaderStageFlagBits::eGeometry:
			shaderKind = shaderc_glsl_geometry_shader;
			break;
		case vk::ShaderStageFlagBits::eRaygenKHR:
			shaderKind = shaderc_raygen_shader;
			break;
		case vk::ShaderStageFlagBits::eAnyHitKHR:
			shaderKind = shaderc_anyhit_shader;
			break;
		case vk::ShaderStageFlagBits::eClosestHitKHR:
			shaderKind = shaderc_closesthit_shader;
			break;
		case vk::ShaderStageFlagBits::eMissKHR:
			shaderKind = shaderc_miss_shader;
			break;
		case vk::ShaderStageFlagBits::eIntersectionKHR:
			shaderKind = shaderc_intersection_shader;
			break;
		case vk::ShaderStageFlagBits::eCallableKHR:
			shaderKind = shaderc_callable_shader;
			break;
	}

    std::ifstream input_file(SHADER_PATH + filePath);
    if (!input_file.is_open()) {
        std::cerr << "Could not open the file - '" << SHADER_PATH + filePath << "'" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string shaderCodeGlsl = std::string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());

	auto preprocessed = vkshader::preprocess_shader("shader_src", shaderKind, shaderCodeGlsl, shaderc_optimization_level_performance);

    std::cout << "Compiling shader  " << SHADER_PATH + filePath << "" << std::endl;
    auto spirv = vkshader::compile_file("shader_src", shaderKind, preprocessed.c_str(), shaderc_optimization_level_performance);

	vk::ShaderModuleCreateInfo createInfo({}, spirv);
	vk::ShaderModule shaderModule;
	try
	{
		shaderModule = _core._device.createShaderModule(createInfo);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}
	return shaderModule;
}

void VulkanEngine::load_models()
{
	auto start_all = std::chrono::high_resolution_clock::now();

	// load bistro optimized
	Scene* scene1 = new Scene(_core);
	scene1->add(ASSET_PATH"/models/sponza.glb");
	// scene1->add(ASSET_PATH"/models/spheres.glb");
	// scene1->add(ASSET_PATH"/models/sphere_plastic.glb", glm::scale(glm::translate(glm::mat4(1.0), glm::vec3(-0.5, -0.5, 0.5)), glm::vec3(0.25))); 
	// scene1->add(ASSET_PATH"/models/sphere_plastic.glb", glm::scale(glm::translate(glm::mat4(1.0), glm::vec3(0.5, 0.25, 0.5)), glm::vec3(0.25))); 
	// scene1->add(ASSET_PATH"/models/dragon.glb");
	scene1->build();
	scene1->buildAccelerationStructure();
	_currentScene = scene1;
	_scenes.push_back(scene1);
	
	auto elapsed_all = std::chrono::high_resolution_clock::now() - start_all;
	long long microseconds_all = std::chrono::duration_cast<std::chrono::microseconds>(elapsed_all).count();
	std::cout << "scene1 loading time: " << microseconds_all / 1e6 << "s" << std::endl;

	// start_all = std::chrono::high_resolution_clock::now();
	// std::thread th { [=]() {
	// 	Scene* scene2 = new Scene(_core);
	// 	scene2->add(ASSET_PATH"/models/bistro_new_1.glb");
	// 	_scenes.push_back(scene2);
	// 	auto elapsed_all = std::chrono::high_resolution_clock::now() - start_all;
	// 	auto microseconds_all = std::chrono::duration_cast<std::chrono::microseconds>(elapsed_all).count();
	// 	std::cout << "scene2 loading time: " << microseconds_all / 1e6 << "s" << std::endl;
    // }};
	// th.detach();

	_mainDeletionQueue.push_function([&]() {
		for(auto& scene : _scenes){
			scene->destroy();
			delete scene;
		}
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
	if((_settingsUBO.accumulate > 0) != _gui.settings.accumulate || _settingsUBO.samples != _gui.settings.samples || _settingsUBO.reflection_recursion != _gui.settings.reflection_recursion || _settingsUBO.refraction_recursion != _gui.settings.refraction_recursion || _gui.settings.fov != _fov || _settingsUBO.ambient_multiplier != _gui.settings.ambient_multiplier){
		_cam.changed = true;
	}
	// shwo cam pos
	glm::vec3 cam_pos = _cam.getPosition();
	glm::vec3 cam_dir = _cam.getDirection();
	_gui.settings.cam_pos = cam_pos;
	_gui.settings.cam_dir = cam_dir;
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
		vk::WriteDescriptorSet resultImageWriteCompute;
		resultImageWriteCompute.dstSet = _frames[i]._computeDescriptor;
		resultImageWriteCompute.descriptorType = vk::DescriptorType::eStorageImage;
		resultImageWriteCompute.dstBinding = 1;
		resultImageWriteCompute.pImageInfo = &resultImageDescriptor;
		resultImageWriteCompute.descriptorCount = 1;

		std::vector<vk::WriteDescriptorSet> setWrites = {
			resultImageWriteCompute
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