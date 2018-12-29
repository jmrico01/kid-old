#include "main.h"

#undef internal
#include <random>
#define internal static

#include "main_platform.h"
#include "km_debug.h"
#include "km_defines.h"
#include "km_input.h"
#include "km_math.h"
#include "km_string.h"
#include "opengl.h"
#include "opengl_funcs.h"
#include "opengl_base.h"
#include "load_png.h"

// These must match the max sizes in blur.frag
#define KERNEL_HALFSIZE_MAX 10
#define KERNEL_SIZE_MAX (KERNEL_HALFSIZE_MAX * 2 + 1)

inline float32 RandFloat32()
{
    return (float32)rand() / RAND_MAX;
}
inline float32 RandFloat32(float32 min, float32 max)
{
    DEBUG_ASSERT(max > min);
    return RandFloat32() * (max - min) + min;
}

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

void InitializeFramebuffers(int n, Framebuffer framebuffers[])
{
    for (int i = 0; i < n; i++) {
        glGenFramebuffers(1, &framebuffers[i].framebuffer);
        framebuffers[i].state = FBSTATE_INITIALIZED;
    }
}

void UpdateFramebufferColorAttachments(int n, Framebuffer framebuffers[],
    GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type)
{
    for (int i = 0; i < n; i++) {
        if (framebuffers[i].state == FBSTATE_COLOR
        || framebuffers[i].state == FBSTATE_COLOR_DEPTH) {
            glDeleteTextures(1, &framebuffers[i].color);
        }
        glGenTextures(1, &framebuffers[i].color);

        glBindTexture(GL_TEXTURE_2D, framebuffers[i].color);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
            width, height,
            0,
            format, type,
            NULL
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i].framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, framebuffers[i].color, 0);

        GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
            DEBUG_PRINT("Incomplete framebuffer (%d), status %x\n",
                i, fbStatus);
        }
    }
}

void UpdateFramebufferDepthAttachments(int n, Framebuffer framebuffers[],
    GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type)
{
    for (int i = 0; i < n; i++) {
        if (framebuffers[i].state == FBSTATE_COLOR_DEPTH) {
            glDeleteTextures(1, &framebuffers[i].depth);
        }
        glGenTextures(1, &framebuffers[i].depth);

        glBindTexture(GL_TEXTURE_2D, framebuffers[i].depth);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
            width, height,
            0,
            format, type,
            NULL
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // TODO FIX
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i].framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D, framebuffers[i].depth, 0);

        GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
            DEBUG_PRINT("Incomplete framebuffer (%d), status %x\n",
                i, fbStatus);
        }
    }
}

