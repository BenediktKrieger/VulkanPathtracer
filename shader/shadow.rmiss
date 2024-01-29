#version 460
#extension GL_EXT_ray_tracing : require

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

void main()
{
	Payload.shadow = false;
}