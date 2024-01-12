#version 460
#extension GL_EXT_ray_tracing : enable

struct Hit {
	vec3 color;
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

layout(location = 0) rayPayloadInEXT RayPayload Payload;
layout(binding = 6, set = 0) uniform sampler2D hdrMapSampler;

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
    vec3 color = texture(hdrMapSampler, SampleSphericalMap(unit_direction)).xyz;
    Payload.color *= color;
    Payload.continueTrace = false;
}