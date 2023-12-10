#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define PI 3.14159265358979323

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
  vec4 baseColorFactor;
  vec4 emissiveFactor;
  float emissiveStrength;
  uint alphaMode;
  float pad[2]; 
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
layout(binding = 3, set = 0) buffer Indices { uint i[]; } indices;
layout(binding = 4, set = 0) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 5, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
layout(binding = 6, set = 0) uniform sampler2D texSampler[];

struct RayPayload {
	vec3 color;
  vec3 attenuation;
  vec3 origin;
  vec3 dir;
	uint recursion;
  bool shadow;
};

layout(location = 0) rayPayloadInEXT RayPayload Payload;

hitAttributeEXT vec2 attribs;

uvec4 seed = uvec4(gl_LaunchIDEXT.y * gl_LaunchIDEXT.x + gl_LaunchIDEXT.x, floatBitsToUint(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT));

void pcg4d(inout uvec4 v)
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
  v = v ^ (v >> 16u);
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

float rand()
{
  pcg4d(seed); return float(seed.x) / float(0xffffffffu);
}

vec3 random_uniform_hemisphere(){
  float r1 = rand();
  float r2 = rand();
  float theta = 2 * PI * r1;
  float phi = acos(r2);
  float x = sin(phi) * cos(theta);
  float y = sin(phi) * sin(theta);
  float z = cos(phi);
  return vec3(x, y, z);
}

vec3 random_cosine_hemisphere(){
  float r1 = rand();
  float r2 = rand();
  float phi = 2*PI*r1;
  float x = cos(phi)*sqrt(r2);
  float y = sin(phi)*sqrt(r2);
  float z = sqrt(1-r2);
  return vec3(x, y, z);
}

vec3 random_on_cosine_hemisphere(vec3 normal){
  vec3 n = normal;
  vec3 h = abs(n.x) > 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
  vec3 v = normalize(cross(n, h));
  vec3 u = cross(n, v);
  mat3 uvw = mat3(u,v,n);
  return uvw * random_cosine_hemisphere();
}

vec3 random_on_uniform_hemisphere(vec3 normal){
  vec3 n = normal;
  vec3 h = abs(n.x) > 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
  vec3 v = normalize(cross(n, h));
  vec3 u = cross(n, v);
  mat3 uvw = mat3(u,v,n);
  return uvw * random_uniform_hemisphere();
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
  
  vec3 newOrigin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + normal * 0.0001;
  // vec3 reflectedDir = reflect(gl_WorldRayDirectionEXT, normal);
  // metallic
  // vec3 newDir = normalize(reflectedDir * 5 + random_unit_vector(pos));
  // lambertian
  vec3 newDir = random_on_cosine_hemisphere(normal);

  vec4 baseColor = geometryNode.baseColorFactor;
  if(geometryNode.baseColorTexture >= 0){
    baseColor = texture(texSampler[geometryNode.baseColorTexture], uv);
  }
  if(geometryNode.emissiveStrength > 1.0 || geometryNode.emissiveTexture >= 0){
    Payload.color = (1-Payload.attenuation) * Payload.color + Payload.attenuation * vec3(geometryNode.emissiveStrength) * geometryNode.emissiveFactor.xyz;
    Payload.attenuation = vec3(0.0);
    Payload.recursion = 1000;
  }else{
    Payload.color *= (1-Payload.attenuation);
    Payload.recursion += 1;
    Payload.attenuation *= baseColor.xyz;
    Payload.origin = newOrigin;
    Payload.dir = newDir;
  }
  // Shadow testing
  // Payload.shadow = true;
  // traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0, 0, 1, newOrigin, 0.0001, vec3(0.05, 0.05, 1), 10000, 0);
  // if(Payload.shadow){
  // }
}
