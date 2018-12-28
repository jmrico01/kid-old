#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 model;
uniform mat4 vp;

out vec3 posWorldSpace;
out vec3 normalWorldSpace;

void main()
{
    vec4 worldPos = model * vec4(position, 1.0);
    gl_Position = vp * worldPos;
    
    posWorldSpace = worldPos.xyz;
    normalWorldSpace = (model * vec4(normal, 0.0)).xyz;
}