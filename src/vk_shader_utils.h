#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <shaderc/shaderc.hpp>

namespace vkshader
{
    std::string preprocess_shader(const std::string& source_name, shaderc_shader_kind kind, const std::string& source, shaderc_optimization_level optimization = shaderc_optimization_level_zero);
    std::vector<uint32_t> compile_file(const std::string& source_name, shaderc_shader_kind kind,  const std::string& source, shaderc_optimization_level optimization = shaderc_optimization_level_zero);
};