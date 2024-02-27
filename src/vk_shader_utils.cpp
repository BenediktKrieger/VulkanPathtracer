#include <vk_shader_utils.h>


// Returns GLSL shader source text after preprocessing.
std::string vkshader::preprocess_shader(const std::string& source_name, shaderc_shader_kind kind, const std::string& source, shaderc_optimization_level optimization) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
  options.SetTargetSpirv(shaderc_spirv_version_1_4);
  options.SetOptimizationLevel(optimization);
  options.SetGenerateDebugInfo();

  shaderc::PreprocessedSourceCompilationResult result = compiler.PreprocessGlsl(source, kind, source_name.c_str(), options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    std::cerr << result.GetErrorMessage();
    return "";
  }

  return {result.cbegin(), result.cend()};
}



// Compiles a shader to a SPIR-V binary. Returns the binary as
// a vector of 32-bit words.
std::vector<uint32_t> vkshader::compile_file(const std::string& source_name, shaderc_shader_kind kind, const std::string& source, shaderc_optimization_level optimization) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
  options.SetTargetSpirv(shaderc_spirv_version_1_4);
  options.SetOptimizationLevel(optimization);
  options.SetGenerateDebugInfo();
  shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, kind, source_name.c_str(), options);

  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    std::cerr << module.GetErrorMessage();
    return std::vector<uint32_t>();
  }

  return {module.cbegin(), module.cend()};
} 