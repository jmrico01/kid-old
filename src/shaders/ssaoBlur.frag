#version 330 core

#define BLUR_MAX_DEPTH 0.05

in vec2 fragUV;

out float outFloat;

uniform sampler2D ssao;
uniform sampler2D gPosition;

uniform mat4 view;

void main()
{
    vec3 posView = texture(gPosition, fragUV).rgb;
    posView = (view * vec4(posView, 1.0)).xyz;

    vec2 texelSize = 1.0 / vec2(textureSize(ssao, 0));
    float result = 0.0;
    int count = 0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec3 posOffsetView = texture(gPosition, fragUV + offset).xyz;
            posOffsetView = (view * vec4(posOffsetView, 1.0)).xyz;
            if (abs(posView.z - posOffsetView.z) < BLUR_MAX_DEPTH) {
                result += texture(ssao, fragUV + offset).r;
                count++;
            }
        }
    }

    outFloat = result / count;
}