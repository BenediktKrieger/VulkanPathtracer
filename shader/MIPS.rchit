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
layout(binding = 3, set = 0) buffer Indices { uint i[]; } indices;
layout(binding = 4, set = 0) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 5, set = 0) buffer Materials { Material m[]; } materials;
layout(binding = 6, set = 0) buffer Lights { Light l[]; } lights;
layout(binding = 8, set = 0) uniform Settings {
    bool accumulate;
	uint samples;
	uint reflection_recursion;
	uint refraction_recursion;
    float ambient_multiplier;
} settings;
layout(binding = 9, set = 0) uniform sampler2D texSampler[];

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
        float aabbFaceCosine = max(0.000001, abs(dot(direction, normalFace)));
        visibleArea += (aabbFaceCosine * aabbFaceArea);
    }
    return distanceSquared / max(0.0001, visibleArea);
}

float getSpherePdf(vec3 center, float radius, vec3 origin, vec3 direction){
    float t;
    if (!intersectSphere(center, radius, origin, direction, t))
        return 0;

    float direction_length_squared = pow(length(center - origin), 2);
    float cos_theta_max = sqrt(1 - radius*radius / direction_length_squared);
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
            float aabbFaceCosine = max(0.000001, abs(dot(direction, normalFace)));
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

void fresnel(const vec3 V, const vec3 N, const float ior, inout float kr, inout float kt){ 
  float cosi = clamp(-1, 1, dot(V, N)); 
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
    vec3 normal_geometric = normalize(TriVertices[0].normal * barycentricCoords.x + TriVertices[1].normal * barycentricCoords.y + TriVertices[2].normal * barycentricCoords.z);
    vec3 color = vec3(0.0);
    vec3 emission = vec3(0.0);

    // for(uint i = 0; i < lights.l.length(); i++){
    //     Light light = lights.l[i];
    //     float dist;
    //     if(light.geoType == 0){
    //         if(intersectSphere(light.center, light.radius, gl_WorldRayOriginEXT, gl_WorldRayDirectionEXT, dist)){
    //             Payload.color *= vec3(1.0, 0, 0);
    //             Payload.continueTrace = false;
    //             return;
    //         }
    //     } else {
    //         if(intersectAABB(light.min, light.max, gl_WorldRayOriginEXT, gl_WorldRayDirectionEXT, dist)){
    //             Payload.color *= vec3(1.0, 0, 0);
    //             Payload.continueTrace = false;
    //             return;
    //         }
    //     }
    // }

    // if(material.emissiveTexture >= 0){
    //     vec3 emissiveFactor = texture(texSampler[material.emissiveTexture], uv).rgb;
    //     if(emissiveFactor.x > 0.5 || emissiveFactor.y > 0.5 || emissiveFactor.z > 0.5)
    //     Payload.color *= vec3(1.0, 0, 0);
    //     Payload.continueTrace = false;
    //     return;
    // }

    // check for emission of hit
    if(material.emissiveStrength > 1.0 || material.emissiveTexture >= 0){
        if(material.emissiveTexture >= 0){
            vec3 emissiveFactor = texture(texSampler[material.emissiveTexture], uv).xyz;
            emission = vec3(material.emissiveStrength) * emissiveFactor;
            if(emissiveFactor.x > 0.1 || emissiveFactor.y > 0.1 || emissiveFactor.z > 0.1){
                //float lvk_weight = max(0, dot(normal_geometric, -gl_WorldRayDirectionEXT));
                Payload.color *= emission;
                Payload.continueTrace = false;
                return;
            }
        } else {
            emission = vec3(material.emissiveStrength) * material.emissiveFactor.xyz;
            //float lvk_weight = max(0, dot(normal_geometric, -gl_WorldRayDirectionEXT));
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
        float russianRoulette = rand();

        const uint maxLightCount = 20;
        uint lightCandidates[maxLightCount];
        uint lightCount = 0;
        uint lightLength = uint(lights.l[0].radiosity);
        for(uint i = 0; i < lightLength && lightCount < maxLightCount; i++){
            Light light = lights.l[i];
            if(light.geoType < 3){
                vec3 center = light.center;
                if(light.geoType != 0){
                    center = (light.min + light.max) / 2;
                }
                float radApprox = light.radiosity / pow(distance(center, newOrigin), 2);
                if(radApprox > 0.3){
                    lightCandidates[lightCount] = i;
                    lightCount++;
                }
            }
        }
        
        vec3 sampleNormal = normal;
        float roughness = material.roughnessFactor;
        vec2 alpha = vec2(roughness * roughness);
        float directLightImportance = roughness * 0.7;
        float brdfImportance = (1 - directLightImportance);
        if(lightCount < 1){
            brdfImportance = 1.0;
            directLightImportance = 0;
        }
        float singleLightImportance = directLightImportance / lightCount;

        // get sample by importance
        int lighIndex = -1;
        float distanceToLight = -1;
        vec3 pointOnLight = vec3(0);
        if(russianRoulette < directLightImportance){
            //sample lights
            uint index = uint(floor(russianRoulette / singleLightImportance));
            lighIndex = int(lightCandidates[index]);
            Light light = lights.l[lighIndex];
            if(light.geoType == 0){
                pointOnLight = random_on_sphere(light.center, light.radius, newOrigin);
            } else {
                pointOnLight = random_on_aabb(light.min, light.max, newOrigin);
            }
            newDir = normalize(pointOnLight - newOrigin);
            sampleNormal = normalize(newDir - gl_WorldRayDirectionEXT);
        } else {
            // sample material-brdf
            if((russianRoulette - directLightImportance) < (brdfImportance * 0.5))
                newDir = random_on_cosine_hemisphere(normal);
            else{
                sampleNormal = sampleGGXVNDF(normal, -gl_WorldRayDirectionEXT, alpha);
                newDir = reflect(gl_WorldRayDirectionEXT, sampleNormal);
            }
        }

        // get material-brdf pdf
        float mat_specular_pdf = pdfGGX(normal, sampleNormal, -gl_WorldRayDirectionEXT, alpha);
        float mat_diffuse_pdf = getCosinePdf(normal, newDir);
        float mat_pdf =  (0.5 * mat_specular_pdf + 0.5 * mat_diffuse_pdf);

        // get sample pdf
        float sampling_pdf = brdfImportance * mat_pdf;
        if(directLightImportance > 0.0001){
            for(uint i = 0; i < lightCount; i++){
                uint index = lightCandidates[i];
                Light light = lights.l[index];
                float lightPdf = 0.0;
                if(light.geoType == 0){
                    lightPdf = lighIndex == index ? getSpherePdf(light.center, light.radius, pointOnLight - newOrigin) : getSpherePdf(light.center, light.radius, newOrigin, newDir);
                } else {
                    lightPdf = lighIndex == index ? getAABBPdf(light.min, light.max, pointOnLight - newOrigin) : getAABBPdf(light.min, light.max, newOrigin, newDir);
                }
                sampling_pdf += singleLightImportance * lightPdf;
            }
        }

        Payload.diffuseRecursion += 1;
        Payload.origin = newOrigin;
        Payload.dir = newDir;
        Payload.f *= mat_pdf;
        Payload.pdf *= sampling_pdf;
    }
    Payload.color *= color;
}