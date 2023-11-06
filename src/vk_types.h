#pragma once
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VK_ENABLE_BETA_EXTENSIONS
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define TINYGLTF_USE_CPP14
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.hpp>
#include <vk_shaderConverter.h>
#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>