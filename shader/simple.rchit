#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct GeometryNode {
  int indexOffset;
  int vertexOffset;
  int baseColorTexture;
  int metallicRoughnessTexture;
  int normalTexture;
  int occlusionTexture;
  int emissiveTexture;
  int specularGlossinessTexture;
  int diffuseTexture;
  float alphaCutoff;
  float metallicFactor;
  float roughnessFactor;
  float baseColorFactor[4];
  uint alphaMode;
  float pad[3];
};

struct Vertex {
  vec3 pos;
  float pad0;
	vec3 normal;
  float pad1;
	vec2 uv;
  float pad2[2];
	vec4 color;
	vec4 joint0;
	vec4 weight0;
	vec4 tangent;
};

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) buffer Indices { uint i[]; } indices;
layout(binding = 3, set = 0) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 4, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;

struct RayPayload {
	vec3 color;
	uint recursion;
  float weight;
};

layout(location = 0) rayPayloadInEXT RayPayload Payload;

hitAttributeEXT vec2 attribs;

uvec3 pcg3d(uvec3 v) {
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  v ^= v >> 16u;
  v.x += v.y * v.z;
  v.y += v.z * v.x;
  v.z += v.x * v.y;
  return v;
}
vec3 random3(vec3 seed) {
  return normalize((uintBitsToFloat((pcg3d(floatBitsToUint(seed)) & 0x007FFFFFu) | 0x3F800000u) - 1.0) * 2 - vec3(1.0));
}

vec3 random_on_hemisphere(vec3 seed, const vec3 normal) {
    vec3 on_unit_sphere = normalize(normal + random3(seed));
    if (dot(on_unit_sphere, normal) > 0.0)
        return on_unit_sphere;
    else
        return -on_unit_sphere;
}

void main()
{
  GeometryNode geometryNode = geometryNodes.nodes[gl_GeometryIndexEXT];

  const uint triIndex = geometryNode.indexOffset + gl_PrimitiveID * 3;

  Vertex TriVertices[3];
  for (uint i = 0; i < 3; i++) {
    uint index = indices.i[triIndex + i];
    TriVertices[i] = vertices.v[index];
	}
	vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

	vec2 uv =       TriVertices[0].uv *               barycentricCoords.x + TriVertices[1].uv *           barycentricCoords.y + TriVertices[2].uv *           barycentricCoords.z;
  vec3 tangent =  TriVertices[0].tangent.xyz * barycentricCoords.x + TriVertices[1].tangent.xyz *  barycentricCoords.y + TriVertices[2].tangent.xyz *  barycentricCoords.z;
	vec3 normal =   TriVertices[0].normal *       barycentricCoords.x + TriVertices[1].normal *       barycentricCoords.y + TriVertices[2].normal *       barycentricCoords.z;
  vec3 pos =      TriVertices[0].pos *             barycentricCoords.x + TriVertices[1].pos *          barycentricCoords.y + TriVertices[2].pos *          barycentricCoords.z;
  vec3 newDir = random_on_hemisphere(pos, normal);
  
  Payload.color = (1-Payload.weight) * Payload.color + Payload.weight * vec3(0.0);
  Payload.recursion += 1;
  if(Payload.recursion < 5) {
    Payload.weight *= 0.5;
    float epsilon = 0.01;
    vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + normal * epsilon;
    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin, epsilon, newDir, 10000.0, 0);
  }
}
