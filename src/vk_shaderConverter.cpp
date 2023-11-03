#include "vk_shaderConverter.h"

void vkshader::Init() {
	glslang::InitializeProcess();
}

void vkshader::Finalize() {
    glslang::FinalizeProcess();
}

EShLanguage vkshader::FindLanguage(const vk::ShaderStageFlagBits shader_type) {
    switch (shader_type) {
    case vk::ShaderStageFlagBits::eVertex:
        return EShLangVertex;
    case vk::ShaderStageFlagBits::eTessellationControl:
        return EShLangTessControl;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        return EShLangTessEvaluation;
    case vk::ShaderStageFlagBits::eGeometry:
        return EShLangGeometry;
    case vk::ShaderStageFlagBits::eFragment:
        return EShLangFragment;
    case vk::ShaderStageFlagBits::eCompute:
        return EShLangCompute;
    default:
        return EShLangVertex;
    }
}

bool vkshader::GLSLtoSPV(const vk::ShaderStageFlagBits shader_type, const std::string &filename, std::vector<unsigned int> &spirv) {
    std::ifstream input_file(SHADER_PATH + filename);
    if (!input_file.is_open()) {
        std::cerr << "Could not open the file - '" << filename << "'" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string shaderCodeGlsl =  std::string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
    const char *pshader = shaderCodeGlsl.c_str();
    EShLanguage stage = vkshader::FindLanguage(shader_type);
    glslang::TShader shader(stage);
    glslang::TProgram program;
    const char *shaderStrings[1];
    TBuiltInResource Resources = {};

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

    shaderStrings[0] = pshader;
    shader.setStrings(shaderStrings, 1);

    if (!shader.parse(&Resources, 100, false, messages)) {
        puts(shader.getInfoLog());
        puts(shader.getInfoDebugLog());
        return false; 
    }

    program.addShader(&shader);

    if (!program.link(messages)) {
        puts(shader.getInfoLog());
        puts(shader.getInfoDebugLog());
        fflush(stdout);
        return false;
    }

    glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);
    return true;
}