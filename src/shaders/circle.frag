#version 330 core

in vec2 vertPos;

out vec4 outColor;

uniform vec4 color;

#define BORDER_FRAC_SIZE 0.025
#define BORDER_FRAC_START_SQ ((0.5 - BORDER_FRAC_SIZE) \
    * (0.5 - BORDER_FRAC_SIZE))

void main()
{
    // NOTE vertex positions are [-0.5, 0.5]
    float distSq = vertPos.x * vertPos.x + vertPos.y * vertPos.y;
    if (distSq < BORDER_FRAC_START_SQ) {
        outColor = color;
    }
    else if (distSq < 0.5 * 0.5) {
        float opaqueness = (distSq - BORDER_FRAC_START_SQ)
            / (0.5 * 0.5 - BORDER_FRAC_START_SQ);
        outColor = color * (1.0 - opaqueness);
    }
    else {
    	outColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
}