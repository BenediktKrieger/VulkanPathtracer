#version 460
#extension GL_EXT_ray_tracing : require

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

void main()
{
	Payload.shadow = false;
}