extern "C" GAME_UPDATE_AND_RENDER_FUNC(GameUpdateAndRender)
{
    // NOTE: for clarity
    // A call to this function means the following has happened:
    //  - A frame has been displayed to the user
    //  - The latest user input has been processed by the platform layer
    //
    // This function is expected to update the state of the game
    // and draw the frame that will be displayed, ideally, some constant
    // amount of time in the future.
	DEBUG_ASSERT(sizeof(GameState) <= memory->permanent.size);

	GameState *gameState = (GameState*)memory->permanent.memory;
    if (memory->DEBUGShouldInitGlobalFuncs) {
	    // Initialize global function names
#if GAME_SLOW
        debugPrint_ = platformFuncs->DEBUGPlatformPrint;
#endif
        #define FUNC(returntype, name, ...) name = \
        platformFuncs->glFunctions.name;
            GL_FUNCTIONS_BASE
            GL_FUNCTIONS_ALL
        #undef FUNC

        memory->DEBUGShouldInitGlobalFuncs = false;
    }
	if (!memory->isInitialized) {
        // Very explicit depth testing setup (DEFAULT VALUES)
        // NDC is left-handed with this setup
        // (subtle left-handedness definition:
        //  front objects have z = -1, far objects have z = 1)
        // Nearer objects have less z than farther objects
        glDepthFunc(GL_LEQUAL);
        // Depth buffer clears to farthest z-value (1)
        glClearDepth(1.0);
        // Depth buffer transforms -1 to 1 range to 0 to 1 range
        glDepthRange(0.0, 1.0);

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);

        /*InitAudioState(thread, &gameState->audioState, audio,
            &memory->transient,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);*/

        // Game data
		gameState->pos = Vec2Int { 0, 300 };

        gameState->grainTime = 0.0f;

        // Rendering stuff
        gameState->rectGL = InitRectGL(thread,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->texturedRectGL = InitTexturedRectGL(thread,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->lineGL = InitLineGL(thread,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->textGL = InitTextGL(thread,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        FT_Error error = FT_Init_FreeType(&gameState->ftLibrary);
        if (error) {
            DEBUG_PRINT("FreeType init error: %d\n", error);
        }
        gameState->fontFaceSmall = LoadFontFace(thread, gameState->ftLibrary,
            "data/fonts/ocr-a/regular.ttf", 18,
            memory->transient,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->fontFaceMedium = LoadFontFace(thread, gameState->ftLibrary,
            "data/fonts/ocr-a/regular.ttf", 24,
            memory->transient,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        InitializeFramebuffers(NUM_FRAMEBUFFERS_COLOR_DEPTH, gameState->framebuffersColorDepth);
        InitializeFramebuffers(NUM_FRAMEBUFFERS_COLOR, gameState->framebuffersColor);
        InitializeFramebuffers(NUM_FRAMEBUFFERS_GRAY, gameState->framebuffersGray);

        const GLfloat vertices[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            1.0f, 1.0f,
            1.0f, 1.0f,
            -1.0f, 1.0f,
            -1.0f, -1.0f
        };
        const GLfloat uvs[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
            0.0f, 0.0f
        };

        glGenVertexArrays(1, &gameState->screenQuadVertexArray);
        glBindVertexArray(gameState->screenQuadVertexArray);

        glGenBuffers(1, &gameState->screenQuadVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gameState->screenQuadVertexBuffer);
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

        glGenBuffers(1, &gameState->screenQuadUVBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gameState->screenQuadUVBuffer);
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

        gameState->screenShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/screen.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->bloomExtractShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/bloomExtract.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->bloomBlendShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/bloomBlend.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->blurShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/blur.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->grainShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/grain.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        gameState->testTexture = LoadPNGOpenGL(thread,
            "data/textures/jon.png",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        gameState->particleTextureBase = LoadPNGOpenGL(thread,
            "data/textures/base.png",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

		memory->isInitialized = true;
	}
    if (screenInfo.changed) {
        // TODO not ideal to check for changed screen every frame
        // probably not that big of a deal, but might also be easy to avoid
        // later on with a more callback-y mechanism?

        UpdateFramebufferColorAttachments(NUM_FRAMEBUFFERS_COLOR_DEPTH,
            gameState->framebuffersColorDepth,
            GL_RGB, screenInfo.size.x, screenInfo.size.y, GL_RGB, GL_UNSIGNED_BYTE);
        UpdateFramebufferColorAttachments(NUM_FRAMEBUFFERS_COLOR, gameState->framebuffersColor,
            GL_RGB, screenInfo.size.x, screenInfo.size.y, GL_RGB, GL_UNSIGNED_BYTE);
        UpdateFramebufferColorAttachments(NUM_FRAMEBUFFERS_GRAY, gameState->framebuffersGray,
            GL_RED, screenInfo.size.x, screenInfo.size.y, GL_RED, GL_FLOAT);

        UpdateFramebufferDepthAttachments(NUM_FRAMEBUFFERS_COLOR_DEPTH,
            gameState->framebuffersColorDepth,
            GL_DEPTH24_STENCIL8, screenInfo.size.x, screenInfo.size.y,
            GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);

        DEBUG_PRINT("Updated screen-size-dependent info\n");
    }

    gameState->grainTime = fmod(gameState->grainTime + deltaTime, 5.0f);

    int playerPixelsPerSecond = 500;
    int playerSpeed = playerPixelsPerSecond * deltaTime;
    Vec2Int vel = Vec2Int::zero;
    if (IsKeyPressed(input, KM_KEY_A)) {
        vel.x -= playerSpeed;
    }
    if (IsKeyPressed(input, KM_KEY_D)) {
        vel.x += playerSpeed;
    }
    gameState->pos += vel;

    // Toggle global mute
    if (WasKeyReleased(input, KM_KEY_M)) {
        gameState->audioState.globalMute = !gameState->audioState.globalMute;
    }

    // ------------------------- Begin Rendering -------------------------
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.9f, 0.9f, 0.9f, 0.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColorDepth[0].framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Vec2 anchor = Vec2 { 0.5f, 0.0f };
    Vec2Int size = Vec2Int { 200, 200 };
    DrawTexturedRect(gameState->texturedRectGL, screenInfo,
        gameState->pos, anchor, size, gameState->testTexture);

    // --------------------- Post processing passes ---------------------
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // Apply filters
    // PostProcessGrain(gameState->framebuffersColorDepth[0], gameState->framebuffersColor[0],
    //     gameState->screenQuadVertexArray,
    //     gameState->grainShader, gameState->grainTime);

    // Render to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(gameState->screenQuadVertexArray);
    glUseProgram(gameState->screenShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gameState->framebuffersColorDepth[0].color);
    GLint loc = glGetUniformLocation(gameState->screenShader, "framebufferTexture");
    glUniform1i(loc, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // -------------------------- End Rendering --------------------------
    glEnable(GL_BLEND);

    {
        char fpsStr[128];
        sprintf(fpsStr, "FPS: %f", 1.0f / deltaTime);
        Vec2Int fpsPos = {
            screenInfo.size.x - 10,
            screenInfo.size.y - 10,
        };
        Vec4 color = Vec4 { 0.1f, 0.1f, 0.1f, 1.0f };
        DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
            fpsStr, fpsPos, Vec2 { 1.0f, 1.0f }, color, memory->transient);
    }

    OutputAudio(audio, gameState, input, memory->transient);

#if GAME_INTERNAL
    if (gameState->audioState.debugView) {
        char strBuf[128];
        Vec2Int audioInfoStride = {
            0,
            -((int)gameState->fontFaceSmall.height + 6)
        };
        Vec2Int audioInfoPos = {
            10,
            screenInfo.size.y - 10,
        };
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            "Audio Engine", audioInfoPos, Vec2 { 0.0f, 1.0f },
            Vec4::one,
            memory->transient
        );
        sprintf(strBuf, "Sample Rate: %d", audio->sampleRate);
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            strBuf, audioInfoPos, Vec2 { 0.0f, 1.0f },
            Vec4::one,
            memory->transient
        );
        sprintf(strBuf, "Channels: %d", audio->channels);
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            strBuf, audioInfoPos, Vec2 { 0.0f, 1.0f },
            Vec4::one,
            memory->transient
        );
        sprintf(strBuf, "tWaveTable: %f", gameState->audioState.waveTable.tWaveTable);
        audioInfoPos += audioInfoStride;
        DrawText(gameState->textGL, gameState->fontFaceSmall, screenInfo,
            strBuf, audioInfoPos, Vec2 { 0.0f, 1.0f },
            Vec4::one,
            memory->transient
        );
    }
#endif

#if GAME_SLOW
    // Catch-all site for OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        DEBUG_PRINT("OpenGL error: 0x%x\n", err);
    }
#endif
}

#include "km_input.cpp"
#include "km_string.cpp"
#include "km_lib.cpp"
#include "opengl_base.cpp"
#include "text.cpp"
#include "load_png.cpp"
#include "particles.cpp"
#include "load_wav.cpp"
#include "audio.cpp"