#pragma once

#include <vk_types.h>
#include <vector>
#include <vk_utils.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct VertexInputDescription {
	std::vector<vk::VertexInputBindingDescription> bindings;
	std::vector<vk::VertexInputAttributeDescription> attributes;
	vk::PipelineVertexInputStateCreateFlags flags;
};

struct Material {
	enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
	AlphaMode alphaMode = ALPHAMODE_OPAQUE;
	float alphaCutoff = 1.0f;
	float metallicFactor = 1.0f;
	float roughnessFactor = 1.0f;
	glm::vec4 baseColorFactor = glm::vec4(1.0f);
	uint32_t baseColorTexture = -1;
	uint32_t metallicRoughnessTexture = -1;
	uint32_t normalTexture = -1;
	uint32_t occlusionTexture = -1;
	uint32_t emissiveTexture = -1;
	uint32_t specularGlossinessTexture = -1;
	uint32_t diffuseTexture = -1;
};

struct Primitive {
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t firstVertex;
	uint32_t vertexCount;
	uint32_t materialIndex;
};

struct Node {
	Node* parent;
	std::vector<Node*> children;
	glm::mat4 matrix;
	std::string name;
	std::vector<Primitive*> primitives;
	glm::vec3 translation{};
	glm::vec3 scale{ 1.0f };
	glm::quat rotation{};
	glm::mat4 localMatrix();
	glm::mat4 getMatrix();
	~Node();
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
	glm::vec4 color;
	glm::vec4 joint0;
	glm::vec4 weight0;
	glm::vec4 tangent;
	static VertexInputDescription get_vertex_description();
}; 

class Model {
public:
	std::vector<Node*> _nodes; 
	std::vector<Node*> _linearNodes; 
	std::vector<uint32_t> _indices;
	std::vector<Vertex> _vertices;
	vkutils::AllocatedBuffer _vertexBuffer;
	vkutils::AllocatedBuffer _indexBuffer;
	bool load_from_gltf(const char* filename);
private:
	void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);
};