#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D framebufferTexture;
uniform sampler2D lut4k;

void main()
{
    vec3 color = texture(framebufferTexture, fragUV).rgb;
    int r = int(color.r * 255);
    int g = int(color.g * 255);
    int b = int(color.b * 255);
    int quadX = b % 16;
    int quadY = b / 16;
    vec2 lutUV = vec2(
        (quadX * 256 + r) / 4095.0f,
        (quadY * 256 + g) / 4095.0f
    );
    lutUV.y = 1.0f - lutUV.y;
    vec3 lutColor = texture(lut4k, lutUV).rgb;
    outColor = vec4(lutColor, 1.0);
}