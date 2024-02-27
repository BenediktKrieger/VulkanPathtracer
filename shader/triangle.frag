#version 460

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inColor;
layout (location = 4) in vec4 inJoint;
layout (location = 5) in vec4 inWeight;
layout (location = 6) in vec3 inTangent;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	//return normal
	vec3 normal = (inNormal + 1) / 2;
	vec3 tangent = (inTangent + 1) / 2;
	outFragColor = vec4(normal, 1.0);
}