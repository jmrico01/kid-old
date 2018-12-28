#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;

out vec2 fragUV;

uniform vec3 posBottomLeft;
uniform vec2 size;

void main()
{
    fragUV = uv;
    gl_Position = vec4(
        position.xy * size + posBottomLeft.xy,
        posBottomLeft.z,
        1.0);
}