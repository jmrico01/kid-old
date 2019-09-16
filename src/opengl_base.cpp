#include "opengl_base.h"

#include <km_common/km_debug.h>
#include <km_common/km_defines.h>
#include <km_common/km_math.h>
//#include <ft2build.h>
//#include FT_FREETYPE_H
#include <stdlib.h>

#include "opengl_funcs.h"
#include "opengl.h"

#define OGL_INFO_LOG_LENGTH_MAX 512

internal bool CompileAndCheckShader(GLuint shaderID, PlatformReadFileResult shaderFile)
{
	// Compile shader
	GLint shaderFileSize = (GLint)shaderFile.size;
	glShaderSource(shaderID, 1, (const GLchar* const*)&shaderFile.data,
		&shaderFileSize);
	glCompileShader(shaderID);

	// Check shader
	GLint result;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		char infoLog[OGL_INFO_LOG_LENGTH_MAX];
		int infoLogLength;
		glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > OGL_INFO_LOG_LENGTH_MAX) {
			infoLogLength = OGL_INFO_LOG_LENGTH_MAX;
		}
		glGetShaderInfoLog(shaderID, infoLogLength, NULL, infoLog);
		infoLog[infoLogLength] = '\0';
		LOG_ERROR("Shader compilation log:\n");
		LOG_ERROR("%s\n", infoLog);

		return false;
	}

	return true;
}

RectCoordsNDC ToRectCoordsNDC(Vec2Int pos, Vec2Int size,
	ScreenInfo screenInfo)
{
	RectCoordsNDC result;
	result.pos = {
		2.0f * pos.x / screenInfo.size.x - 1.0f,
		2.0f * pos.y / screenInfo.size.y - 1.0f,
		0.0f
	};
	result.size = {
		2.0f * size.x / screenInfo.size.x,
		2.0f * size.y / screenInfo.size.y
	};
	return result;
}
RectCoordsNDC ToRectCoordsNDC(Vec2Int pos, Vec2Int size, Vec2 anchor,
	ScreenInfo screenInfo)
{
	RectCoordsNDC result;
	result.pos = { (float32)pos.x, (float32)pos.y, 0.0f };
	result.size = { (float32)size.x, (float32)size.y };
	result.pos.x -= anchor.x * result.size.x;
	result.pos.y -= anchor.y * result.size.y;
	result.pos.x = result.pos.x * 2.0f / screenInfo.size.x - 1.0f;
	result.pos.y = result.pos.y * 2.0f / screenInfo.size.y - 1.0f;
	result.size.x *= 2.0f / screenInfo.size.x;
	result.size.y *= 2.0f / screenInfo.size.y;
	return result;
}

template <typename Allocator>
GLuint LoadShaders(const ThreadContext* thread, Allocator* allocator,
	const char* vertFilePath, const char* fragFilePath)
{
	const auto& allocatorState = allocator->SaveState();
	defer (allocator->LoadState(allocatorState));

	// Create GL shaders.
	GLuint vertShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read shader code from files.
	PlatformReadFileResult vertFile = PlatformReadFile(thread, allocator, vertFilePath);
	if (vertFile.size == 0) {
		LOG_ERROR("Failed to read vertex shader file.\n");
		return 0; // TODO what to return
	}
	PlatformReadFileResult fragFile = PlatformReadFile(thread, allocator, fragFilePath);
	if (fragFile.size == 0) {
		LOG_ERROR("Failed to read fragment shader file.\n");
		return 0; // TODO what to return
	}

	// Compile and check shader code.
	// TODO error checking
	if (!CompileAndCheckShader(vertShaderID, vertFile)) {
		LOG_ERROR("Vertex shader compilation failed (%s)\n", vertFilePath);
		glDeleteShader(vertShaderID);
		glDeleteShader(fragShaderID);

		return 0; // TODO what to return
	}
	if (!CompileAndCheckShader(fragShaderID, fragFile)) {
		LOG_ERROR("Fragment shader compilation failed (%s)\n", fragFilePath);
		glDeleteShader(vertShaderID);
		glDeleteShader(fragShaderID);

		return 0; // TODO what to return
	}

	// Link the shader program.
	GLuint programID = glCreateProgram();
	glAttachShader(programID, vertShaderID);
	glAttachShader(programID, fragShaderID);
	glLinkProgram(programID);

	// Check the shader program.
	GLint result;
	glGetProgramiv(programID, GL_LINK_STATUS, &result);
	if (result == GL_FALSE) {
		char infoLog[OGL_INFO_LOG_LENGTH_MAX];
		int infoLogLength;
		glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &infoLogLength);
		if (infoLogLength > OGL_INFO_LOG_LENGTH_MAX) {
			infoLogLength = OGL_INFO_LOG_LENGTH_MAX;
		}
		glGetProgramInfoLog(programID, infoLogLength, NULL, infoLog);
		infoLog[infoLogLength] = '\0';
		LOG_ERROR("Program linking failed:\n");
		LOG_ERROR("%s\n", infoLog);

		return 0; // TODO what to return
	}

	glDetachShader(programID, vertShaderID);
	glDetachShader(programID, fragShaderID);
	glDeleteShader(vertShaderID);
	glDeleteShader(fragShaderID);

	return programID;
}

