//glsl version 4.5
#version 450

layout (location = 0) in vec4 inColor;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	//return red
	outFragColor = vec4(inColor);
}