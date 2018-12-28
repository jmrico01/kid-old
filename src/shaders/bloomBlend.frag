#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform float bloomMag;

void main()
{
    vec3 color = texture(scene, fragUV).rgb;
    vec3 bloomColor = texture(bloomBlur, fragUV).rgb;
    outColor = vec4(color + bloomMag * bloomColor, 1.0);
}