template <typename Allocator>
RectGL InitRectGL(const ThreadContext* thread, Allocator* allocator)
{
	// TODO I don't even need this stupid shit. Jeez, I'm dumb...
	RectGL rectGL;
	const GLfloat vertices[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f
	};

	glGenVertexArrays(1, &rectGL.vertexArray);
	glBindVertexArray(rectGL.vertexArray);

	glGenBuffers(1, &rectGL.vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, rectGL.vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0, // match shader layout location
		2, // size (vec2)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);

	glBindVertexArray(0);

	rectGL.programID = LoadShaders(thread, allocator, "shaders/rect.vert", "shaders/rect.frag");
	
	return rectGL;
}

template <typename Allocator>
TexturedRectGL InitTexturedRectGL(const ThreadContext* thread, Allocator* allocator)
{
	TexturedRectGL texturedRectGL;
	// TODO probably use indexing for this
	const GLfloat vertices[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f
	};
	const GLfloat uvs[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
		0.0f, 0.0f
	};

	glGenVertexArrays(1, &texturedRectGL.vertexArray);
	glBindVertexArray(texturedRectGL.vertexArray);

	glGenBuffers(1, &texturedRectGL.vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, texturedRectGL.vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0, // match shader layout location
		2, // size (vec2)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);

	glGenBuffers(1, &texturedRectGL.uvBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, texturedRectGL.uvBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(
		1, // match shader layout location
		2, // size (vec2)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);

	glBindVertexArray(0);

	texturedRectGL.programID = LoadShaders(thread, allocator,
		"shaders/texturedRect.vert", "shaders/texturedRect.frag");
	
	return texturedRectGL;
}

template <typename Allocator>
LineGL InitLineGL(const ThreadContext* thread, Allocator* allocator)
{
	LineGL lineGL;

	glGenVertexArrays(1, &lineGL.vertexArray);
	glBindVertexArray(lineGL.vertexArray);

	glGenBuffers(1, &lineGL.vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, lineGL.vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_LINE_POINTS * sizeof(Vec3), NULL,
		GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0, // match shader layout location
		3, // size (vec3)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);

	glBindVertexArray(0);

	lineGL.programID = LoadShaders(thread, allocator, "shaders/line.vert", "shaders/line.frag");
	
	return lineGL;
}

template <typename Allocator>
PlaneGL InitPlaneGL(const ThreadContext* thread, Allocator* allocator)
{
	PlaneGL planeGL;
	const GLfloat vertices[] = {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		-1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f
	};

	glGenVertexArrays(1, &planeGL.vertexArray);
	glBindVertexArray(planeGL.vertexArray);

	glGenBuffers(1, &planeGL.vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, planeGL.vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0, // match shader layout location
		3, // size (vec3)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);

	glBindVertexArray(0);

	planeGL.programID = LoadShaders(thread, allocator, "shaders/plane.vert", "shaders/plane.frag");
	
	return planeGL;
}

template <typename Allocator>
BoxGL InitBoxGL(const ThreadContext* thread, Allocator* allocator)
{
	BoxGL boxGL;
	const GLfloat vertices[] = {
		0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f,

		0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,

		1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f,

		0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 0.0f,

		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f,

		0.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 0.0f
	};

	glGenVertexArrays(1, &boxGL.vertexArray);
	glBindVertexArray(boxGL.vertexArray);

	glGenBuffers(1, &boxGL.vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, boxGL.vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0, // match shader layout location
		3, // size (vec3)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);

	glBindVertexArray(0);

	boxGL.programID = LoadShaders(thread, allocator, "shaders/box.vert", "shaders/box.frag");
	
	return boxGL;
}

void DrawRect(RectGL rectGL, ScreenInfo screenInfo,
	Vec2Int pos, Vec2 anchor, Vec2Int size, Vec4 color)
{
	RectCoordsNDC ndc = ToRectCoordsNDC(pos, size, anchor, screenInfo);

	GLint loc;
	glUseProgram(rectGL.programID);
	loc = glGetUniformLocation(rectGL.programID, "posBottomLeft");
	glUniform3fv(loc, 1, &ndc.pos.e[0]);
	loc = glGetUniformLocation(rectGL.programID, "size");
	glUniform2fv(loc, 1, &ndc.size.e[0]);
	loc = glGetUniformLocation(rectGL.programID, "color");
	glUniform4fv(loc, 1, &color.e[0]);

	glBindVertexArray(rectGL.vertexArray);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void DrawTexturedRect(TexturedRectGL texturedRectGL, ScreenInfo screenInfo,
	Vec2Int pos, Vec2 anchor, Vec2Int size, bool32 flipHorizontal, GLuint texture)
{
	RectCoordsNDC ndc = ToRectCoordsNDC(pos, size, anchor, screenInfo);

	GLint loc;
	glUseProgram(texturedRectGL.programID);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	loc = glGetUniformLocation(texturedRectGL.programID, "textureSampler");
	glUniform1i(loc, 0);

	loc = glGetUniformLocation(texturedRectGL.programID, "posBottomLeft");
	glUniform3fv(loc, 1, &ndc.pos.e[0]);
	loc = glGetUniformLocation(texturedRectGL.programID, "size");
	glUniform2fv(loc, 1, &ndc.size.e[0]);
	loc = glGetUniformLocation(texturedRectGL.programID, "flipHorizontal");
	glUniform1i(loc, flipHorizontal);

	glBindVertexArray(texturedRectGL.vertexArray);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void DrawPlane(PlaneGL planeGL,
	Mat4 vp, Vec3 point, Vec3 normal, Vec4 color)
{
	GLint loc;
	glUseProgram(planeGL.programID);

	Mat4 model = Translate(point)
		* UnitQuatToMat4(QuatRotBetweenVectors(Vec3::unitZ, normal));
	Mat4 mvp = vp * model;
	loc = glGetUniformLocation(planeGL.programID, "mvp");
	glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp.e[0][0]);
	loc = glGetUniformLocation(planeGL.programID, "color");
	glUniform4fv(loc, 1, &color.e[0]);

	glBindVertexArray(planeGL.vertexArray);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void DrawBox(BoxGL boxGL,
	Mat4 vp, Vec3 min, Vec3 max, Vec4 color)
{
	GLint loc;
	glUseProgram(boxGL.programID);

	loc = glGetUniformLocation(boxGL.programID, "min");
	glUniform3fv(loc, 1, &min.e[0]);
	loc = glGetUniformLocation(boxGL.programID, "max");
	glUniform3fv(loc, 1, &max.e[0]);
	loc = glGetUniformLocation(boxGL.programID, "mvp");
	glUniformMatrix4fv(loc, 1, GL_FALSE, &vp.e[0][0]);
	loc = glGetUniformLocation(boxGL.programID, "color");
	glUniform4fv(loc, 1, &color.e[0]);

	glBindVertexArray(boxGL.vertexArray);
	glDrawArrays(GL_TRIANGLES, 0, 48);
	glBindVertexArray(0);
}

void DrawLine(LineGL lineGL, Mat4 transform, const LineGLData* lineData, Vec4 color)
{
	GLint loc;
	glUseProgram(lineGL.programID);

	loc = glGetUniformLocation(lineGL.programID, "mvp");
	glUniformMatrix4fv(loc, 1, GL_FALSE, &transform.e[0][0]);
	loc = glGetUniformLocation(lineGL.programID, "color");
	glUniform4fv(loc, 1, &color.e[0]);

	glBindVertexArray(lineGL.vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, lineGL.vertexBuffer);
	// Buffer orphaning, a common way to improve streaming perf.
	// See http://www.opengl.org/wiki/Buffer_Object_Streaming
	glBufferData(GL_ARRAY_BUFFER, MAX_LINE_POINTS * sizeof(Vec3), NULL,
		GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, lineData->count * sizeof(Vec3),
		lineData->pos);
	glDrawArrays(GL_LINE_STRIP, 0, lineData->count);
	glBindVertexArray(0);
}