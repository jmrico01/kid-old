#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 mvp;
uniform mat4 model;
uniform mat4 view;
uniform vec3 lightDir;

out vec3 normalCamSpace;
out vec3 lightDirCamSpace;

void main()
{
    gl_Position = mvp * vec4(position, 1.0);
    normalCamSpace = (view * model * vec4(normal, 0.0)).xyz;
    lightDirCamSpace = (view * model * vec4(lightDir, 0.0)).xyz;
}