#version 330 core

layout(location = 0) in vec2 position;

uniform vec3 posCenter;
uniform vec2 size;

out vec2 vertPos;

void main()
{
    gl_Position = vec4(
        position * size + posCenter.xy,
        posCenter.z,
        1.0
    );

    vertPos = position;
}