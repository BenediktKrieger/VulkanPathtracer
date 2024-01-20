#pragma once

#include <Core.h>
#include <vk_model.h>

class Scene {
public:
    vk::AccelerationStructureKHR tlas;
    vkutils::AllocatedBuffer vertexBuffer;
    vkutils::AllocatedBuffer indexBuffer;
    vkutils::AllocatedBuffer materialBuffer;
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};
    std::vector<vkutils::Material> materials{};
    std::vector<Texture> textures{};
    std::vector<Model *> models{};
    std::vector<glm::mat4> modelMatrices{};
    
    Scene();
    Scene(vk::Core &core);
    void add(std::string path, glm::mat4 transform = glm::mat4(1.0));
    void build();
    void buildAccelerationStructure();
    void destroy();
private:
    vk::Core* core;
    vk::Sampler sampler;
    bool _isBuilded;
    
    std::vector<vkutils::AllocatedBuffer> blasBuffer{};
    std::vector<vk::DeviceAddress> blasAddress{};
    std::vector<vk::AccelerationStructureKHR> blas{};
    vkutils::AllocatedBuffer tlasBuffer;
    vk::DeviceAddress tlasAddress;
    
    std::vector<vk::TransformMatrixKHR> tlasTransforms{};
    void createEmptyTexture();
};