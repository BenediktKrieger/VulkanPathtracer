#pragma once

#include <Core.h>
#include <vk_model.h>

class Scene {
public:
    Scene();
    Scene(vk::Core &core);
    void add(std::string path);
    void buildAccelerationStructure();
    std::vector<vk::DescriptorImageInfo> getTextureDescriptors();
    void destroy();
    vk::AccelerationStructureKHR tlas;
    vkutils::AllocatedBuffer vertexBuffer;
    vkutils::AllocatedBuffer indexBuffer;
    vkutils::AllocatedBuffer materialBuffer;
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};
    std::vector<vkutils::Material> materials{};
    std::vector<Texture> textures{};
    std::vector<Model *> models{};
private:
    vk::Core* core;
    vk::Sampler sampler;
    std::vector<vkutils::AllocatedBuffer> blasBuffer{};
    std::vector<vk::DeviceAddress> blasAddress{};
    std::vector<vk::AccelerationStructureKHR> blas{};

    vkutils::AllocatedBuffer tlasBuffer;
    vk::DeviceAddress tlasAddress;
};