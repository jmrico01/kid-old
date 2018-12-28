#version 330 core

#define KERNEL_HALFSIZE_MAX 10
#define KERNEL_SIZE_MAX (KERNEL_HALFSIZE_MAX * 2 + 1)

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D framebufferTexture;
uniform int isHorizontal;
uniform float gaussianKernel[KERNEL_SIZE_MAX];
uniform int kernelHalfSize;

void main()
{
    vec2 texOffset = 1.0 / textureSize(framebufferTexture, 0);
    vec2 step = vec2(texOffset.x, 0.0);
    if (isHorizontal == 0) {
        step = vec2(0.0, texOffset.y);
    }

    vec3 color = vec3(0.0, 0.0, 0.0);
    for (int i = -kernelHalfSize; i <= kernelHalfSize; i++) {
        vec3 c = texture(framebufferTexture, fragUV + step * i).rgb;
        color += c * gaussianKernel[i + kernelHalfSize];
    }

    outColor = vec4(color, 1.0);
}