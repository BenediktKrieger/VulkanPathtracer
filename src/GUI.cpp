#include <GUI.h>

vk::GUI::GUI():_core(){
    settings.renderer = 0;
    settings.cam_pos[0] = 0.f;
    settings.cam_pos[1] = 0.f;
    settings.cam_pos[2] = 0.f;
    settings.cam_dir[0] = 0.f;
    settings.cam_dir[1] = 0.f;
    settings.cam_dir[2] = 0.f;
    settings.fov = 75.f;
    settings.cam_mode = 0;
    settings.speed = 1.0;
    settings.auto_exposure = true;
    settings.exposure = 1.0;
    settings.accumulate = true;
    settings.min_samples = 1;
    settings.limit_samples = false;
    settings.max_samples = 1000;
    settings.reflection_recursion = 6;
    settings.refraction_recursion = 8;
    settings.ambient_multiplier = 1.f;
    settings.mips = true;
    settings.mips_sensitivity = 0.01f;
}

vk::GUI::GUI(vk::Core *core)
{
    _core = core;
    settings.renderer = 0;
    settings.cam_pos[0] = 0.f;
    settings.cam_pos[1] = 0.f;
    settings.cam_pos[2] = 0.f;
    settings.cam_dir[0] = 0.f;
    settings.cam_dir[1] = 0.f;
    settings.cam_dir[2] = 0.f;
    settings.fov = 75.f;
    settings.cam_mode = 0;
    settings.speed = 1.0;
    settings.auto_exposure = true;
    settings.exposure = 1.0;
    settings.accumulate = true;
    settings.min_samples = 1;
    settings.limit_samples = false;
    settings.max_samples = 1000;
    settings.reflection_recursion = 6;
    settings.refraction_recursion = 8;
    settings.ambient_multiplier = 1.f;
    settings.mips = true;
    settings.mips_sensitivity = 0.01f;
    std::vector<vk::DescriptorPoolSize> poolSizes =
    {
        { vk::DescriptorType::eSampler, 1000 },
		{ vk::DescriptorType::eCombinedImageSampler, 1000 },
		{ vk::DescriptorType::eSampledImage, 1000 },
		{ vk::DescriptorType::eStorageImage, 1000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		{ vk::DescriptorType::eUniformBuffer, 1000 },
		{ vk::DescriptorType::eStorageBuffer, 1000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		{ vk::DescriptorType::eInputAttachment, 1000 }
    };

	vk::DescriptorPoolCreateInfo pool_info(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        1000,
        poolSizes
    );
    try
	{
		_pool = _core->_device.createDescriptorPool(pool_info);
	}
	catch (std::exception &e)
	{
		std::cerr << "Exception Thrown: " << e.what();
	}

	ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    io.FontDefault = io.Fonts->AddFontFromFileTTF(ASSET_PATH"/fonts/Roboto-Regular.ttf", 16.f);
  
    ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForVulkan(_core->_window);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _core->_instance;
	init_info.PhysicalDevice = _core->_chosenGPU;
	init_info.Device = _core->_device;
	init_info.Queue = _core->_graphicsQueue;
	init_info.DescriptorPool = (VkDescriptorPool) _pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
    init_info.ColorAttachmentFormat = (VkFormat) _core->_swapchainImageFormat;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    initRenderPass();
    initFrambuffers();

	ImGui_ImplVulkan_Init(&init_info, _renderPass);
	ImGui_ImplVulkan_CreateFontsTexture();
}

void vk::GUI::initRenderPass()
{
    vk::AttachmentDescription attachment(
        {}, 
        _core->_swapchainImageFormat, 
        vk::SampleCountFlagBits::e1, 
        vk::AttachmentLoadOp::eLoad, 
        vk::AttachmentStoreOp::eStore, 
        vk::AttachmentLoadOp::eDontCare, 
        vk::AttachmentStoreOp::eDontCare, 
        vk::ImageLayout::eColorAttachmentOptimal, 
        vk::ImageLayout::ePresentSrcKHR
    );
    vk::AttachmentReference attachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics, {}, {}, 1, &attachmentRef, {}, {}, {}, {});

    vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL, 
        0, 
        vk::PipelineStageFlagBits::eColorAttachmentOutput, 
        vk::PipelineStageFlagBits::eColorAttachmentOutput, 
        vk::AccessFlagBits::eNoneKHR, 
        vk::AccessFlagBits::eColorAttachmentWrite);

    vk::RenderPassCreateInfo renderPassInfo(
        {},
        attachment,
        subpass,
        dependency
    );
    try{
        _renderPass = _core->_device.createRenderPass(renderPassInfo);
    }catch(std::exception& e) {
        std::cerr << "Exception Thrown: " << e.what();
    }
}

