#version 460
#extension GL_EXT_ray_tracing : enable

struct Hit {
	vec3 color;
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

layout(location = 0) rayPayloadInEXT RayPayload Payload;
layout(binding = 7, set = 0) uniform sampler2D hdrMapSampler;
layout(binding = 8, set = 0) uniform Settings {
    bool accumulate;
    uint samples;
    uint reflection_recursion;
    uint refraction_recursion;
    float ambient_multiplier;
} settings;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 direction)
{
    vec2 uv = vec2(atan(direction.z, direction.x), asin(direction.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec3 unit_direction = gl_WorldRayDirectionEXT;
    vec3 color = texture(hdrMapSampler, SampleSphericalMap(unit_direction)).xyz * settings.ambient_multiplier;
    Payload.color *= color;
    Payload.continueTrace = false;
}