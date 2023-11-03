#include <vk_types.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <iostream>
#include <sstream>
#include <fstream>

namespace vkshader
{
	EShLanguage FindLanguage(const vk::ShaderStageFlagBits shader_type);
	void Init();
	void Finalize();
	bool GLSLtoSPV(const vk::ShaderStageFlagBits shader_type, const std::string &filename, std::vector<unsigned int> &spirv);
};