void vk::GUI::initFrambuffers()
{
	vk::FramebufferCreateInfo createInfo = vkinit::framebuffer_create_info(_renderPass, _core->_windowExtent);

	const uint32_t swapchain_imagecount = (uint32_t)_core->_swapchainImages.size();
	_framebuffers.resize(swapchain_imagecount);

	for (unsigned int i = 0; i < swapchain_imagecount; i++)
	{
		vk::ImageView  colorAttachment = _core->_swapchainImageViews[i];
		createInfo.setAttachments(colorAttachment);
		try
		{
			_framebuffers[i] = _core->_device.createFramebuffer(createInfo);
		}
		catch (std::exception &e)
		{
			std::cerr << "Exception Thrown: " << e.what();
		}
	}
}

void vk::GUI::render(vk::CommandBuffer &cmd, uint32_t index)
{
    vk::RenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _core->_windowExtent, _framebuffers[index]);
    cmd.beginRenderPass(rpInfo, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRenderPass();
}

void vk::GUI::update()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    static bool p_dockSpaceOpen = true;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", &p_dockSpaceOpen, window_flags);
        ImGui::PopStyleVar(3);
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        ImGui::Begin("Metrics", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse); 
            static float values[120] = {};
            static int values_offset = 0;
            static double refresh_time = ImGui::GetTime();
            while (refresh_time < ImGui::GetTime())
            {
                static float phase = 0.0f;
                values[values_offset] = io.DeltaTime * 1000.f;
                values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
                phase += 0.10f * values_offset;
                refresh_time += 1.0f / 60.0f;
            }
            float average = 0.0f;
            for (int n = 0; n < IM_ARRAYSIZE(values); n++)
                average += values[n];
            average /= (float)IM_ARRAYSIZE(values);
            char overlay[32];
            sprintf(overlay, "%.03f ms", average);
            char plotlabel[32];
            uint32_t averagefps = (uint32_t) floor(1000.f / average);
            sprintf(plotlabel, "FPS: %u", averagefps);
            ImGui::PlotLines(plotlabel, values, IM_ARRAYSIZE(values), values_offset, overlay, 0.0f, 16.666f, ImVec2(0, 100.0f));
        ImGui::End();

        ImGui::Begin("Settings", NULL);
            ImGui::SeparatorText("Renderer");
            ImGui::RadioButton("Rasterizer", &settings.renderer, 0); ImGui::SameLine();
            ImGui::RadioButton("Pathtracer", &settings.renderer, 1);
            ImGui::SeparatorText("Camera Setup");
            ImGui::InputFloat3("Position", glm::value_ptr(settings.cam_pos));
            ImGui::InputFloat3("Direction", glm::value_ptr(settings.cam_dir));
            ImGui::SliderFloat("Field Of View", &settings.fov, 10.f, 150.f, "%.1f degrees");
            ImGui::SliderFloat("Movement Speed", &settings.speed, 0.01f, 10.f, "%.2f");
            ImGui::Checkbox("Auto Exposure", &settings.auto_exposure);
            if (!settings.auto_exposure)
            {
                ImGui::SliderFloat("Exposure", &settings.exposure, 0.01f, 10.f, "%.2f");
            }
            ImGui::SeparatorText("Pathtracer Setup");
            ImGui::Checkbox("Accumulate Image", &settings.accumulate);
            ImGui::SliderInt("Minimum Samples Per Pixel", reinterpret_cast<int *>(&settings.min_samples), 1, 100);
            ImGui::Checkbox("Limit Samples", &settings.limit_samples);
            if(settings.limit_samples)
            {
                const ImU32 u32_one = 1, u32_tenthousand = 10000;
                ImGui::DragScalar("Maximum Samples Per Pixel", ImGuiDataType_U32, &settings.max_samples, 10.f, &u32_one, &u32_tenthousand, "%u");
            }
            ImGui::SliderInt("Bounce Limit (Reflection)", reinterpret_cast<int *>(&settings.reflection_recursion), 1, 32);
            ImGui::SliderInt("Bounce Limit (Refraction)", reinterpret_cast<int *>(&settings.refraction_recursion), 1, 32);
            ImGui::Checkbox("Multiple Importance Sampling", &settings.mips);
            if(settings.mips)
            {
                ImGui::SliderFloat("Radiance Sensititvity", &settings.mips_sensitivity, 0.01f, 1.f, "%.2f");
            }
            ImGui::SeparatorText("Environment Map");
            ImGui::SliderFloat("Skylight Multiplier", &settings.ambient_multiplier, 0.f, 20.f, "%.1f");
        ImGui::End();

        //ImGui::ShowDemoWindow();
    ImGui::End();
    
    ImGui::Render(); 
    additionalWindows();
}

void vk::GUI::additionalWindows()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void vk::GUI::handleInput(const SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);
}

void vk::GUI::destroyFramebuffer()
{
    for (unsigned int i = 0; i < _core->_swapchainImages.size(); i++)
	{
        _core->_device.destroyFramebuffer(_framebuffers[i]);
    }
}

void vk::GUI::destroy()
{
    ImGui_ImplVulkan_Shutdown();
    _core->_device.destroyDescriptorPool(_pool);
    destroyFramebuffer();
    _core->_device.destroyRenderPass(_renderPass);
    ImGui::DestroyContext();
}

vk::GUI::~GUI()
{
}
