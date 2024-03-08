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
    mat4 modelMatrix;
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
layout(binding = 2, set = 0) buffer Indices { uint i[]; } indices;
layout(binding = 3, set = 0) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 4, set = 0) buffer Materials { Material m[]; } materials;
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
    v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
    v = v ^ (v >> 16u);
    v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

float rand()
{
    pcg4d(seed);
    return float(seed.x) / float(0xffffffffu);
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

vec3 random_on_cosine_hemisphere(vec3 normal){
    vec3 w = normal;
    vec3 h = abs(w.x) > 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 v = normalize(cross(w, h));
    vec3 u = cross(w, v);
    mat3 uvw = mat3(u,v,w);
    return uvw * random_cosine_hemisphere();
}

vec3 random_on_uniform_hemisphere(vec3 normal){
    vec3 w = normal;
    vec3 h = abs(w.x) > 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 v = normalize(cross(w, h));
    vec3 u = cross(w, v);
    mat3 uvw = mat3(u,v,w);
    return uvw * random_uniform_hemisphere();
}

float getLightPdf(vec3 origin, vec3 direction, bool wasSendToLight, float distanceToLight){
    //hardcoded light
    vec3 a = vec3(-0.24, 0.98799, -0.22);
    vec3 b = vec3(0.23, 0.98799, 0.16);
    // if ray hits light pdf is positive else 0
    if(wasSendToLight){
        float hit_distance = distanceToLight;
        vec3 hit_normal = vec3(0, -1.0, 0);
        float distanceSquared = pow(hit_distance, 2.0);
        float lightArea = (b.x-a.x)*(b.z-a.z);
        float lightCosine = max(0.0000001, abs(dot(direction, hit_normal)));
        return distanceSquared / (lightCosine * lightArea);
    } else {
        return 0;
    }
}

float getCosinePdf(vec3 normal, vec3 direction) {
    return max(0.0000001, dot(normal, direction) / PI);
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
    vec3 normal_geometric = normalize(TriVertices[0].normal * barycentricCoords.x + TriVertices[1].normal * barycentricCoords.y + TriVertices[2].normal * barycentricCoords.z);
    vec3 color = vec3(0.0);
    vec3 emission = vec3(0.0);

    // check for emission of hit
    if(material.emissiveStrength > 1.0 || material.emissiveTexture >= 0){
        if(material.emissiveTexture >= 0){
            vec3 emissiveFactor = texture(texSampler[material.emissiveTexture], uv).xyz;
            emission = vec3(material.emissiveStrength) * emissiveFactor;
            if(emissiveFactor.x > 0.1 || emissiveFactor.y > 0.1 || emissiveFactor.z > 0.1){
                Payload.color *= emission;
                Payload.continueTrace = false;
                return;
            }
        } else {
            emission = vec3(material.emissiveStrength) * material.emissiveFactor.xyz;
            Payload.color *= emission;
            Payload.continueTrace = false;
            return;
        }
    }

    if(Payload.diffuseRecursion >= settings.reflection_recursion){
        //this is the last bounce
        Payload.continueTrace = false;
    } else {
        // color
        color = material.baseColorFactor.xyz;
        if(material.baseColorTexture >= 0){
            color = texture(texSampler[material.baseColorTexture], uv).xyz;
        }
        // normal
        vec3 normal = normal_geometric;
        if(material.normalTexture >= 0){
            vec3 tangent = normalize(TriVertices[0].tangent.xyz * barycentricCoords.x + TriVertices[1].tangent.xyz *  barycentricCoords.y + TriVertices[2].tangent.xyz *  barycentricCoords.z);
            vec3 binormal = cross(normal, tangent);
            normal = normalize(mat3(tangent, binormal, normal) * (texture(texSampler[material.normalTexture], uv).xyz * 2.0 - 1.0));
        }
        normal = normalize((normalToWorld * vec4(normal, 1.0)).xyz);
        
        vec3 newOrigin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

        vec3 newDir = vec3(0.0);
        bool wasSendToLight = false;
        float distanceToLight = 0;
        if(rand() < 0.5){
            wasSendToLight = true;
            vec3 a = vec3(-0.24, 0.98799, -0.22);
            vec3 b = vec3(0.23, 0.98799, 0.16);
            vec3 pointOnLight = vec3(a.x + rand() * abs(b.x - a.x), 0.98799, a.z + rand() * abs(b.z - a.z));
            vec3 toLight = pointOnLight - newOrigin;
            distanceToLight = length(toLight);
            newDir = normalize(toLight);
            // Shadow testing
            Payload.shadow = true;

            traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 0, 0, 1, newOrigin, 0.0001, newDir, distanceToLight - 0.001, 0);
            
            if(Payload.shadow){
                Payload.color = vec3(0.0);
                Payload.continueTrace = false;
                return;
            }else{
                vec3 lightColor = vec3(17.0);
                float mat_pdf = getCosinePdf(normal, newDir);
                float sampling_pdf1 = getLightPdf(newOrigin, newDir, wasSendToLight, distanceToLight);
                float sampling_pdf2 = getCosinePdf(normal, newDir);
                float sampling_pdf = 0.5 * sampling_pdf1 + 0.5 * sampling_pdf2;
                Payload.color = (color * mat_pdf) / sampling_pdf * lightColor;
                Payload.continueTrace = false;
                return;
            }
        } else {
            newDir = random_on_cosine_hemisphere(normal);
        }
        float mat_pdf = getCosinePdf(normal, newDir);
        float sampling_pdf1 = getLightPdf(newOrigin, newDir, wasSendToLight, distanceToLight);
        float sampling_pdf2 = getCosinePdf(normal, newDir);
        float sampling_pdf = 0.5 * sampling_pdf1 + 0.5 * sampling_pdf2;

        color = (color * mat_pdf) / sampling_pdf;

        Payload.diffuseRecursion += 1;
        Payload.origin = newOrigin;
        Payload.dir = newDir;
    }

    Payload.color *= color;
}