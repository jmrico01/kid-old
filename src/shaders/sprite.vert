#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;

layout(location = 2) in vec3 posBottomLeft;
layout(location = 3) in vec2 size;
layout(location = 4) in vec4 uvInfo;

out vec2 fragUV;

void main()
{
	fragUV = uvInfo.xy + uv * uvInfo.zw;
	gl_Position = vec4(
		position.xy * size + posBottomLeft.xy,
		posBottomLeft.z,
		1.0
	);
}