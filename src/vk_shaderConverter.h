#pragma once

#include <vk_types.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <sstream>
#include <fstream>

namespace vkshader
{
	EShLanguage FindLanguage(const vk::ShaderStageFlagBits shader_type);
	void Init();
	void Finalize();
	void InitResources(TBuiltInResource &Resources);
	bool GLSLtoSPV(const vk::ShaderStageFlagBits shader_type, const std::string &filename, std::vector<unsigned int> &spirv);
};