#version 330 core

layout(location = 0) in vec2 position;

uniform vec3 posBottomLeft;
uniform vec2 size;

void main()
{
    gl_Position = vec4(
        position.xy * size + posBottomLeft.xy,
        posBottomLeft.z,
        1.0);
}