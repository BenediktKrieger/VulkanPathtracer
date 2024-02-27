#version 460

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inColor;
layout (location = 4) in vec4 inJoint;
layout (location = 5) in vec4 inWeight;
layout (location = 6) in vec4 inTangent;

layout( push_constant ) uniform constants
{
	mat4 proj;
	mat4 view;
	mat4 model;
	uint accumulatedFrames;
} PushConstants;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 outColor;
layout (location = 4) out vec4 outJoint;
layout (location = 5) out vec4 outWeight;
layout (location = 6) out vec3 outTangent;

void main()
{
	gl_Position = PushConstants.proj * PushConstants.view * PushConstants.model * vec4(inPosition, 1.0f);
	vec3 normal = normalize((transpose(inverse(PushConstants.model)) * vec4(inNormal, 1.0)).xyz);
	vec3 tangent = normalize(transpose(inverse(PushConstants.model)) * inTangent).xyz;
	outPosition = inPosition;
	outNormal =  normal;
	outUV = inUV;
	outColor = inColor;
	outJoint = inJoint;
	outWeight = inWeight;
	outTangent = tangent;
}