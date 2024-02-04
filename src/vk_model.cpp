#include <vk_model.h>
#include <iostream>

VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description;

	// we will have just 1 vertex buffer binding, with a per-vertex rate
	vk::VertexInputBindingDescription mainBinding = {};
	mainBinding.binding = 0;
	mainBinding.stride = sizeof(Vertex);
	mainBinding.inputRate = vk::VertexInputRate::eVertex;
	description.bindings.push_back(mainBinding);

	// Position will be stored at Location 0
	vk::VertexInputAttributeDescription posAttribute = {};
	posAttribute.binding = 0;
	posAttribute.location = 0;
	posAttribute.format = vk::Format::eR32G32B32Sfloat;
	posAttribute.offset = offsetof(Vertex, pos);

	// Normal will be stored at Location 1
	vk::VertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = vk::Format::eR32G32B32Sfloat;
	normalAttribute.offset = offsetof(Vertex, normal);

	// Position will be stored at Location 2
	vk::VertexInputAttributeDescription uvAttribute = {};
	uvAttribute.binding = 0;
	uvAttribute.location = 2;
	uvAttribute.format = vk::Format::eR32G32Sfloat;
	uvAttribute.offset = offsetof(Vertex, uv);

	// Position will be stored at Location 3
	vk::VertexInputAttributeDescription colorAttribute = {};
	colorAttribute.binding = 0;
	colorAttribute.location = 3;
	colorAttribute.format = vk::Format::eR32G32B32A32Sfloat;
	colorAttribute.offset = offsetof(Vertex, color);

	// Position will be stored at Location 4
	vk::VertexInputAttributeDescription jointAttribute = {};
	jointAttribute.binding = 0;
	jointAttribute.location = 4;
	jointAttribute.format = vk::Format::eR32G32B32A32Sfloat;
	jointAttribute.offset = offsetof(Vertex, joint0);

	// Position will be stored at Location 5
	vk::VertexInputAttributeDescription weightAttribute = {};
	weightAttribute.binding = 0;
	weightAttribute.location = 5;
	weightAttribute.format = vk::Format::eR32G32B32A32Sfloat;
	weightAttribute.offset = offsetof(Vertex, weight0);

	// Position will be stored at Location 6
	vk::VertexInputAttributeDescription tangentAttribute = {};
	tangentAttribute.binding = 0;
	tangentAttribute.location = 6;
	tangentAttribute.format = vk::Format::eR32G32B32A32Sfloat;
	tangentAttribute.offset = offsetof(Vertex, tangent);

	description.attributes.push_back(posAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(uvAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(jointAttribute);
	description.attributes.push_back(weightAttribute);
	description.attributes.push_back(tangentAttribute);
	return description;
}

Model::Model(): core(){
	isBuilded = false;
}

Model::Model(vk::Core& core): core(&core){
	isBuilded = false;
}

void Model::destroy()
{
	if(isBuilded){
		for(auto& node : _linearNodes) {
			delete node;
		}
		
		for(auto& texture : _textures) {
			core->_device.destroyImageView(texture.image._view);
			core->_allocator.destroyImage(texture.image._image, texture.image._allocation);
		}
		core->_device.destroySampler(_sampler);
		_textures.clear();
	}
}

void Model::loadImages()
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
	_sampler = core->_device.createSampler(samplerInfo);

	for (tinygltf::Image &image : _input.images) {
		Texture texture;
		unsigned char* buffer = nullptr;
		vk::DeviceSize bufferSize = 0;
		
		bool deleteBuffer = false;
		if (image.component == 3) {
			bufferSize = image.width * image.height * 4;
			buffer = new unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			unsigned char* rgb = &image.image[0];
			for (size_t i = 0; i < image.width * image.height; ++i) {
				for (int32_t j = 0; j < 3; ++j) {
					rgba[j] = rgb[j];
				}
				rgba += 4;
				rgb += 3;
			}
			deleteBuffer = true;
		}
		else {
			buffer = &image.image[0];
			bufferSize = image.image.size();
		}

		vk::Format format = vk::Format::eR8G8B8A8Unorm;
		uint32_t width = image.width;
		uint32_t height = image.height;

		auto formatProperties = core->_chosenGPU.getFormatProperties(format);
		if(!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc) || !(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
		{
			throw std::runtime_error("unsported Image Format!");
		}

		vk::ImageCreateInfo imageCreateInfo;
		imageCreateInfo.imageType = vk::ImageType::e2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
		imageCreateInfo.extent = vk::Extent3D{ width, height, 1 };
		imageCreateInfo.usage = vk::ImageUsageFlagBits::eSampled;
		texture.image = vkutils::imageFromData(*core, buffer, imageCreateInfo, vk::ImageAspectFlagBits::eColor, vma::MemoryUsage::eAutoPreferDevice);

		vk::DescriptorImageInfo imageInfo;
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		imageInfo.imageView = texture.image._view;
		imageInfo.sampler = _sampler;
		texture.descriptor = imageInfo;

		texture.index = static_cast<uint32_t>(_textures.size());
		_textures.push_back(texture);
		if (deleteBuffer) {
            delete[] buffer;
        }
	}
}

void Model::loadMaterials()
{
	for (tinygltf::Material &mat : _input.materials)
	{
		Material material;

		if (mat.values.find("roughnessFactor") != mat.values.end())
		{
			material.roughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
		}
		if (mat.values.find("metallicFactor") != mat.values.end())
		{
			material.metallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
		}
		if (mat.values.find("emissiveFactor") != mat.values.end())
		{
			material.emissiveFactor = glm::vec4(glm::make_vec3(mat.values["emissiveFactor"].ColorFactor().data()), 1.0f);
		}
		if (mat.extensions.find("KHR_materials_emissive_strength") != mat.extensions.end())
		{
			auto ext = mat.extensions.find("KHR_materials_emissive_strength");
			if (ext->second.Has("emissiveStrength")) {
				auto value = ext->second.Get("emissiveStrength");
				material.emissiveStrength = (float)value.Get<double>();
			}
		}
		if (mat.extensions.find("KHR_materials_transmission") != mat.extensions.end())
		{
			auto ext = mat.extensions.find("KHR_materials_transmission");
			if (ext->second.Has("transmissionFactor")) {
				auto value = ext->second.Get("transmissionFactor");
				material.transmissionFactor = (float)value.Get<double>();
			}
		}
		if (mat.extensions.find("KHR_materials_ior") != mat.extensions.end())
		{
			auto ext = mat.extensions.find("KHR_materials_ior");
			if (ext->second.Has("ior")) {
				auto value = ext->second.Get("ior");
				material.ior = (float)value.Get<double>();
			}
		}
		if (mat.values.find("baseColorFactor") != mat.values.end())
		{
			material.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
		}
		if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end())
		{
			tinygltf::Parameter param = mat.additionalValues["alphaMode"];
			if (param.string_value == "BLEND")
			{
				material.alphaMode = Material::ALPHAMODE_BLEND;
			}
			if (param.string_value == "MASK")
			{
				material.alphaMode = Material::ALPHAMODE_MASK;
			}
		}
		if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end())
		{
			material.alphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
		}
		if (mat.values.find("baseColorTexture") != mat.values.end()) {
			material.baseColorTexture = getTextureIndex(_input.textures[mat.values["baseColorTexture"].TextureIndex()].source);
		}
		if (mat.values.find("metallicRoughnessTexture") != mat.values.end()) {
			material.metallicRoughnessTexture = getTextureIndex(_input.textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
		}
		if(mat.additionalValues.find("normalTexture") != mat.additionalValues.end()) {
			material.normalTexture = getTextureIndex(_input.textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
		}
		if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end()) {
			material.emissiveTexture = getTextureIndex(_input.textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
		}
		if (mat.values.find("occlusionTexture") != mat.values.end()) {
			material.occlusionTexture = getTextureIndex(_input.textures[mat.values["occlusionTexture"].TextureIndex()].source);
		}
		if (mat.values.find("specularGlossinessTexture") != mat.values.end()) {
			material.specularGlossinessTexture = getTextureIndex(_input.textures[mat.values["specularGlossinessTexture"].TextureIndex()].source);
		}
		if (mat.values.find("diffuseTexture") != mat.values.end()) {
			material.diffuseTexture = getTextureIndex(_input.textures[mat.values["diffuseTexture"].TextureIndex()].source);
		}

		_materials.push_back(material);
	}
	_materials.push_back(Material());
}

uint32_t Model::getTextureIndex(uint32_t index)
{
	if (index < _textures.size() && index >= 0) {
		return index;
	}
	return static_cast<uint32_t>(_textures.size() - 1);
}

void Model::loadNode(const tinygltf::Node &inputNode, Node *parent, std::vector<uint32_t> &indexBuffer, std::vector<Vertex> &vertexBuffer)
{
	Node *node = new Node{};
	node->name = inputNode.name;
	node->parent = parent;

	// Get the local node matrix
	// It's either made up from translation, rotation, scale or a 4x4 matrix
	node->matrix = glm::mat4(1.0f);
	if (inputNode.translation.size() == 3)
	{
		node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
	}
	if (inputNode.rotation.size() == 4)
	{
		glm::quat q = glm::make_quat(inputNode.rotation.data());
		node->matrix *= glm::mat4(q);
	}
	if (inputNode.scale.size() == 3)
	{
		node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
	}
	if (inputNode.matrix.size() == 16)
	{
		node->matrix = glm::make_mat4x4(inputNode.matrix.data());
	};

	// Load node's children
	if (inputNode.children.size() > 0)
	{
		for (size_t i = 0; i < inputNode.children.size(); i++)
		{
			loadNode(_input.nodes[inputNode.children[i]], node, indexBuffer, vertexBuffer);
		}
	}

	// If the node contains mesh data, we load vertices and indices from the buffers
	// In glTF this is done via accessors and buffer views
	if (inputNode.mesh > -1)
	{
		const tinygltf::Mesh mesh = _input.meshes[inputNode.mesh];
		// Iterate through all primitives of this node's mesh
		for (size_t i = 0; i < mesh.primitives.size(); i++)
		{
			const tinygltf::Primitive &glTFPrimitive = mesh.primitives[i];
			uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
			uint32_t firstVertex = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			uint32_t vertexCount = 0;
			// Vertices
			{
				const float *positionBuffer = nullptr;
				const float *normalsBuffer = nullptr;
				const float *texCoordsBuffer = nullptr;
				const float *tangentsBuffer = nullptr;

				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor &accessor = _input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView &view = _input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float *>(&(_input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = static_cast<uint32_t>(accessor.count);
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor &accessor = _input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView &view = _input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float *>(&(_input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor &accessor = _input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView &view = _input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float *>(&(_input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// POI: This sample uses normal mapping, so we also need to load the tangents from the glTF file
				if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor &accessor = _input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView &view = _input.bufferViews[accessor.bufferView];
					tangentsBuffer = reinterpret_cast<const float *>(&(_input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++)
				{
					Vertex vert{};
					vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
					vert.color = glm::vec4(1.0f);
					vert.tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor &accessor = _input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView &bufferView = _input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer &buffer = _input.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType)
				{
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
				{
					const uint32_t *buf = reinterpret_cast<const uint32_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + firstVertex);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
				{
					const uint16_t *buf = reinterpret_cast<const uint16_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + firstVertex);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
				{
					const uint8_t *buf = reinterpret_cast<const uint8_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + firstVertex);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}
			Primitive *primitive = new Primitive(firstIndex, indexCount, firstVertex, vertexCount, glTFPrimitive.material > -1 ? _materials[glTFPrimitive.material] : _materials.back());
			node->primitives.push_back(primitive);
		}
	}

	if (parent)
	{
		parent->children.push_back(node);
	}
	else
	{
		_nodes.push_back(node);
	}
	_linearNodes.push_back(node);
}

bool Model::load_from_glb(const char *filename)
{
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;

	bool fileLoaded = gltfContext.LoadBinaryFromFile(&_input, &error, &warning, filename);

	return fileLoaded;
}

tinygltf::Model* Model::getGltfData()
{
	return &_input;
}

void Model::build()
{
	loadImages();
	loadMaterials();
	const tinygltf::Scene &scene = _input.scenes[0];
	for (size_t i = 0; i < scene.nodes.size(); i++)
	{
		const tinygltf::Node node = _input.nodes[scene.nodes[i]];
		loadNode(node, nullptr, _indices, _vertices);
	}

	for (auto node : _linearNodes)
	{
		if (node->primitives.size() > 0)
		{
			for (auto primitive : node->primitives)
			{
				if (primitive->indexCount > 0)
				{
					_transforms.push_back(vkutils::getTransformMatrixKHR(node->getMatrix()));
				}
			}
		}
	}
	isBuilded = true;
}

glm::mat4 Node::localMatrix()
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}

glm::mat4 Node::getMatrix()
{
	glm::mat4 m = localMatrix();
	Node *p = parent;
	while (p)
	{
		m = p->localMatrix() * m;
		p = p->parent;
	}
	return m;
}
