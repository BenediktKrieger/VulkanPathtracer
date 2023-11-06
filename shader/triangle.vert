#version 450

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
layout (location = 3) in vec4 vColor;
layout (location = 4) in vec4 vJoint;
layout (location = 5) in vec4 vWeight;
layout (location = 6) in vec4 vTangent;

layout (location = 0) out vec4 outColor;

void main()
{
	gl_Position = vec4(vPosition, 1.0f);
	outColor = vColor;
}