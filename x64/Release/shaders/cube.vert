#version 460 core

layout(std140, binding = 1) uniform transform
{
	mat4 MVP;
} Transform;

layout(location = 0) in vec4 position;

out block
{
	vec4 color;
} Out;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	gl_Position = Transform.MVP * position;
	Out.color = position + 0.1;
}