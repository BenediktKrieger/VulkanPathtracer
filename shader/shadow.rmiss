#version 460
#extension GL_EXT_ray_tracing : require

struct RayPayload {
	vec3 color;
  vec3 attenuation;
  vec3 origin;
  vec3 dir;
	uint recursion;
  bool shadow;
};

layout(location = 0) rayPayloadInEXT RayPayload Payload;

void main()
{
	Payload.shadow = false;
}