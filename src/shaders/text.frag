#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D textureSampler;
uniform vec4 color;

void main()
{
	outColor = vec4(1.0, 1.0, 1.0,
		texture(textureSampler, fragUV).r) * color;
}