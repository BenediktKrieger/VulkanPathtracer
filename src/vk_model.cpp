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
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(jointAttribute);
	description.attributes.push_back(tangentAttribute);
	return description;
}

void Model::loadNode(const tinygltf::Node &inputNode, const tinygltf::Model &input, Node *parent, std::vector<uint32_t> &indexBuffer, std::vector<Vertex> &vertexBuffer)
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
			loadNode(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);
		}
	}

	// If the node contains mesh data, we load vertices and indices from the buffers
	// In glTF this is done via accessors and buffer views
	if (inputNode.mesh > -1)
	{
		const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
		// Iterate through all primitives of this node's mesh
		for (size_t i = 0; i < mesh.primitives.size(); i++)
		{
			const tinygltf::Primitive &glTFPrimitive = mesh.primitives[i];
			uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			// Vertices
			{
				const float *positionBuffer = nullptr;
				const float *normalsBuffer = nullptr;
				const float *texCoordsBuffer = nullptr;
				const float *tangentsBuffer = nullptr;
				size_t vertexCount = 0;

				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// POI: This sample uses normal mapping, so we also need to load the tangents from the glTF file
				if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end())
				{
					const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
					tangentsBuffer = reinterpret_cast<const float *>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
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
				const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView &bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer &buffer = input.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType)
				{
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
				{
					const uint32_t *buf = reinterpret_cast<const uint32_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
				{
					const uint16_t *buf = reinterpret_cast<const uint16_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
				{
					const uint8_t *buf = reinterpret_cast<const uint8_t *>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++)
					{
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}
			Primitive *primitive = new Primitive{};
			primitive->firstIndex = firstIndex;
			primitive->indexCount = indexCount;
			primitive->materialIndex = glTFPrimitive.material;
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

bool Model::load_from_gltf(const char *filename)
{
	tinygltf::Model glTFInput;
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;

	std::vector<uint32_t> indexBuffer;
	std::vector<Vertex> vertexBuffer;

	bool fileLoaded = gltfContext.LoadBinaryFromFile(&glTFInput, &error, &warning, filename);

	if (fileLoaded)
	{
		const tinygltf::Scene &scene = glTFInput.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++)
		{
			const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
			loadNode(node, glTFInput, nullptr, _indices, _vertices);
		}

		// Use transform matrices from the glTF nodes
		// std::vector<VkTransformMatrixKHR> transformMatrices{};
		// for (auto node : _nodes) {
		// 	if (node.mesh) {
		// 		const tinygltf::Mesh mesh = glTFInput.meshes[node.mesh];
		// 		for (auto primitive : mesh.primitives) {
		// 			if (primitive->indexCount > 0) {
		// 				VkTransformMatrixKHR transformMatrix{};tinygltf::Primitive
		// 				auto m = glm::mat3x4(glm::transpose(node.getMatrix()));
		// 				memcpy(&transformMatrix, (void*)&m, sizeof(glm::mat3x4));
		// 				transformMatrices.push_back(transformMatrix);
		// 			}
		// 		}
		// 	}
		// }
	}
	else
	{
		return false;
	}
	return true;
}