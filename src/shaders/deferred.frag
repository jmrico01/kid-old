#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gColor;
uniform sampler2D ssao;

uniform vec3 lightDirWorldSpace;

void main()
{
    vec3 pos = texture(gPosition, fragUV).rgb;
    vec3 normal = texture(gNormal, fragUV).rgb;
    vec3 color = texture(gColor, fragUV).rgb;
    float occlusion = texture(ssao, fragUV).r;

    vec3 ambientColor = vec3(0.6, 0.6, 0.6) * occlusion;
    vec3 lightColor = vec3(0.3, 0.3, 0.3);

    vec3 lightDir = normalize(lightDirWorldSpace);
    float cosTheta = clamp(dot(normal, -lightDir), 0.0, 1.0);
    vec3 lightingTotal = ambientColor + lightColor * cosTheta;

    outColor = vec4(lightingTotal * color, 1.0);
}