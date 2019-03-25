#include "post.h"

void PostProcessBloom(Framebuffer framebufferIn,
	Framebuffer framebufferScratch, Framebuffer framebufferOut,
	GLuint screenQuadVertexArray,
	GLuint extractShader, GLuint blurShader, GLuint blendShader)
{
	float32 bloomThreshold = 0.5f;
	int bloomKernelHalfSize = 4;
	int bloomKernelSize = bloomKernelHalfSize * 2 + 1;
	int bloomBlurPasses = 1;
	float32 bloomBlurSigma = 2.0f;
	float32 bloomMag = 0.5f;
	// Extract high-luminance pixels
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferScratch.framebuffer);
	//glClear(GL_COLOR_BUFFER_BIT);

	glBindVertexArray(screenQuadVertexArray);
	glUseProgram(extractShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, framebufferIn.color);
	GLint loc = glGetUniformLocation(extractShader, "framebufferTexture");
	glUniform1i(loc, 0);
	loc = glGetUniformLocation(extractShader, "threshold");
	glUniform1f(loc, bloomThreshold);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	// Blur high-luminance pixels
	GLfloat gaussianKernel[KERNEL_SIZE_MAX];
	GLfloat kernSum = 0.0f;
	float32 sigma = bloomBlurSigma;
	for (int i = -bloomKernelHalfSize; i <= bloomKernelHalfSize; i++) {
		float32 x = (float32)i;
		float32 g = expf(-(x * x) / (2.0f * sigma * sigma));
		gaussianKernel[i + bloomKernelHalfSize] = (GLfloat)g;
		kernSum += (GLfloat)g;
	}
	for (int i = 0; i < bloomKernelSize; i++) {
		gaussianKernel[i] /= kernSum;
	}
	for (int i = 0; i < bloomBlurPasses; i++) {  
		// Horizontal pass
		glBindFramebuffer(GL_FRAMEBUFFER, framebufferOut.framebuffer);
		//glClear(GL_COLOR_BUFFER_BIT);

		glBindVertexArray(screenQuadVertexArray);
		glUseProgram(blurShader);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, framebufferScratch.color);
		loc = glGetUniformLocation(blurShader, "framebufferTexture");
		glUniform1i(loc, 0);
		loc = glGetUniformLocation(blurShader, "isHorizontal");
		glUniform1i(loc, 1);
		loc = glGetUniformLocation(blurShader, "gaussianKernel");
		glUniform1fv(loc, bloomKernelSize, gaussianKernel);
		loc = glGetUniformLocation(blurShader, "kernelHalfSize");
		glUniform1i(loc, bloomKernelHalfSize);

		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Vertical pass
		glBindFramebuffer(GL_FRAMEBUFFER, framebufferScratch.framebuffer);
		//glClear(GL_COLOR_BUFFER_BIT);

		glBindVertexArray(screenQuadVertexArray);
		glUseProgram(blurShader);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, framebufferOut.color);
		loc = glGetUniformLocation(blurShader, "framebufferTexture");
		glUniform1i(loc, 0);
		loc = glGetUniformLocation(blurShader, "isHorizontal");
		glUniform1i(loc, 0);
		loc = glGetUniformLocation(blurShader, "gaussianKernel");
		glUniform1fv(loc, bloomKernelSize, gaussianKernel);
		loc = glGetUniformLocation(blurShader, "kernelHalfSize");
		glUniform1i(loc, bloomKernelHalfSize);

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	// Blend scene with blurred bright pixels
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferOut.framebuffer);
	//glClear(GL_COLOR_BUFFER_BIT);

	glBindVertexArray(screenQuadVertexArray);
	glUseProgram(blendShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, framebufferIn.color);
	loc = glGetUniformLocation(blendShader, "scene");
	glUniform1i(loc, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, framebufferScratch.color);
	loc = glGetUniformLocation(blendShader, "bloomBlur");
	glUniform1i(loc, 1);
	loc = glGetUniformLocation(blendShader, "bloomMag");
	glUniform1f(loc, bloomMag);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PostProcessGrain(Framebuffer framebufferIn, Framebuffer framebufferOut,
    GLuint screenQuadVertexArray, GLuint shader, float32 grainTime)
{
    float32 grainMag = 0.2f;
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferOut.framebuffer);
    //glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(screenQuadVertexArray);
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, framebufferIn.color);
    GLint loc = glGetUniformLocation(shader, "scene");
    glUniform1i(loc, 0);
    loc = glGetUniformLocation(shader, "grainMag");
    glUniform1f(loc, grainMag);
    loc = glGetUniformLocation(shader, "time");
    glUniform1f(loc, grainTime * 100003.0f);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PostProcessLUT(Framebuffer framebufferIn, Framebuffer framebufferOut,
    GLuint screenQuadVertexArray, GLuint shader, TextureGL lut)
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferOut.framebuffer);

    glBindVertexArray(screenQuadVertexArray);
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, framebufferIn.color);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, lut.textureID);

    GLint loc = glGetUniformLocation(shader, "framebufferTexture");
    glUniform1i(loc, 0);
    loc = glGetUniformLocation(shader, "lut4k");
    glUniform1i(loc, 1);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}