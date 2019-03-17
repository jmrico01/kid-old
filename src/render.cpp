#include "render.h"

#include "opengl_funcs.h"

bool InitSpriteState(SpriteStateGL& spriteStateGL,
	const ThreadContext* thread,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	glGenVertexArrays(1, &spriteStateGL.vertexArray);
	glBindVertexArray(spriteStateGL.vertexArray);

	const GLfloat vertices[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f
	};
	glGenBuffers(1, &spriteStateGL.vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, spriteStateGL.vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(
		0, // match shader layout location
		2, // size (vec2)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);

	const GLfloat uvs[] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f
	};
	glGenBuffers(1, &spriteStateGL.uvBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, spriteStateGL.uvBuffer);
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

	spriteStateGL.programID = LoadShaders(thread,
		"shaders/sprite.vert", "shaders/sprite.frag",
		DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);

	return true;
}

bool InitRenderState(RenderState& renderState,
	const ThreadContext* thread,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	InitSpriteState(renderState.spriteStateGL,
		thread, DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);

#if 0
	glGenBuffers(1, &renderState.posBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, renderState.posBuffer);
	glBufferData(GL_ARRAY_BUFFER, SPRITE_BATCH_SIZE * sizeof(Vec3), NULL,
		GL_STREAM_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(
		2, // match shader layout location
		3, // size (vec3)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);
	glVertexAttribDivisor(2, 1);

	glGenBuffers(1, &renderState.sizeBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, renderState.sizeBuffer);
	glBufferData(GL_ARRAY_BUFFER, SPRITE_BATCH_SIZE * sizeof(Vec2), NULL,
		GL_STREAM_DRAW);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(
		3, // match shader layout location
		2, // size (vec2)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);
	glVertexAttribDivisor(3, 1);

	glGenBuffers(1, &renderState.uvInfoBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, renderState.uvInfoBuffer);
	glBufferData(GL_ARRAY_BUFFER, SPRITE_BATCH_SIZE * sizeof(Vec4), NULL,
		GL_STREAM_DRAW);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(
		4, // match shader layout location
		4, // size (vec4)
		GL_FLOAT, // type
		GL_FALSE, // normalized?
		0, // stride
		(void*)0 // array buffer offset
	);
	glVertexAttribDivisor(4, 1);
#endif

	return true;
}

void PushSprite(SpriteDataGL* spriteDataGL,
	Vec2 pos, Vec2 size, Vec2 anchor, float32 alpha,
    bool32 flipHorizontal, GLuint texture)
{
	DEBUG_ASSERT(spriteDataGL->numSprites < SPRITE_BATCH_SIZE);

	int spriteInd = spriteDataGL->numSprites;
	spriteDataGL->pos[spriteInd] = Vec3 {
		pos.x - anchor.x * size.x,
		pos.y - anchor.y * size.y,
		0.0f
	};
	spriteDataGL->size[spriteInd] = size;
	spriteDataGL->uvInfo[spriteInd] = Vec4 {
		0.0f, 0.0f,
		1.0f, 1.0f
	};
	if (flipHorizontal) {
		spriteDataGL->uvInfo[spriteInd] = Vec4 {
			1.0f, 0.0f,
			-1.0f, 1.0f
		};
	}
    spriteDataGL->alpha[spriteInd] = alpha;
	spriteDataGL->texture[spriteInd] = texture;

	spriteDataGL->numSprites++;
}

void DrawSprites(const RenderState& renderState,
	const SpriteDataGL& spriteDataGL, Mat4 view, Mat4 proj)
{
	DEBUG_ASSERT(spriteDataGL.numSprites <= SPRITE_BATCH_SIZE);

	GLuint programID = renderState.spriteStateGL.programID;
	glUseProgram(programID);
	GLint loc;

	Mat4 vp = proj * view;
	loc = glGetUniformLocation(programID, "vp");
	glUniformMatrix4fv(loc, 1, GL_FALSE, &vp.e[0][0]);

	glActiveTexture(GL_TEXTURE0);
	loc = glGetUniformLocation(programID, "textureSampler");
	glUniform1i(loc, 0);

	// TODO revamp this eventually. Right now it's clearly the wrong data model
	for (int i = 0; i < spriteDataGL.numSprites; i++) {
		glBindTexture(GL_TEXTURE_2D, spriteDataGL.texture[i]);

		loc = glGetUniformLocation(programID, "posBottomLeft");
		glUniform3fv(loc, 1, &spriteDataGL.pos[i].e[0]);
		loc = glGetUniformLocation(programID, "size");
		glUniform2fv(loc, 1, &spriteDataGL.size[i].e[0]);
        loc = glGetUniformLocation(programID, "uvInfo");
        glUniform4fv(loc, 1, &spriteDataGL.uvInfo[i].e[0]);
        loc = glGetUniformLocation(programID, "alpha");
        glUniform1f(loc, spriteDataGL.alpha[i]);

		glBindVertexArray(renderState.spriteStateGL.vertexArray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
		glBindVertexArray(0);
	}
}