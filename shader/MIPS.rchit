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

struct Light {
    vec3 min;
    uint geoType;
    vec3 max;
    float radius;
    vec3 center;
    float radiosity;
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
    float f;
    float pdf;
	uint translucentRecursion;
	uint diffuseRecursion;
	bool continueTrace;
};

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) readonly buffer Indices { uint i[]; } indices;
layout(binding = 3, set = 0) readonly buffer Vertices { Vertex v[]; } vertices;
layout(binding = 4, set = 0) readonly buffer Materials { Material m[]; } materials;
layout(binding = 5, set = 0) readonly buffer Lights { Light l[]; } lights;
layout(binding = 7, set = 0) readonly uniform Settings {
    bool accumulate;
	uint min_samples;
    bool limit_samples;
    uint max_samples;
	uint reflection_recursion;
	uint refraction_recursion;
    float ambient_multiplier;
    bool auto_exposure;
    float exposure;
    bool mips;
    float mips_sensitivity;
} settings;
layout(binding = 8, set = 0) uniform sampler2D texSampler[];

layout(location = 0) rayPayloadInEXT RayPayload Payload;

hitAttributeEXT vec2 attribs;

uvec4 seed = uvec4(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, floatBitsToUint(gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT));

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

bool intersectSphere(vec3 center, float radius, vec3 origin, vec3 direction, inout float t){
    vec3  oc           = origin - center;
    float a            = dot(direction, direction);
    float b            = 2.0 * dot(oc, direction);
    float c            = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4 * a * c;
    if(discriminant < 0) {
        t = -1;
        return false;
    } else{
        float numerator = -b - sqrt(discriminant);
        if(numerator > 0.0) {
            t = numerator / (2.0 * a);
            return true;
        }
        numerator = -b + sqrt(discriminant);
        if(numerator > 0.0) {
            t = numerator / (2.0 * a);
            return true;
        }else {
            t = -1;
            return false;
        }
    }
}

bool intersectAABB(vec3 minB, vec3 maxB, vec3 origin, vec3 direction, inout float t){
    direction.x = abs(direction.x) < 0.000001 ? 0.000001 : direction.x;
    direction.y = abs(direction.y) < 0.000001 ? 0.000001 : direction.y;
    direction.z = abs(direction.z) < 0.000001 ? 0.000001 : direction.z;
    vec3 dirfrac = 1.0 / vec3(direction.x, direction.y, direction.z);
    vec3 t1 = (minB - origin) * dirfrac;
    vec3 t2 = (maxB - origin) * dirfrac;
    float tmin = max(max(min(t1.x, t2.x), min(t1.y, t2.y)), min(t1.z, t2.z));
    float tmax = min(min(max(t1.x, t2.x), max(t1.y, t2.y)), max(t1.z, t2.z));
    if (tmax < 0 || tmin > tmax) {
        t = tmax;
        return false;
    }
    t = tmin;
    return true;
}

vec3 random_on_aabb(vec3 minB, vec3 maxB, vec3 origin) {
    vec3 pointInLight = vec3(minB.x + rand() * abs(maxB.x - minB.x), minB.y + rand() * abs(maxB.y - minB.y), minB.z + rand() * abs(maxB.z - minB.z));
    vec3 dir = normalize(pointInLight - origin);
    float visibleAreas[3];
    for(uint face = 0; face < 3; face++){
        vec3 normalFace = vec3(0.0, 0.0, 0.0);
        normalFace[face] = 1.0;
        if(dot(normalFace, dir) < 0){
            normalFace[face] = -1.0;
        }
        vec2 minFace = vec2(minB[(face + 1) % 3], minB[(face + 2) % 3]);
        vec2 maxFace = vec2(maxB[(face + 1) % 3], maxB[(face + 2) % 3]);
        float aabbFaceArea = (maxFace.x - minFace.x) * (maxFace.y - minFace.y);
        float aabbFaceCosine = max(0.000001, abs(dot(dir, normalFace)));
        visibleAreas[face] = (aabbFaceCosine * aabbFaceArea);
    }
    float pick_face = rand() * (visibleAreas[0] + visibleAreas[1] + visibleAreas[2]);
    if(pick_face < visibleAreas[0])
        pointInLight.x = dir.x > 0 ? maxB.x : minB.x;
    else if(pick_face < visibleAreas[1])
        pointInLight.y = dir.y > 0 ? maxB.y : minB.y;
    else
        pointInLight.z = dir.z > 0 ? maxB.z : minB.z;
    return pointInLight;
}

