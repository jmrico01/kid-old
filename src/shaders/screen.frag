#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D framebufferTexture;

void main()
{
    vec3 color = texture(framebufferTexture, fragUV).rgb;
    outColor = vec4(color, 1.0);
}