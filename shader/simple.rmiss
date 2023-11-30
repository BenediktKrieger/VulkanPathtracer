#version 460
#extension GL_EXT_ray_tracing : enable

struct RayPayload {
    vec3 color;
    uint recursion;
    float weight;
    vec3 origin;
    vec3 dir;
};

layout(location = 0) rayPayloadInEXT RayPayload Payload;



void main()
{
    vec3 unit_direction = gl_WorldRayDirectionEXT;
    float a = 0.5 * (unit_direction.y + 1.0);
    vec3 color = (1.0 - a)* vec3(1.0, 0.5, 0.0) + a * vec3(0.5, 0.7, 1.0);

    Payload.color = (1-Payload.weight) * Payload.color + Payload.weight * (color * 2);
    Payload.weight = 0.0;
    Payload.recursion = 1000;
}