vec3 random_on_sphere(vec3 center, float radius, vec3 origin){
    vec3 on_sphere = random_on_cosine_hemisphere(-1 * normalize(center - origin));
    return (center + radius * on_sphere);
}

float getSpherePdf(vec3 center, float radius, vec3 direction){
    float direction_length_squared = pow(length(direction), 2);
    float cos_theta_max = sqrt(1 - radius*radius / direction_length_squared);
    float solid_angle = 2*PI*(1-cos_theta_max);
    return  1 / solid_angle;
}

float getAABBPdf(vec3 minB, vec3 maxB, vec3 direction){
    float distanceToLight = length(direction);
    direction = normalize(direction);
    float distanceSquared = distanceToLight * distanceToLight;
    float visibleArea = 0.0;
    for(uint face = 0; face < 3; face++){
        vec3 normalFace = vec3(0.0, 0.0, 0.0);
        normalFace[face] = 1.0;
        if(dot(normalFace, direction) < 0){
            normalFace[face] = -1.0;
        }
        vec2 minFace = vec2(minB[(face + 1) % 3], minB[(face + 2) % 3]);
        vec2 maxFace = vec2(maxB[(face + 1) % 3], maxB[(face + 2) % 3]);
        float aabbFaceArea = (maxFace.x - minFace.x) * (maxFace.y - minFace.y);
        float aabbFaceCosine = max(0.000001, dot(direction, normalFace));
        visibleArea += (aabbFaceCosine * aabbFaceArea);
    }
    return distanceSquared / max(0.0001, visibleArea);
}

float getSpherePdf(vec3 center, float radius, vec3 origin, vec3 direction){
    float t;
    if (!intersectSphere(center, radius, origin, direction, t))
        return 0;

    float direction_length_squared = pow(length(center - origin), 2);
    float cos_theta_max = sqrt(1 - ((radius*radius) / direction_length_squared));
    float solid_angle = 2*PI*(1-cos_theta_max);
    return  1 / solid_angle;
}

float getAABBPdf(vec3 minB, vec3 maxB, vec3 origin, vec3 direction){
    float distanceToLight;
    if(intersectAABB(minB, maxB, origin, direction, distanceToLight)){
        float distanceSquared = distanceToLight * distanceToLight;
        float visibleArea = 0.0;
        for(uint face = 0; face < 3; face++){
            vec3 normalFace = vec3(0.0, 0.0, 0.0);
            normalFace[face] = 1.0;
            if(dot(normalFace, direction) < 0){
                normalFace[face] = -1.0;
            }
            vec2 minFace = vec2(minB[(face + 1) % 3], minB[(face + 2) % 3]);
            vec2 maxFace = vec2(maxB[(face + 1) % 3], maxB[(face + 2) % 3]);
            float aabbFaceArea = (maxFace.x - minFace.x) * (maxFace.y - minFace.y);
            float aabbFaceCosine = max(0.000001, dot(direction, normalFace));
            visibleArea += (aabbFaceCosine * aabbFaceArea);
        }
        return distanceSquared / max(0.0001, visibleArea);
    } else {
        return 0;
    }
}

float getCosinePdf(vec3 normal, vec3 direction) {
    return max(0.000001, dot(normal, direction) / PI);
}

float GGXNDF(vec3 N, vec2 alpha) {
    vec3 nSquare = (N * N) / vec3(alpha * alpha, 1);
    float nominator = PI * alpha.x * alpha.y * pow(nSquare.x + nSquare.y + nSquare.z, 2);
    return 1 / nominator;
}

