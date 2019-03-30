#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;

out vec2 fragUV;

uniform mat4 transform;
uniform vec4 uvInfo;

uniform mat4 batchTransform;

void main()
{
	fragUV = uvInfo.xy + uv * uvInfo.zw;
	gl_Position = batchTransform * transform * vec4(position.x, position.y, 0.0, 1.0);
}