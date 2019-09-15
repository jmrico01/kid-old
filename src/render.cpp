#include "render.h"

#include "opengl_base.h"
#include "opengl_funcs.h"

Mat4 CalculateTransform(Vec2 pos, Vec2 size, Vec2 anchor,
    Quat baseRot, Quat rot, bool32 flip)
{
    Vec2 anchorOffset = -Vec2 { anchor.x * size.x, anchor.y * size.y };
    Mat4 transform1 = UnitQuatToMat4(baseRot)
        * Translate(ToVec3(anchorOffset, 0.0f))
        * Scale(ToVec3(size, 1.0f));
    if (flip) {
        transform1 = Scale(Vec3 { -1.0f, 1.0f, 1.0f }) * transform1;
    }
    return Translate(ToVec3(pos, 0.0f)) * UnitQuatToMat4(rot) * transform1;
}

Mat4 CalculateTransform(Vec2 pos, Vec2 size, Vec2 anchor, Quat rot, bool32 flip)
{
	Vec2 anchorOffset = -Vec2 { anchor.x * size.x, anchor.y * size.y };
    Mat4 transform = Translate(ToVec3(anchorOffset, 0.0f)) * Scale(ToVec3(size, 1.0f));
    if (flip) {
        transform = Scale(Vec3 { -1.0f, 1.0f, 1.0f }) * transform;
    }
	return Translate(ToVec3(pos, 0.0f)) * UnitQuatToMat4(rot) * transform;
}

template <typename Allocator>
bool InitSpriteState(const ThreadContext* thread, Allocator* allocator,
	SpriteStateGL& spriteStateGL)
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

	spriteStateGL.multiplyProgramID = LoadShaders(thread, allocator,
		"shaders/sprite.vert", "shaders/spriteMultiply.frag");

	return true;
}

template <typename Allocator>
bool InitRenderState(const ThreadContext* thread, Allocator* allocator, RenderState& renderState)
{
	InitSpriteState(thread, allocator, renderState.spriteStateGL);

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

void PushSprite(SpriteDataGL* spriteDataGL, Mat4 transform, float32 alpha, GLuint texture)
{
	DEBUG_ASSERT(spriteDataGL->numSprites < SPRITE_BATCH_SIZE);

	int spriteInd = spriteDataGL->numSprites;
    /*if (flipHorizontal) {
        Mat4 flipMatrix = Translate(Vec3::unitX) * Scale(Vec3 { -1.0f, 1.0f, 1.0f });
        transform = flipMatrix * transform;
    }*/
	spriteDataGL->transform[spriteInd] = transform;
	spriteDataGL->uvInfo[spriteInd] = Vec4 {
		0.0f, 0.0f,
		1.0f, 1.0f
	};
	/*if (flipHorizontal) {
		spriteDataGL->uvInfo[spriteInd] = Vec4 {
			1.0f, 0.0f,
			-1.0f, 1.0f
		};
	}*/
    spriteDataGL->alpha[spriteInd] = alpha;
	spriteDataGL->texture[spriteInd] = texture;

	spriteDataGL->numSprites++;
}

void DrawSprites(const RenderState& renderState,
	const SpriteDataGL& spriteDataGL, Mat4 transform)
{
	DEBUG_ASSERT(spriteDataGL.numSprites <= SPRITE_BATCH_SIZE);

	GLuint programID = renderState.spriteStateGL.multiplyProgramID;
	glUseProgram(programID);
	GLint loc;

	loc = glGetUniformLocation(programID, "batchTransform");
	glUniformMatrix4fv(loc, 1, GL_FALSE, &transform.e[0][0]);

	glActiveTexture(GL_TEXTURE0);
	loc = glGetUniformLocation(programID, "textureSampler");
	glUniform1i(loc, 0);

	// TODO revamp this eventually. Right now it's clearly the wrong data model
	for (int i = 0; i < spriteDataGL.numSprites; i++) {
		glBindTexture(GL_TEXTURE_2D, spriteDataGL.texture[i]);

		loc = glGetUniformLocation(programID, "transform");
		glUniformMatrix4fv(loc, 1, GL_FALSE, &spriteDataGL.transform[i].e[0][0]);
        loc = glGetUniformLocation(programID, "uvInfo");
        glUniform4fv(loc, 1, &spriteDataGL.uvInfo[i].e[0]);
        loc = glGetUniformLocation(programID, "alpha");
        glUniform1f(loc, spriteDataGL.alpha[i]);

		glBindVertexArray(renderState.spriteStateGL.vertexArray);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
	}
}