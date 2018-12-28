#version 330 core

#define KERNEL_SIZE 64

in vec2 fragUV;

out float outFloat;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D noise;

uniform vec3 samples[KERNEL_SIZE];

uniform mat4 view;
uniform mat4 proj;

uniform vec2 noiseScale; // Screen size divided by noise texture size
uniform float radius;
uniform float bias;

void main()
{
    vec3 pos = texture(gPosition, fragUV).rgb;
    pos = (view * vec4(pos, 1.0)).xyz;
    vec3 normal = texture(gNormal, fragUV).rgb;
    normal = (view * vec4(normal, 0.0)).xyz;
    vec3 randomVec = texture(noise, fragUV * noiseScale).xyz;

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < KERNEL_SIZE; i++) {
        vec3 sample = tbn * samples[i];
        sample = pos + sample * radius;

        vec4 offset = vec4(sample, 1.0);
        offset = proj * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        vec3 samplePos = texture(gPosition, offset.xy).xyz;
        samplePos = (view * vec4(samplePos, 1.0)).xyz;
        float sampleDepth = samplePos.z;

        float rangeCheck = smoothstep(0.0, 1.0,
            radius / abs(pos.z - sampleDepth));
        occlusion += (sampleDepth >= sample.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / KERNEL_SIZE);

    outFloat = occlusion;
}