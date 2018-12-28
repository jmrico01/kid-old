#version 330 core

in vec2 fragUV;

out vec4 outColor;

uniform sampler2D framebufferTexture;
uniform vec2 invScreenSize;

#define EDGE_THRESHOLD_MIN 0.0312
#define EDGE_THRESHOLD_MAX 0.125

#define ITERATIONS 12
#define QUALITY(i) (quality_[i])
#define SUBPIXEL_QUALITY 0.75

const float quality_[ITERATIONS] = float[ITERATIONS](
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.5, 2.0, 2.0, 2.0, 4.0, 8.0
);

float ColorToLuminance(vec3 color) {
    return sqrt(dot(color, vec3(0.299, 0.587, 0.114)));
}

void main()
{
    // Wonderful, very complete FXAA algorithm. Source:
    // http://blog.simonrodriguez.fr/articles/30-07-2016_implementing_fxaa.html
    vec3 color = texture(framebufferTexture, fragUV).rgb;
    float lumMid = ColorToLuminance(color);
    /*float lumLeft = ColorToLuminance(textureOffset(framebufferTexture,
        fragUV, ivec2(-1, 0)).rgb);
    float lumRight = ColorToLuminance(textureOffset(framebufferTexture,
        fragUV, ivec2(1, 0)).rgb);
    float lumBot = ColorToLuminance(textureOffset(framebufferTexture,
        fragUV, ivec2(0, -1)).rgb);
    float lumTop = ColorToLuminance(textureOffset(framebufferTexture,
        fragUV, ivec2(0, 1)).rgb);

    float lumMin = min(lumMid,
        min(min(lumLeft, lumRight), min(lumBot, lumTop)));
    float lumMax = max(lumMid,
        max(max(lumLeft, lumRight), max(lumBot, lumTop)));
    float lumDelta = lumMax - lumMin;*/

    float lumBotLeft = ColorToLuminance(textureOffset(framebufferTexture,
        fragUV, ivec2(-1, -1)).rgb);
    float lumBotRight = ColorToLuminance(textureOffset(framebufferTexture,
        fragUV, ivec2(1, -1)).rgb);
    float lumTopLeft = ColorToLuminance(textureOffset(framebufferTexture,
        fragUV, ivec2(-1, 1)).rgb);
    float lumTopRight = ColorToLuminance(textureOffset(framebufferTexture,
        fragUV, ivec2(1, 1)).rgb);

    float lumMin = min(lumMid,
        min(min(lumTopLeft, lumTopRight), min(lumBotLeft, lumBotRight)));
    float lumMax = max(lumMid,
        max(max(lumTopLeft, lumTopRight), max(lumBotLeft, lumBotRight)));
    //float lumDelta = lumMax - lumMin;

    // other
    #define FXAA_REDUCE_MIN   (1.0 / 128.0)
    #define FXAA_REDUCE_MUL   (1.0 / 8.0)
    #define FXAA_SPAN_MAX     8.0
    
    vec2 dir;
    dir.x = -((lumTopLeft + lumTopRight) - (lumBotLeft + lumBotRight));
    dir.y =  ((lumTopLeft + lumBotLeft) - (lumTopRight + lumBotRight));
    
    float dirReduce = max(
        (lumTopLeft + lumTopRight + lumBotLeft + lumBotRight)
        * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
              max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
              dir * rcpDirMin)) * invScreenSize;
    
    vec3 rgbA = 0.5 * (
        texture(framebufferTexture, fragUV + dir * (1.0 / 3.0 - 0.5)).xyz +
        texture(framebufferTexture, fragUV + dir * (2.0 / 3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(framebufferTexture, fragUV + dir * -0.5).xyz +
        texture(framebufferTexture, fragUV + dir * 0.5).xyz);

    float lumB = ColorToLuminance(rgbB);
    if ((lumB < lumMin) || (lumB > lumMax))
        outColor = vec4(rgbA, 1.0);
    else
        outColor = vec4(rgbB, 1.0);
}