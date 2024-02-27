#pragma once

#include <vk_shader_utils.h>
#include <vk_utils.h>
#include <vk_scene.h>
#include <Camera.h>
#include <GUI.h>

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine
{
public:
	vk::Core _core;
	vk::GUI _gui;
	bool _isInitialized{false};
	bool _framebufferResized{false};
	uint32_t _frameNumber{0};
	int _selectedShader{0};

	vkutils::PushConstants PushConstants;
	Camera _cam;
	float _fov;

	vkutils::FrameData _frames[FRAME_OVERLAP];
	vk::RenderPass _renderPass;
	vk::PipelineLayout _rasterizerPipelineLayout;
	vk::PipelineLayout _raytracerPipelineLayout;
	vkutils::AllocatedBuffer _raygenShaderBindingTable;
	vkutils::AllocatedBuffer _missShaderBindingTable;
	vkutils::AllocatedBuffer _hitShaderBindingTable;

	vk::PhysicalDeviceRayTracingPipelinePropertiesKHR _raytracingPipelineProperties;

	vk::Pipeline _rasterizerPipeline;
	vk::Pipeline _raytracerPipeline;
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> _shaderGroups;
	
	Scene* _currentScene;
	std::vector<Scene*> _scenes;

	vkutils::Shadersettings _settingsUBO;
	vkutils::AllocatedBuffer _settingsBuffer;

	vkutils::AllocatedImage _depthImage;
	vk::Format _depthFormat;

	vkutils::AllocatedImage _hdrMap;

	vk::Sampler _envMapSampler;

	vk::DescriptorPool _rasterizerDescriptorPool;
	vk::DescriptorPool _raytracerDescriptorPool;
	vk::DescriptorSetLayout _rasterizerSetLayout;
	vk::DescriptorSetLayout _raytracerSetLayout;

	vkutils::AllocatedImage _accumulationImage;

	vkutils::DeletionQueue _resizeDeletionQueue;
	vkutils::DeletionQueue _mainDeletionQueue;

	void init();
	void cleanup();
	void draw();
	void run();

	vkutils::FrameData& get_current_frame();

private:
	void init_vulkan();

	void init_swapchain();

	void init_default_renderpass();

	void init_framebuffers();

	void init_commands();

	void init_sync_structures();

	void init_gui();

	void init_accumulation_image();

	void init_hdr_map();

	void init_ubo();

	void init_descriptors();

	void init_pipelines();

	void load_models();

	void upload_model(Model& model);

	void init_bottom_level_acceleration_structure(Model &model);

	void init_top_level_acceleration_structure();

	vkutils::AllocatedImage createStorageImage(vk::Format format, uint32_t width, uint32_t height);

	void createShaderBindingTable();

	void updateBuffers();

	void recreateSwapchain();

	vk::ShaderModule load_shader_module(vk::ShaderStageFlagBits type, std::string filePath);
};
