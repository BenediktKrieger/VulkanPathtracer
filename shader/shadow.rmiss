#version 460
#extension GL_EXT_ray_tracing : require

struct ShadowRayPayload {
	bool shadow;
};

layout(location = 1) rayPayloadInEXT ShadowRayPayload ShadowPayload;

void main()
{
	ShadowPayload.shadow = false;
}