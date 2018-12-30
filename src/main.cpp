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
#include "post.h"

inline float32 RandFloat32()
{
    return (float32)rand() / RAND_MAX;
}
inline float32 RandFloat32(float32 min, float32 max)
{
    DEBUG_ASSERT(max > min);
    return RandFloat32() * (max - min) + min;
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

        InitAudioState(thread, &gameState->audioState, audio,
            &memory->transient,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        // Game data
        gameState->pos = Vec2Int { 0, 100 };
        gameState->vel = Vec2Int { 0, 0 };
        gameState->falling = true;
        gameState->facingRight = true;

        gameState->grainTime = 0.0f;

#if GAME_INTERNAL
        gameState->debugView = true;
#endif

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

        glGenVertexArrays(1, &gameState->screenQuadVertexArray);
        glBindVertexArray(gameState->screenQuadVertexArray);

        glGenBuffers(1, &gameState->screenQuadVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, gameState->screenQuadVertexBuffer);
        const GLfloat vertices[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            1.0f, 1.0f,
            1.0f, 1.0f,
            -1.0f, 1.0f,
            -1.0f, -1.0f
        };
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
        const GLfloat uvs[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
            0.0f, 0.0f
        };
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

        gameState->particleTextureBase = LoadPNGOpenGL(thread,
            "data/textures/base.png",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        gameState->backgroundTexture = LoadPNGOpenGL(thread,
            "data/textures/bg.png",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        const int numIdleFrames = 2;
        const int idleFrames[numIdleFrames] = { 0, 4 };
        gameState->animationKid = LoadAnimation(thread,
            8, 8, "data/textures/kid",
            numIdleFrames, idleFrames,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->animationMe = LoadAnimation(thread,
            8, 8, "data/textures/me",
            numIdleFrames, idleFrames,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->animationGuys = LoadAnimation(thread,
            3, 2, "data/textures/guys",
            0, nullptr,
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

    const float32 REF_SCALE_FACTOR = screenInfo.size.y / 1080.0f; 
    const int FLOOR_LEVEL = 0;
    const int PLAYER_WALK_SPEED = (int)(220 * REF_SCALE_FACTOR);
    const int PLAYER_JUMP_SPEED = 500;
    const int GRAVITY_ACCEL = 1000;

    gameState->vel.x = 0;
    if (IsKeyPressed(input, KM_KEY_A)) {
        gameState->vel.x -= PLAYER_WALK_SPEED;
        gameState->facingRight = false;
    }
    if (IsKeyPressed(input, KM_KEY_D)) {
        gameState->vel.x += PLAYER_WALK_SPEED;
        gameState->facingRight = true;
    }

    if (gameState->falling) {
        gameState->vel.y -= (int)(GRAVITY_ACCEL * deltaTime);
    }
    else {
        gameState->vel.y = 0;
        if (IsKeyPressed(input, KM_KEY_SPACE)) {
            gameState->vel.y = PLAYER_JUMP_SPEED;
            gameState->falling = true;
            gameState->audioState.soundKick.playing = true;
            gameState->audioState.soundKick.sampleIndex = 0;
        }
    }

    Vec2Int deltaPos = {
        (int)((float32)gameState->vel.x * deltaTime),
        (int)((float32)gameState->vel.y * deltaTime)
    };
    gameState->pos += deltaPos;
    if (gameState->pos.y < FLOOR_LEVEL) {
        gameState->pos.y = FLOOR_LEVEL;
        gameState->falling = false;
        gameState->audioState.soundSnare.playing = true;
        gameState->audioState.soundSnare.sampleIndex = 0;
    }

    bool32 isWalking = !gameState->falling && gameState->vel.x != 0;
    gameState->animationKid.Update(deltaTime, isWalking);
    gameState->animationMe.Update(deltaTime, isWalking);
    gameState->animationGuys.Update(deltaTime, true);

    // Toggle global mute
    if (WasKeyPressed(input, KM_KEY_M)) {
        gameState->audioState.globalMute = !gameState->audioState.globalMute;
    }

    // ---------------------------- Begin Rendering ---------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColorDepth[0].framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Vec2Int playerPos = { screenInfo.size.x / 2, screenInfo.size.y / 3 };
    Vec2Int objectOffset = playerPos - gameState->pos;

    Vec2Int backgroundPos = {
        0,
        (int)(-75 * REF_SCALE_FACTOR)
    };
    Vec2 backgroundAnchor = { 0.5f, 0.0f };
    Vec2Int backgroundSize = {
        (int)(4834 * REF_SCALE_FACTOR),
        screenInfo.size.y
    };
    DrawTexturedRect(gameState->texturedRectGL, screenInfo,
        backgroundPos + objectOffset, backgroundAnchor, backgroundSize, false,
        gameState->backgroundTexture);

    Vec2Int GUYS_SIZE = { 638, 602 };
    Vec2Int guysPos = {
        (int)(1300 * REF_SCALE_FACTOR),
        (int)(-75 * REF_SCALE_FACTOR)
    };
    Vec2 guysAnchor = { 0.5f, 0.0f };
    Vec2Int guysSize = {
        (int)((float32)GUYS_SIZE.x * REF_SCALE_FACTOR),
        (int)((float32)GUYS_SIZE.y * REF_SCALE_FACTOR)
    };
    gameState->animationGuys.Draw(gameState->texturedRectGL, screenInfo,
        guysPos + objectOffset, guysAnchor, guysSize, false);

    Vec2Int ORIGINAL_SIZE = { 377, 393 };
    Vec2 anchor = { 0.5f, 0.0f };
    Vec2Int size = {
        (int)((float32)ORIGINAL_SIZE.x * REF_SCALE_FACTOR),
        (int)((float32)ORIGINAL_SIZE.y * REF_SCALE_FACTOR)
    };
    Vec2Int ME_TEXT_OFFSET = { -20, 20 };
    gameState->animationKid.Draw(gameState->texturedRectGL, screenInfo,
        playerPos, anchor, size, !gameState->facingRight);
    gameState->animationMe.Draw(gameState->texturedRectGL, screenInfo,
        playerPos + ME_TEXT_OFFSET, anchor, size, false);

    const float32 ASPECT_RATIO = 4.0f / 3.0f;
    int targetWidth = (int)(screenInfo.size.y * ASPECT_RATIO);
    int pillarboxWidth = (screenInfo.size.x - targetWidth) / 2;
    Vec2Int pillarboxPos1 = Vec2Int::zero;
    Vec2Int pillarboxPos2 = { screenInfo.size.x - pillarboxWidth, 0 };
    Vec2 pillarboxAnchor = Vec2 { 0.0f, 0.0f };
    Vec2Int pillarboxSize = { pillarboxWidth, screenInfo.size.y };
    Vec4 pillarboxColor = { 0.1f, 0.1f, 0.1f, 1.0f };
    DrawRect(gameState->rectGL, screenInfo,
        pillarboxPos1, pillarboxAnchor, pillarboxSize, pillarboxColor);
    DrawRect(gameState->rectGL, screenInfo,
        pillarboxPos2, pillarboxAnchor, pillarboxSize, pillarboxColor);

    // ------------------------ Post processing passes ------------------------
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

    // ---------------------------- End Rendering -----------------------------
    glEnable(GL_BLEND);

    OutputAudio(audio, gameState, input, memory->transient);

#if GAME_INTERNAL
    Vec4 fontColor = { 0.1f, 0.1f, 0.1f, 1.0f };

    if (WasKeyPressed(input, KM_KEY_G)) {
        gameState->debugView = !gameState->debugView;
    }

    if (gameState->debugView) {
        char fpsStr[128];
        sprintf(fpsStr, "%.2f FPS", 1.0f / deltaTime);
        Vec2Int fpsPos = {
            screenInfo.size.x - 15,
            screenInfo.size.y - 10,
        };
        DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
            fpsStr, fpsPos, Vec2 { 1.0f, 1.0f }, fontColor, memory->transient);
    }

    DrawDebugAudioInfo(audio, gameState, input, screenInfo, memory->transient);
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
#include "animation.cpp"
#include "post.cpp"