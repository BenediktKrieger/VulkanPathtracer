#version 460
#extension GL_EXT_ray_tracing : enable

struct Hit {
	vec3 color;
};

struct RayPayload {
	Hit path[6];
	vec3 origin;
	vec3 dir;
	uint recursion;
	bool trace;
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
    Payload.path[Payload.recursion].color = color;
    Payload.recursion += 1;
    Payload.trace = false;
}