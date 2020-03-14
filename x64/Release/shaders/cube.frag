#version 460 core

in block
{
	vec4 color;
} In;

layout(location = 0) out vec4 color;

void main()
{
	color = abs(In.color);
}