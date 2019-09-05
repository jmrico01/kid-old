#pragma once

#include "opengl.h"

// These must match the max sizes in blur.frag
#define KERNEL_HALFSIZE_MAX 10
#define KERNEL_SIZE_MAX (KERNEL_HALFSIZE_MAX * 2 + 1)

void PostProcessBloom(Framebuffer framebufferIn,
	Framebuffer framebufferScratch, Framebuffer framebufferOut,
	GLuint screenQuadVertexArray,
	GLuint extractShader, GLuint blurShader, GLuint blendShader);

void PostProcessGrain(Framebuffer framebufferIn, Framebuffer framebufferOut,
	GLuint screenQuadVertexArray, GLuint shader, float32 grainTime);

void PostProcessLUT(Framebuffer framebufferIn, Framebuffer framebufferOut,
	GLuint screenQuadVertexArray, GLuint shader, TextureGL lut);
