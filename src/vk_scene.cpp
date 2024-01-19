#include <vk_scene.h>

Scene::Scene(): core(){}

Scene::Scene(vk::Core& core): core(&core){}

void Scene::add(std::string path)
{
    Model *model = new Model(*core);
	model->load_from_glb(path.c_str());
    
    models.push_back(model);
    vertices.insert(std::end(vertices), std::begin(model->_vertices), std::end(model->_vertices));
    indices.insert(std::end(indices), std::begin(model->_indices), std::end(model->_indices));
    textures.insert(std::end(textures), std::begin(model->_textures), std::end(model->_textures));
}

std::vector<vk::DescriptorImageInfo> Scene::getTextureDescriptors()
{
	std::vector<vk::DescriptorImageInfo> descriptorImageInfos(textures.size());
    for (size_t i = 0; i < textures.size(); i++)
    {
        descriptorImageInfos[i] = textures[i].descriptor;
    }
    return descriptorImageInfos;
}

void Scene::buildAccelerationStructure()
{
    {
        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eNearest;
        samplerInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eMirroredRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eMirroredRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eMirroredRepeat;
        samplerInfo.compareOp = vk::CompareOp::eNever;
        samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
        samplerInfo.maxLod = 1;
        samplerInfo.maxAnisotropy = 8.0f;
        samplerInfo.anisotropyEnable = true;
        sampler = core->_device.createSampler(samplerInfo);

        Texture emptyTexture;
        unsigned char* buffer = new unsigned char[4];
        memset(buffer, 0, 4);

        vk::ImageCreateInfo imageCreateInfo;
        imageCreateInfo.imageType = vk::ImageType::e2D;
        imageCreateInfo.format = vk::Format::eR8G8B8A8Unorm;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
        imageCreateInfo.extent = vk::Extent3D{ 1, 1, 1 };
        imageCreateInfo.usage = vk::ImageUsageFlagBits::eSampled;
        emptyTexture.image = vkutils::imageFromData(*core, buffer, imageCreateInfo, vk::ImageAspectFlagBits::eColor, vma::MemoryUsage::eAutoPreferDevice);
        emptyTexture.index = static_cast<uint32_t>(textures.size());
        vk::DescriptorImageInfo imageInfo;
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        imageInfo.imageView = emptyTexture.image._view;
        imageInfo.sampler = sampler;
        emptyTexture.descriptor = imageInfo;
        textures.push_back(emptyTexture);
    }
    {
        vk::DeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);
        vk::DeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
        vertexBuffer = vkutils::deviceBufferFromData(*core, (void*) vertices.data(), vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice);
        indexBuffer = vkutils::deviceBufferFromData(*core, (void*) indices.data(), indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice);
    }
    std::vector<uint32_t> materialOffsets;
    //build blas
    {
        uint32_t modelIndexOffset = 0;
        uint32_t modelVertexOffset = 0;
        uint32_t modelTextureOffset = 0;
        for(auto& model : models){
            materialOffsets.push_back(materials.size());
            vk::DeviceSize vertexBufferSize = model->_vertices.size() * sizeof(Vertex);
            vk::DeviceSize indexBufferSize = model->_indices.size() * sizeof(uint32_t);
            std::cout << "Vertex Count: " << model->_vertices.size() << std::endl;
            model->_vertexBuffer = vkutils::deviceBufferFromData(*core, (void*) model->_vertices.data(), vertexBufferSize, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);
            model->_indexBuffer = vkutils::deviceBufferFromData(*core, (void*) model->_indices.data(), indexBufferSize, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);

            std::vector<vk::TransformMatrixKHR> transformMatrices;
            for (auto node : model->_linearNodes) {
                for (auto primitive : node->primitives) {
                    if (primitive->indexCount > 0) {
                        vk::TransformMatrixKHR transformMatrix{};
                        auto m = glm::mat3x4(glm::transpose(node->getMatrix()));
                        memcpy(&transformMatrix, (void*)&m, sizeof(glm::mat3x4));
                        transformMatrices.push_back(transformMatrix);
                    }
                }
            }

            vk::DeviceSize transformBufferSize = transformMatrices.size() * sizeof(vk::TransformMatrixKHR);
            vkutils::AllocatedBuffer transformBuffer = vkutils::deviceBufferFromData(*core, transformMatrices.data(), transformBufferSize, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);

            // Build BLAS
            std::vector<uint32_t> maxPrimitiveCounts{};
            std::vector<vk::AccelerationStructureGeometryKHR> geometries{};
            std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangeInfos{};
            std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos{};
            vk::BufferDeviceAddressInfo vertexBufferAdressInfo(model->_vertexBuffer._buffer);
            vk::BufferDeviceAddressInfo indexBufferAdressInfo(model->_indexBuffer._buffer);
            vk::BufferDeviceAddressInfo transformBufferAdressInfo(transformBuffer._buffer);
            for (auto node : model->_linearNodes) {
                for (auto primitive : node->primitives) {
                    if (primitive->indexCount > 0) {
                        //Device Addresses
                        vk::DeviceOrHostAddressConstKHR vertexBufferDeviceAddress;
                        vk::DeviceOrHostAddressConstKHR indexBufferDeviceAddress;
                        vk::DeviceOrHostAddressConstKHR transformBufferDeviceAddress;
                        vertexBufferDeviceAddress.deviceAddress = core->_device.getBufferAddress(vertexBufferAdressInfo);
                        indexBufferDeviceAddress.deviceAddress = core->_device.getBufferAddress(indexBufferAdressInfo) + primitive->firstIndex * sizeof(uint32_t);
                        transformBufferDeviceAddress.deviceAddress = core->_device.getBufferAddress(transformBufferAdressInfo) + static_cast<uint32_t>(geometries.size()) * sizeof(vk::TransformMatrixKHR);

                        //Create Geometry for every gltf primitive (node)
                        vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
                        triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
                        triangles.maxVertex = static_cast<uint32_t>(model->_vertices.size());
                        triangles.vertexStride = sizeof(Vertex);
                        triangles.indexType = vk::IndexType::eUint32;
                        triangles.vertexData = vertexBufferDeviceAddress;
                        triangles.indexData = indexBufferDeviceAddress;
                        triangles.transformData = transformBufferDeviceAddress;

                        vk::AccelerationStructureGeometryKHR geometry;
                        geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
                        geometry.geometry.triangles = triangles;
                        if(primitive->material.alphaMode != Material::ALPHAMODE_OPAQUE)
                        {
                            geometry.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
                        }
                        else
                        {
                            geometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
                        }

                        geometries.push_back(geometry);
                        maxPrimitiveCounts.push_back(primitive->indexCount / 3);
                        vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo;
                        buildRangeInfo.firstVertex = 0;
                        buildRangeInfo.primitiveOffset = 0;
                        buildRangeInfo.primitiveCount = primitive->indexCount / 3;
                        buildRangeInfo.transformOffset = 0;
                        buildRangeInfos.push_back(buildRangeInfo);
                        
                        //push Material in same order to Reference it
                        vkutils::Material material{};
                        material.indexOffset = modelIndexOffset + primitive->firstIndex;
                        material.vertexOffset = modelVertexOffset;
                        material.baseColorTexture = primitive->material.baseColorTexture > -1 ? primitive->material.baseColorTexture : -1;
                        material.diffuseTexture = primitive->material.diffuseTexture > -1 ? primitive->material.diffuseTexture : -1;
                        material.emissiveTexture = primitive->material.emissiveTexture > -1 ? primitive->material.emissiveTexture : -1;
                        material.metallicRoughnessTexture = primitive->material.metallicRoughnessTexture > -1 ? primitive->material.metallicRoughnessTexture : -1;
                        material.normalTexture = primitive->material.normalTexture > -1 ? primitive->material.normalTexture : -1;
                        material.occlusionTexture = primitive->material.occlusionTexture > -1 ? primitive->material.occlusionTexture : -1;
                        material.metallicFactor = primitive->material.metallicFactor;
                        material.roughnessFactor = primitive->material.roughnessFactor;
                        material.alphaMode = primitive->material.alphaMode;
                        material.alphaCutoff = primitive->material.alphaCutoff;
                        material.baseColorFactor[0] = primitive->material.baseColorFactor.x;
                        material.baseColorFactor[1] = primitive->material.baseColorFactor.y;
                        material.baseColorFactor[2] = primitive->material.baseColorFactor.z;
                        material.baseColorFactor[3] = primitive->material.baseColorFactor.w;
                        material.emissiveFactor[0] = primitive->material.emissiveFactor.x;
                        material.emissiveFactor[1] = primitive->material.emissiveFactor.y;
                        material.emissiveFactor[2] = primitive->material.emissiveFactor.z;
                        material.emissiveFactor[3] = primitive->material.emissiveFactor.w;
                        material.emissiveStrength = primitive->material.emissiveStrength;
                        material.transmissionFactor = primitive->material.transmissionFactor;
                        material.ior = primitive->material.ior;
                        materials.push_back(material);
                    }
                }
            }
            for (auto& rangeInfo : buildRangeInfos) {
                pBuildRangeInfos.push_back(&rangeInfo);
            }

            // Get size info
            vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo;
            accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
            accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
            accelerationStructureBuildGeometryInfo.geometryCount = (uint32_t) geometries.size();
            accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();

            auto accelerationStructureBuildSizesInfo = core->_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, accelerationStructureBuildGeometryInfo, maxPrimitiveCounts);

            //Build BLAS Buffer
            blasBuffer.push_back(vkutils::createBuffer(*core, accelerationStructureBuildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, vma::MemoryUsage::eAutoPreferDevice));

            //Get BLAS Handle
            vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo;
            accelerationStructureCreateInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
            accelerationStructureCreateInfo.buffer = blasBuffer.back()._buffer;
            accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
            blas.push_back(core->_device.createAccelerationStructureKHR(accelerationStructureCreateInfo));

            // Create ScratchBuffer
            vkutils::AllocatedBuffer scratchBuffer = vkutils::createBuffer(*core, accelerationStructureBuildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice, vma::AllocationCreateFlagBits::eDedicatedMemory);
            vk::BufferDeviceAddressInfo scratchBufferAdressInfo(scratchBuffer._buffer);
            vk::DeviceOrHostAddressConstKHR scratchBufferAddress;
            scratchBufferAddress.deviceAddress = core->_device.getBufferAddress(scratchBufferAdressInfo);

            accelerationStructureBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
            accelerationStructureBuildGeometryInfo.dstAccelerationStructure = blas.back();
            accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBufferAddress.deviceAddress;

            // Create Single-Use CommandBuffer and Build Acceleration Structure on GPU
            vk::CommandBufferAllocateInfo allocInfo{};
            allocInfo.level = vk::CommandBufferLevel::ePrimary;
            allocInfo.commandPool = core->_cmdPool;
            allocInfo.commandBufferCount = 1;

            vk::CommandBuffer commandBuffer = core->_device.allocateCommandBuffers(allocInfo).front();

            vk::CommandBufferBeginInfo beginInfo{};
            beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            commandBuffer.begin(beginInfo);
                commandBuffer.buildAccelerationStructuresKHR(1, &accelerationStructureBuildGeometryInfo, pBuildRangeInfos.data());
            commandBuffer.end();

            vk::SubmitInfo submitInfo{};
            submitInfo.setCommandBuffers(commandBuffer);
            core->_graphicsQueue.submit(submitInfo);
            core->_graphicsQueue.waitIdle();
            core->_device.freeCommandBuffers(core->_cmdPool, commandBuffer);

            vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo;
            accelerationDeviceAddressInfo.accelerationStructure = blas.back();
            blasAddress.push_back(core->_device.getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo));

            //delete Scratch Buffer
            core->_allocator.destroyBuffer(scratchBuffer._buffer, scratchBuffer._allocation);
            core->_allocator.destroyBuffer(transformBuffer._buffer, transformBuffer._allocation);
            modelIndexOffset += (uint32_t) model->_indices.size();
            modelVertexOffset += (uint32_t) model->_vertices.size();
            modelTextureOffset += (uint32_t) model->_textures.size();
        }
        vk::DeviceSize materialBufferSize = static_cast<uint32_t>(materials.size()) * sizeof(vkutils::Material);
        materialBuffer =  vkutils::deviceBufferFromData(*core, materials.data(), materialBufferSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice);
    }
    ///build tlas
    {
        std::vector<vk::AccelerationStructureInstanceKHR> instances;
        vk::TransformMatrixKHR transformMatrix = std::array<std::array<float, 4>, 3>{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f 
        };

        for(uint32_t i = 0; i < blasAddress.size(); i++){
            vk::DeviceAddress& address = blasAddress[i];
            uint32_t offset = materialOffsets[i];
            instances.push_back(vk::AccelerationStructureInstanceKHR(transformMatrix, offset, 0xFF, 0, vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable, address));
        }

        vk::DeviceSize instancesBufferSize = instances.size() * sizeof(vk::AccelerationStructureInstanceKHR);
        vkutils::AllocatedBuffer instancesBuffer = vkutils::deviceBufferFromData(*core, instances.data(), instancesBufferSize, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vma::MemoryUsage::eAutoPreferDevice);

        vk::DeviceOrHostAddressConstKHR instanceDataDeviceAddress;
        vk::BufferDeviceAddressInfo instanceBufferAdressInfo(instancesBuffer._buffer);
        instanceDataDeviceAddress.deviceAddress = core->_device.getBufferAddress(instanceBufferAdressInfo);

        vk::AccelerationStructureGeometryInstancesDataKHR instancesData(VK_FALSE, instanceDataDeviceAddress);

        vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
        accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
        accelerationStructureGeometry.geometry.instances = instancesData;

        vk::AccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo;
        accelerationStructureBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
        accelerationStructureBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
        accelerationStructureBuildGeometryInfo.setGeometries(accelerationStructureGeometry);

        uint32_t primitive_count = (uint32_t) instances.size();

        auto accelerationStructureBuildSizesInfo = core->_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, accelerationStructureBuildGeometryInfo, primitive_count);

        tlasBuffer = vkutils::createBuffer(*core, accelerationStructureBuildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR, vma::MemoryUsage::eAutoPreferDevice, vma::AllocationCreateFlagBits::eDedicatedMemory);

        vk::AccelerationStructureCreateInfoKHR accelerationStructureCreateInfo;
        accelerationStructureCreateInfo.buffer = tlasBuffer._buffer;
        accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;

        tlas = core->_device.createAccelerationStructureKHR(accelerationStructureCreateInfo);

        vkutils::AllocatedBuffer scratchBuffer = vkutils::createBuffer(*core, accelerationStructureBuildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer, vma::MemoryUsage::eAutoPreferDevice, vma::AllocationCreateFlagBits::eDedicatedMemory);
        vk::BufferDeviceAddressInfo scratchBufferAdressInfo(scratchBuffer._buffer);
        vk::DeviceOrHostAddressConstKHR scratchBufferAddress;
        scratchBufferAddress.deviceAddress = core->_device.getBufferAddress(scratchBufferAdressInfo);

        accelerationStructureBuildGeometryInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        accelerationStructureBuildGeometryInfo.dstAccelerationStructure = tlas;
        accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBufferAddress.deviceAddress;

        vk::AccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo;
        accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;

        std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandPool = core->_cmdPool;
        allocInfo.commandBufferCount = 1;

        vk::CommandBuffer cmd = core->_device.allocateCommandBuffers(allocInfo).front();

        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        cmd.begin(beginInfo);
            cmd.buildAccelerationStructuresKHR(1, &accelerationStructureBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());
        cmd.end();

        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers(cmd);
        core->_graphicsQueue.submit(submitInfo);
        core->_graphicsQueue.waitIdle();
        core->_device.freeCommandBuffers(core->_cmdPool, cmd);

        vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo;
        accelerationDeviceAddressInfo.accelerationStructure = tlas;

        tlasAddress = core->_device.getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);

        core->_allocator.destroyBuffer(scratchBuffer._buffer, scratchBuffer._allocation);
        core->_allocator.destroyBuffer(instancesBuffer._buffer, instancesBuffer._allocation);
    }
}

void Scene::destroy()
{
    for(auto model : models){
        model->destroy();
    }
    core->_allocator.destroyBuffer(indexBuffer._buffer, indexBuffer._allocation);
    core->_allocator.destroyBuffer(vertexBuffer._buffer, vertexBuffer._allocation);
    core->_allocator.destroyBuffer(materialBuffer._buffer, materialBuffer._allocation);
    for(auto buffer : blasBuffer){
        core->_allocator.destroyBuffer(buffer._buffer, buffer._allocation);
    }
    core->_allocator.destroyBuffer(tlasBuffer._buffer, tlasBuffer._allocation);
    for(auto as : blas){
        core->_device.destroyAccelerationStructureKHR(as);
    }
    core->_device.destroyAccelerationStructureKHR(tlas);
}
