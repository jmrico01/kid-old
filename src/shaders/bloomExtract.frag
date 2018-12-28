#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D framebufferTexture;
uniform float threshold;

float ColorToLuminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main()
{
    vec3 color = texture(framebufferTexture, fragUV).rgb;
    float lum = ColorToLuminance(color);
    if (lum >= threshold) {
        outColor = vec4(color, 1.0);
    }
    else {
        // NOTE unexpected cool effect:
        //  set alpha value here to 0.0, and the bloom values
        //  get preserved through frames
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}