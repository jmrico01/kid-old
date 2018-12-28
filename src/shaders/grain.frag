#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D scene;
uniform sampler2D noiseTex;
uniform float grainMag;
uniform float time;

float rand(vec2 n) { 
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float noise(vec2 p){
    vec2 ip = floor(p);
    vec2 u = fract(p);
    u = u*u*(3.0-2.0*u);
    
    float res = mix(
        mix(rand(ip),rand(ip+vec2(1.0,0.0)),u.x),
        mix(rand(ip+vec2(0.0,1.0)),rand(ip+vec2(1.0,1.0)),u.x),u.y);
    return res*res;
}

void main()
{
    vec2 randVec = vec2(
        mod(time * fragUV.x, 701.0),
        mod(time * fragUV.y, 373.0)
    );
    vec3 color = texture(scene, fragUV).rgb;
    vec3 noiseColor = color;
    color *= 1.0 - grainMag;
    noiseColor *= grainMag * noise(randVec * 100.0f);
    outColor = vec4(color + noiseColor, 1.0);
}