float SmithG1(vec3 N, vec3 V, vec2 alpha) {
    vec3 w = N;
    vec3 h = abs(w.x) > 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 v = normalize(cross(w, h));
    vec3 u = cross(w, v);
    mat3 uvw = transpose(mat3(u,v,w));
    V = uvw * V;
    vec3 V2A2 = V * V * vec3(alpha * alpha, 1);
    float lambda = (-1 + sqrt(1+((V2A2.x + V2A2.y) / V2A2.z))) / 2;
    return 1 / (1 + lambda);
}

float SmithG1(vec3 V, vec2 alpha) {
    vec3 V2A2 = V * V * vec3(alpha * alpha, 1);
    float lambda = (-1 + sqrt(1+((V2A2.x + V2A2.y) / V2A2.z))) / 2;
    return 1 / (1 + lambda);
}

float GGXVNDF(vec3 N, vec3 V, vec2 alpha) {
    return (SmithG1(V, alpha) * max(0, dot(V, N)) * GGXNDF(N, alpha)) / dot(V, vec3(0, 0, 1));
}

float pdfGGX(vec3 N, vec3 NI, vec3 V, vec2 alpha) {
    vec3 w = N;
    vec3 h = abs(w.x) > 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 v = normalize(cross(w, h));
    vec3 u = cross(w, v);
    mat3 uvw = transpose(mat3(u,v,w));
    V = uvw * V;
    NI = uvw * NI;
    float GGXVNDF = GGXVNDF(NI, V, alpha);
    float JacobianR = 4 * dot(V,NI);
    return clamp(GGXVNDF / JacobianR, 0, 1);
}

vec3 sampleGGXVNDF(vec3 normal, vec3 V, vec2 alpha) {
    float U1 = rand();
    float U2 = rand();
    vec3 w = normal;
    vec3 h = abs(w.x) > 0.9 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 v = normalize(cross(w, h));
    vec3 u = cross(w, v);
    mat3 uvw = mat3(u,v,w);
    V = transpose(uvw) * V;
    // transforming the view direction to the hemisphere configuration 
    vec3 Vh = normalize(vec3(alpha.x * V.x, alpha.y * V.y, V.z)); 
    // orthonormal basis(with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y; 
    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x ,0) * inversesqrt(lensq): vec3(1, 0, 0);
    vec3 T2 = cross(Vh,T1); 
    // parameterization of the projected area 
    float r = sqrt(U1); 
    float phi = 2.0 * PI * U2; 
    float t1 = r * cos(phi); 
    float t2 = r * sin(phi); 
    float s = 0.5 * (1.0+Vh.z); 
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // reprojection on to hemisphere 
    vec3 Nh= t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // transforming the normal back to the ellipsoid configuration 
    vec3 Ne = normalize(vec3(alpha.x * Nh.x, alpha.y * Nh.y, max(0.0, Nh.z)));
    return uvw * Ne;
}

void schlick(const vec3 V, const vec3 N, const float ior, inout float kr, inout float kt){
    // entering medium
    float cosi = dot(-V, N);
    float n1 = 1.0;
    float n2 = ior;
    // leaving medium
    if(cosi < 0){
        n1 = ior;
        n2 = 1.0;
        cosi *= -1;
    }
    float F0 = pow(n2 - n1, 2)/pow(n2 + n1, 2);
    kr = F0 + (1 - F0) * pow(1 - cosi, 5);
    kt = 1.0 - kr;
}

vec3 refractRay(const vec3 I, const vec3 N, const float ior) { 
    float cosi = clamp(-1, 1, dot(I, N)); 
    float etai = 1;
    float etat = ior; 
    vec3 n = N; 
    if (cosi < 0) { 
        cosi = -cosi; 
    } else { 
        etai = ior;
        etat = 1; 
        n= -N; 
    } 
    float eta = etai / etat; 
    float k = 1 - eta * eta * (1 - cosi * cosi); 
    return k < 0 ? normalize(reflect(I, N)) : normalize(eta * I + (eta * cosi - sqrt(k)) * n);
}

