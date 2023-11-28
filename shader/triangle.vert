#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
layout (location = 3) in vec4 vColor;
layout (location = 4) in vec4 vJoint;
layout (location = 5) in vec4 vWeight;
layout (location = 6) in vec4 vTangent;

layout( push_constant ) uniform constants
{
	mat4 proj;
	mat4 view;
	mat4 model;
	uint accumulatedFrames;
} PushConstants;

layout (location = 0) out vec3 outNormal;

void main()
{
	gl_Position = PushConstants.proj * PushConstants.view * PushConstants.model * vec4(vPosition, 1.0f);
	outNormal = vec3(vNormal);
}