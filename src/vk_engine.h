#pragma once

#include <vk_types.h>
#include <vector>
#include <vk_utils.h>
#include <vk_model.h>
#include <vk_shaderConverter.h>
#include <Camera.h>
#include <Core.h>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_sdl3.h>

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine
{
public:
	vk::Core _core;
	bool _isInitialized{false};
	bool _framebufferResized{false};
	uint32_t _frameNumber{0};
	int _selectedShader{0};

	vkutils::PushConstants PushConstants;
	Camera cam;

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
	
	Model _model;

	vk::Format _depthFormat;
	vkutils::AllocatedImage _depthImage;

	vk::DescriptorPool _rasterizerDescriptorPool;
	vk::DescriptorPool _raytracerDescriptorPool;
	vk::DescriptorSetLayout _rasterizerSetLayout;
	vk::DescriptorSetLayout _raytracerSetLayout;

	vkutils::AllocatedBuffer _bottomLevelASBuffer;
	vk::DeviceAddress _bottomLevelDeviceAddress;
	vk::AccelerationStructureKHR _bottomLevelAS;
	
	vkutils::AllocatedBuffer _topLevelASBuffer;
	vk::DeviceAddress _topLevelDeviceAddress;
	vk::AccelerationStructureKHR _topLevelAS;

	vkutils::AllocatedImage _accumulationImage;
	
	std::vector<vkutils::GeometryNode> _materials;
	vkutils::AllocatedBuffer _materialBuffer;

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

	void init_imgui();

	void init_accumulation_image();

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
