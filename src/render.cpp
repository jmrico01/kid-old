#include "render.h"

#include "opengl_funcs.h"

bool InitRenderState(RenderState& renderState,
	const ThreadContext* thread,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{

    glGenVertexArrays(1, &renderState.vertexArray);
    glBindVertexArray(renderState.vertexArray);

    const GLfloat vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    glGenBuffers(1, &renderState.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, renderState.vertexBuffer);
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
    glVertexAttribDivisor(0, 0);

    const GLfloat uvs[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    glGenBuffers(1, &renderState.uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, renderState.uvBuffer);
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
    glVertexAttribDivisor(1, 0);

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

    glBindVertexArray(0);

    renderState.programID = LoadShaders(thread,
        "shaders/sprite.vert", "shaders/sprite.frag",
        DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);

	return true;
}

void DrawSpriteBatch(const RenderState& renderState, const SpriteDataGL& spriteDataGL)
{
}