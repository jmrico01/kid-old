#version 330 core

in vec3 posWorldSpace;
in vec3 normalWorldSpace;

layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec3 gColor;

uniform vec4 color;

void main()
{
    gPosition = posWorldSpace;
    gNormal = normalize(normalWorldSpace);
    gColor = color.xyz;
}