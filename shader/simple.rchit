#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#define PI 3.14159265358979323

struct Material {
  uint indexOffset;
  uint vertexOffset;
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
  float transmissionFactor;
  float ior;
  uint alphaMode;
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

struct RayPayload {
	vec3 color;
	vec3 origin;
	vec3 dir;
	uint translucentRecursion;
	uint diffuseRecursion;
	bool continueTrace;
  bool shadow;
};


layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) buffer Indices { uint i[]; } indices;
layout(binding = 4, set = 0) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 5, set = 0) buffer Materials { Material m[]; } materials;
layout(binding = 7, set = 0) uniform Settings {
  bool accumulate;
	uint samples;
	uint reflection_recursion;
	uint refraction_recursion;
  float ambient_multiplier;
} settings;
layout(binding = 8, set = 0) uniform sampler2D texSampler[];

layout(location = 0) rayPayloadInEXT RayPayload Payload;

hitAttributeEXT vec2 attribs;

uvec4 seed = uvec4(gl_LaunchIDEXT.y * gl_LaunchIDEXT.x + gl_LaunchIDEXT.x, floatBitsToUint(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT));

void pcg4d(inout uvec4 v)
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.w; 
  v.y += v.z * v.x; 
  v.z += v.x * v.y; 
  v.w += v.y * v.z;
  v = v ^ (v >> 16u);
  v.x += v.y * v.w; 
  v.y += v.z * v.x; 
  v.z += v.x * v.y; 
  v.w += v.y * v.z;
}

float rand()
{
  pcg4d(seed);
  return float(seed.x) / float(0xffffffffu);
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

vec3 refractRay(const vec3 I, const vec3 N, const float ior) { 
  float cosi = clamp(-1, 1, dot(N, I));
  float eta = 1 / ior;
  vec3 n = N; 
  if (cosi < 0) {
    cosi = -cosi;
  } else {
    eta = ior;
    n = -N;
  }
  float k = 1 - eta * eta * (1 - cosi * cosi); 
  if(k < 0)
    return vec3(0);
  else
    return eta * I - (eta * cosi + sqrt(k)) * n;
}

void fresnel(const vec3 I, const vec3 N, const float ior, inout float kr, inout float kt){ 
  float cosi = clamp(-1, 1, dot(I, N)); 
  float etai = 1; 
  float etat = ior; 
  if (cosi > 0) { 
    float oldetai = etai;
    etai = etat;
    etat = oldetai;
  }
  float sint = etai / etat * sqrt(max(0.f, 1 - cosi * cosi));
  if (sint >= 1) { 
    kr = 1; 
  } 
  else { 
    float cost = sqrt(max(0.f, 1 - sint * sint)); 
    cosi = abs(cosi); 
    float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost)); 
    float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost)); 
    kr = (Rs * Rs + Rp * Rp) / 2; 
  }
  kt = 1.0 - kr;
} 

void main()
{
  mat4 normalToWorld = transpose(inverse(mat4(gl_ObjectToWorldEXT)));
  Material material = materials.m[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
  const uint triIndex = material.indexOffset + gl_PrimitiveID * 3;
  Vertex TriVertices[3];
  for (uint i = 0; i < 3; i++) {
    uint index = material.vertexOffset + indices.i[triIndex + i];
    TriVertices[i] = vertices.v[index];
	}
	vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	vec2 uv = TriVertices[0].uv * barycentricCoords.x + TriVertices[1].uv * barycentricCoords.y + TriVertices[2].uv *  barycentricCoords.z;
  // color
  vec3 color = material.baseColorFactor.xyz;
  if(material.baseColorTexture >= 0){
    color = texture(texSampler[material.baseColorTexture], uv).xyz;
  }
  // normal
  vec3 normal = normalize(TriVertices[0].normal * barycentricCoords.x + TriVertices[1].normal * barycentricCoords.y + TriVertices[2].normal * barycentricCoords.z);
  if(material.normalTexture >= 0){
    vec3 tangent = normalize(TriVertices[0].tangent.xyz * barycentricCoords.x + TriVertices[1].tangent.xyz *  barycentricCoords.y + TriVertices[2].tangent.xyz *  barycentricCoords.z);
    vec3 binormal = cross(normal, tangent);
    normal = normalize(mat3(tangent, binormal, normal) * (texture(texSampler[material.normalTexture], uv).xyz * 2.0 - 1.0));
  }
  normal = normalize((normalToWorld * vec4(normal, 1.0)).xyz);
  // roughness
  float roughness = material.roughnessFactor;
  // metallness
  float metallness = material.metallicFactor;
  
  vec3 newOrigin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
  vec3 newDir;
  if(material.transmissionFactor > 0.01){
    // diallectic
    float reflectance = 0.0;
    float transmission = 0.0;
    fresnel(gl_WorldRayDirectionEXT, normal, material.ior, reflectance, transmission);
    if(reflectance > 0.999){
      newDir = reflect(gl_WorldRayDirectionEXT, normal);
    }
    else if(transmission > 0.999){
      newDir = normalize(refractRay(gl_WorldRayDirectionEXT, normal, material.ior));
    }
    else {
      float russian_roulette = rand();
      if(russian_roulette < reflectance){
        newDir = reflect(gl_WorldRayDirectionEXT, normal);
      } else {
        newDir = normalize(refractRay(gl_WorldRayDirectionEXT, normal, material.ior));
      }
    }
    Payload.translucentRecursion += 1;
  } else if(roughness < 0.01) {
    // mirror
    newDir = reflect(gl_WorldRayDirectionEXT, normal);
    Payload.diffuseRecursion += 1;
  } else if(roughness < 0.9 && metallness > 0.01) {
    // metall
    newDir = normalize(reflect(gl_WorldRayDirectionEXT, normal) * max(1.0, (1-roughness) * 10) + random_uniform_hemisphere());
    Payload.diffuseRecursion += 1;
  } else {
    // lambertian
    newDir = random_on_cosine_hemisphere(normal);
    Payload.diffuseRecursion += 1;
  }

  if(Payload.diffuseRecursion >= settings.reflection_recursion){
    color = vec3(0.0);
    Payload.continueTrace = false;
  }
  if(Payload.translucentRecursion >= settings.refraction_recursion){
    color = vec3(0.0);
    Payload.continueTrace = false;
  }

  if(material.emissiveStrength > 1.0 || material.emissiveTexture >= 0){
    //emissive
    vec3 emission = vec3(1.0);
    if(material.emissiveTexture >= 0){
      vec3 emissiveFactor = texture(texSampler[material.emissiveTexture], uv).xyz;
      emission = vec3(material.emissiveStrength) * emissiveFactor;
      if(emissiveFactor.x > 0.1 || emissiveFactor.y > 0.1 || emissiveFactor.z > 0.1){
        Payload.continueTrace = false;
        color = color + emission;
      }
    } else {
      emission = vec3(material.emissiveStrength) * material.emissiveFactor.xyz;
      Payload.continueTrace = false;
      color = color + emission;
    }
  }

  Payload.color *= color.xyz;
  Payload.origin = newOrigin;
  Payload.dir = newDir;
  // Shadow testing
  // Payload.shadow = true;
  // traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0, 0, 1, newOrigin, 0.0001, vec3(0.05, 0.05, 1), 10000, 0);
  // if(Payload.shadow){
  // }
}