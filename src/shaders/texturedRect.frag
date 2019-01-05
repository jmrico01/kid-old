#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D textureSampler;

void main()
{
	vec4 texColor = texture(textureSampler, fragUV);
	vec3 premultiplied = texColor.rgb * texColor.a + vec3(1.0f, 1.0f, 1.0f) * (1.0 - texColor.a);
	outColor = vec4(premultiplied, 1.0f);
}