void main()
{
    Material material = materials.m[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    const uint triIndex = material.indexOffset + gl_PrimitiveID * 3;
    Vertex TriVertices[3];
    for (uint i = 0; i < 3; i++) {
        uint index = material.vertexOffset + indices.i[triIndex + i];
        TriVertices[i] = vertices.v[index];
    }   
	vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
	vec2 uv = TriVertices[0].uv * barycentricCoords.x + TriVertices[1].uv * barycentricCoords.y + TriVertices[2].uv *  barycentricCoords.z;

    vec3 color = vec3(0.0);
    vec3 emission = vec3(0.0);

    // visualize proxy geometry
    // if(Payload.diffuseRecursion == 0 && Payload.translucentRecursion == 0){
    //     for(uint i = 0; i < uint(lights.l[0].radiosity); i++){
    //         Light light = lights.l[i];
    //         float dist;
    //         if(light.geoType == 0){
    //             if(intersectSphere(light.center, light.radius, gl_WorldRayOriginEXT, gl_WorldRayDirectionEXT, dist)){
    //                 Payload.color *= vec3(1.0, 0, 0);
    //                 Payload.continueTrace = false;
    //                 return;
    //             }
    //         } else {
    //             if(intersectAABB(light.min, light.max, gl_WorldRayOriginEXT, gl_WorldRayDirectionEXT, dist)){
    //                 Payload.color *= vec3(1.0, 0, 0);
    //                 Payload.continueTrace = false;
    //                 return;
    //             }
    //         }
    //     }
    // }

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
    mat4 modelMatrix = mat4(gl_ObjectToWorldEXT) * material.modelMatrix;
    mat4 normalToWorld = transpose(inverse(modelMatrix));
    // color
    color = material.baseColorFactor.xyz;
    if(material.baseColorTexture >= 0){
        color = texture(texSampler[material.baseColorTexture], uv).xyz;
    }

    //position
    vec3 position = (modelMatrix * vec4(TriVertices[0].pos * barycentricCoords.x + TriVertices[1].pos * barycentricCoords.y + TriVertices[2].pos * barycentricCoords.z, 1.0)).xyz;

    // normal
    vec3 normal = normalize((normalToWorld * vec4(normalize(TriVertices[0].normal * barycentricCoords.x + TriVertices[1].normal * barycentricCoords.y + TriVertices[2].normal * barycentricCoords.z), 1.0)).xyz);
    if(material.normalTexture >= 0){
        vec3 edge1 = TriVertices[1].pos - TriVertices[0].pos;
        vec3 edge2 = TriVertices[2].pos - TriVertices[0].pos;
        vec2 deltaUV1 = TriVertices[1].uv - TriVertices[0].uv;
        vec2 deltaUV2 = TriVertices[2].uv - TriVertices[0].uv;
        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        vec3 tangent = normalize(f * (deltaUV2.y * edge1 - deltaUV1.y * edge2));
        vec3 binormal = normalize(f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2));
        normal = normalize(mat3(tangent, binormal, normal) * (texture(texSampler[material.normalTexture], uv).xyz * 2.0 - 1.0));
    }

    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    float transmission = material.transmissionFactor;
    if(material.metallicRoughnessTexture >= 0){
        vec3 metallicroughness = texture(texSampler[material.metallicRoughnessTexture], uv).xyz;
        roughness = metallicroughness.y;
        metallic = metallicroughness.z;
    }

    // // debug normal
    // Payload.color = (normal + 1.0) / 2.0;
    // Payload.continueTrace = false;
    // return;
    vec3 newOrigin = position;
    
    // material is perfect mirror importance sampling can be skipped. Path weight stays the same.
    if(metallic > 0.01 && roughness < 0.005){
        Payload.diffuseRecursion += 1;
        Payload.color *= color;
        Payload.origin = newOrigin + normal * 0.0001;
        Payload.dir = reflect(gl_WorldRayDirectionEXT, normal);
        return;
    }
    // avoid roughness 0 because of floatingpoint precision
    roughness = max(0.01, roughness);

    vec3 newDir = vec3(0.0);
    float russianRoulette = rand();

    const uint maxLightCount = 30;
    float maxRad = 0.0;
    uint lightCandidates[maxLightCount];
    float lightRadApprox[maxLightCount];
    uint lightCount = 0;
    uint lightLength = uint(lights.l[0].radiosity);
    for(uint i = 0; i < lightLength && lightCount < maxLightCount; i++){
        Light light = lights.l[i];
        if(light.geoType < 2){
            vec3 center = light.center;
            if(light.geoType != 0){
                center = (light.min + light.max) / 2;
            }
            float radApprox = light.radiosity / pow(distance(center, newOrigin), 2);
            // only collect lightsources that have an estimated radiance above 0.01
            if(radApprox > settings.mips_sensitivity){
                maxRad += radApprox;
                lightCandidates[lightCount] = i;
                lightRadApprox[lightCount] = radApprox;
                lightCount++;
            }
        }
    }

    // calculate importance sampling strategy weights based on roughness
    vec3 sampleNormal = normal;
    vec2 roughness_alpha = vec2(roughness * roughness);
    float directLightImportance = roughness * 0.7 * min(1, maxRad);
    float brdfImportance = (1 - directLightImportance);
    if(lightCount < 1 || !settings.mips){
        brdfImportance = 1.0;
        directLightImportance = 0;
    }

    // get sample by importance
    int lightIndex = -1;
    float distanceToLight = -1;
    vec3 pointOnLight = vec3(0);
    float rprop = 0.0;
    float tprop = 0.0;
    schlick(gl_WorldRayDirectionEXT, normal, material.ior, rprop, tprop);
    float specularPart = 0.0;
    float diffusePart = 0.0;
    float transmissivePart = 0.0;
    float mat_pdf = 0.0;

    // get term weights
    if(metallic > 0.0001){
        specularPart = 1.0;
        transmissivePart = 0.0;
        diffusePart = 0.0;
    }else{
        if(roughness > 0.6999){
            specularPart = 0;
            transmissivePart = 0;
            diffusePart = 1.0;
        }else{
            specularPart = rprop;
            transmissivePart = tprop * transmission;
            diffusePart = tprop * (1-transmission);
        }
    }
    if(russianRoulette < specularPart){
        russianRoulette = russianRoulette / specularPart;
        if(dot(newDir, normal) < 0){
            normal = -normal;
        }
        if(metallic < 0.0001){
            color = vec3(1.0);
        }
        if(russianRoulette < directLightImportance){
            russianRoulette = russianRoulette / directLightImportance;
            uint index = 0;
            float lightWeight = 0.0;
            while(lightWeight < russianRoulette && index < lightCount && lightWeight < 1.00001){
                lightWeight += lightRadApprox[index] / maxRad;
                index++;
            }
            lightIndex = int(lightCandidates[index-1]);
            Light light = lights.l[lightIndex];
            if(light.geoType == 0){
                pointOnLight = random_on_sphere(light.center, light.radius, newOrigin);
            } else {
                pointOnLight = random_on_aabb(light.min, light.max, newOrigin);
            }
            newDir = normalize(pointOnLight - newOrigin);
            sampleNormal = normalize(newDir - gl_WorldRayDirectionEXT);
            Payload.diffuseRecursion += 1;
        }else{
            sampleNormal = sampleGGXVNDF(normal, -gl_WorldRayDirectionEXT, roughness_alpha);
            newDir = reflect(gl_WorldRayDirectionEXT, sampleNormal);
            Payload.diffuseRecursion += 1;
        }
        mat_pdf = pdfGGX(normal, sampleNormal, -gl_WorldRayDirectionEXT, roughness_alpha) * SmithG1(normal, newDir, roughness_alpha);
    } else if(russianRoulette < specularPart + transmissivePart) {
        russianRoulette = (russianRoulette - specularPart) / transmissivePart;
        if(russianRoulette < directLightImportance){
            russianRoulette = russianRoulette / directLightImportance;
            uint index = 0;
            float lightWeight = 0.0;
            while(lightWeight < russianRoulette && index < lightCount && lightWeight < 1.00001){
                lightWeight += lightRadApprox[index] / maxRad;
                index++;
            }
            lightIndex = int(lightCandidates[index-1]);
            Light light = lights.l[lightIndex];
            if(light.geoType == 0){
                pointOnLight = random_on_sphere(light.center, light.radius, newOrigin);
            } else {
                pointOnLight = random_on_aabb(light.min, light.max, newOrigin);
            }
            newDir = normalize(pointOnLight - newOrigin);
            sampleNormal = normalize(newDir - gl_WorldRayDirectionEXT);
            Payload.diffuseRecursion += 1;
        }else{
            sampleNormal = sampleGGXVNDF(normal, normal, roughness_alpha);
            newDir = refractRay(gl_WorldRayDirectionEXT, sampleNormal, material.ior);
            Payload.translucentRecursion += 1;
        }
        mat_pdf = pdfGGX(normal, sampleNormal, normal, roughness_alpha);
    } else {
        russianRoulette = (russianRoulette - specularPart - transmissivePart) / diffusePart;
        if(dot(newDir, normal) < 0){
            normal = -normal;
        }
        if(russianRoulette < directLightImportance){
            russianRoulette = russianRoulette / directLightImportance;
            uint index = 0;
            float lightWeight = 0.0;
            while(lightWeight < russianRoulette && index < lightCount && lightWeight < 1.00001){
                lightWeight += lightRadApprox[index] / maxRad;
                index++;
            }
            lightIndex = int(lightCandidates[index-1]);
            Light light = lights.l[lightIndex];
            if(light.geoType == 0){
                pointOnLight = random_on_sphere(light.center, light.radius, newOrigin);
            } else {
                pointOnLight = random_on_aabb(light.min, light.max, newOrigin);
            }
            newDir = normalize(pointOnLight - newOrigin);
            sampleNormal = normalize(newDir - gl_WorldRayDirectionEXT);
            Payload.diffuseRecursion += 1;
        }else{
            newDir = random_on_cosine_hemisphere(normal);
            sampleNormal = normalize(newDir - gl_WorldRayDirectionEXT);
            Payload.diffuseRecursion += 1;
        }
        mat_pdf = getCosinePdf(normal, newDir);
    }

    // get material pdf
    float sampling_material_pdf = mat_pdf;
    // get lights pdf
    float sampling_light_pdf = 0.0;
    // sum the pdfs of all collected lightsources
    if(directLightImportance > 0.0001){
        for(uint i = 0; i < lightCount; i++){
            uint index = lightCandidates[i];
            float radApprox = lightRadApprox[i];
            Light light = lights.l[index];
            float lightPdf = 0.0;
            if(light.geoType == 0){
                lightPdf = lightIndex == index ? getSpherePdf(light.center, light.radius, pointOnLight - newOrigin) : getSpherePdf(light.center, light.radius, newOrigin, newDir);
            } else {
                lightPdf = lightIndex == index ? getAABBPdf(light.min, light.max, pointOnLight - newOrigin) : getAABBPdf(light.min, light.max, newOrigin, newDir);
            }
            sampling_light_pdf += (radApprox/maxRad) * lightPdf;
        }
    }
    float sampling_pdf = brdfImportance * sampling_material_pdf + directLightImportance * sampling_light_pdf;
    if(dot(newDir, normal) < 0){
        newOrigin += 0.0001 * -normal;
    }else{
        newOrigin += 0.0001 * normal;
    }

    Payload.origin = newOrigin;
    Payload.dir = newDir;
    Payload.f *= mat_pdf;
    Payload.pdf *= sampling_pdf;
    Payload.color *= color;
}