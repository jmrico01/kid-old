#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;

out vec2 fragUV;

uniform vec3 posBottomLeft;
uniform vec2 size;
uniform bool flipHorizontal;

void main()
{
	fragUV = uv;
	if (flipHorizontal) {
		fragUV.x = 1.0 - fragUV.x;
	}
	
	gl_Position = vec4(
		position.xy * size + posBottomLeft.xy,
		posBottomLeft.z,
		1.0
	);
}