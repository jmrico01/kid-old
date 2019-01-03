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

void PlayerInput(GameState* gameState, float32 deltaTime, const GameInput* input)
{
    // Constants tuned for 1080p
    const int PLAYER_WALK_SPEED = 220;
    const int PLAYER_JUMP_SPEED = 500;

    if (IsKeyPressed(input, KM_KEY_A)) {
        gameState->playerVel.x -= PLAYER_WALK_SPEED;
        gameState->facingRight = false;
    }
    if (IsKeyPressed(input, KM_KEY_D)) {
        gameState->playerVel.x += PLAYER_WALK_SPEED;
        gameState->facingRight = true;
    }

    if (!gameState->falling) {
        if (IsKeyPressed(input, KM_KEY_SPACE)) {
            gameState->playerVel.y = PLAYER_JUMP_SPEED;
            gameState->falling = true;
            gameState->audioState.soundKick.playing = true;
            gameState->audioState.soundKick.sampleIndex = 0;
        }
    }
}

void DrawObjectStatic(const ObjectStatic& objectStatic,
    float32 scaleFactor, Vec2Int objectOffset,
    TexturedRectGL texturedRectGL, ScreenInfo screenInfo)
{
    Vec2Int pos = objectStatic.pos * scaleFactor;
    Vec2Int size = objectStatic.texture.size * scaleFactor;
    DrawTexturedRect(texturedRectGL, screenInfo,
        pos + objectOffset, objectStatic.anchor, size, false,
        objectStatic.texture.textureID);
}

void DrawObjectAnimated(const ObjectAnimated& objectAnimated,
    float32 scaleFactor, Vec2Int objectOffset,
    TexturedRectGL texturedRectGL, ScreenInfo screenInfo)
{
    Vec2Int pos = objectAnimated.pos * scaleFactor;
    Vec2Int size = objectAnimated.animation.frameTextures[0].size * scaleFactor;
    objectAnimated.animation.Draw(texturedRectGL, screenInfo,
        pos + objectOffset, objectAnimated.anchor, size, false);
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
        gameState->playerPos = Vec2Int { 0, 100 };
        gameState->playerVel = Vec2Int { 0, 0 };
        gameState->falling = true;
        gameState->facingRight = true;

        gameState->grainTime = 0.0f;

#if GAME_INTERNAL
        gameState->debugView = true;
        gameState->editor = false;
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

        gameState->background.texture = LoadPNGOpenGL(thread,
            "data/textures/bg.png",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->background.pos = { -gameState->background.texture.size.x / 2, -75 };
        gameState->background.anchor = { 0.0f, 0.0f };
        gameState->clouds.texture = LoadPNGOpenGL(thread,
            "data/textures/clouds.png",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->clouds.pos = { -gameState->clouds.texture.size.x / 2, -75 };
        gameState->clouds.anchor = { 0.0f, 0.0f };

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

        gameState->guys.animation = LoadAnimation(thread,
            3, 2, "data/textures/guys",
            0, nullptr,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->guys.pos = Vec2Int { 1300, -75 };
        gameState->guys.anchor = Vec2 { 0.5f, 0.0f };

        gameState->bush.animation = LoadAnimation(thread,
            4, 6, "data/textures/bush",
            0, nullptr,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->bush.pos = Vec2Int { -150, 20 };
        gameState->bush.anchor = Vec2 { 0.5f, 0.0f };

#if GAME_INTERNAL
        gameState->guys.box = CreateClickableBox(gameState->guys.pos,
            gameState->guys.animation.frameTextures[0].size,
            Vec4 { 0.0f, 0.0f, 0.0f, 0.1f },
            Vec4 { 0.0f, 0.0f, 0.0f, 0.4f },
            Vec4 { 0.0f, 0.0f, 0.0f, 0.7f }
        );
        gameState->bush.box = CreateClickableBox(gameState->bush.pos,
            gameState->bush.animation.frameTextures[0].size,
            Vec4 { 0.0f, 0.0f, 0.0f, 0.1f },
            Vec4 { 0.0f, 0.0f, 0.0f, 0.4f },
            Vec4 { 0.0f, 0.0f, 0.0f, 0.7f }
        );
#endif

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

    gameState->playerVel.x = 0;
#if GAME_INTERNAL
    if (gameState->editor) {
        UpdateClickableBoxes(&gameState->guys.box, 1, input);
        UpdateClickableBoxes(&gameState->bush.box, 1, input);
        if (input->mouseButtons[0].isDown) {
            gameState->cameraPos -= input->mouseDelta;
        }
    }
    else {
        PlayerInput(gameState, deltaTime, input);
    }
#else
    PlayerInput(gameState, deltaTime, input);
#endif

    const int FLOOR_LEVEL = 0;
    const int GRAVITY_ACCEL = 1000;

    if (gameState->falling) {
        gameState->playerVel.y -= (int)(GRAVITY_ACCEL * deltaTime);
    }
    else {
        gameState->playerVel.y = 0;
    }
    gameState->playerPos += gameState->playerVel * deltaTime;
    if (gameState->playerPos.y < FLOOR_LEVEL) {
        gameState->playerPos.y = FLOOR_LEVEL;
        gameState->falling = false;
        gameState->audioState.soundSnare.playing = true;
        gameState->audioState.soundSnare.sampleIndex = 0;
    }

    gameState->guys.animation.Update(deltaTime, true);
    gameState->bush.animation.Update(deltaTime, true);

    bool32 isWalking = !gameState->falling && gameState->playerVel.x != 0;
    gameState->animationKid.Update(deltaTime, isWalking);
    gameState->animationMe.Update(deltaTime, isWalking);

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

#if GAME_INTERNAL
    if (!gameState->editor) {
        gameState->cameraPos = gameState->playerPos;
    }
#else
    gameState->cameraPos = gameState->playerPos;
#endif

    const float32 REF_SCALE_FACTOR = screenInfo.size.y / 1080.0f;
    const Vec2Int CAMERA_OFFSET = { screenInfo.size.x / 2, screenInfo.size.y / 5 };
    Vec2Int objectOffset = CAMERA_OFFSET - gameState->cameraPos * REF_SCALE_FACTOR;

    DrawObjectStatic(gameState->background, REF_SCALE_FACTOR, objectOffset,
        gameState->texturedRectGL, screenInfo);
    DrawObjectStatic(gameState->clouds, REF_SCALE_FACTOR, objectOffset,
        gameState->texturedRectGL, screenInfo);

    DrawObjectAnimated(gameState->guys, REF_SCALE_FACTOR, objectOffset,
        gameState->texturedRectGL, screenInfo);
    DrawObjectAnimated(gameState->bush, REF_SCALE_FACTOR, objectOffset,
        gameState->texturedRectGL, screenInfo);

    { // kid & me text
        Vec2Int pos = gameState->playerPos * REF_SCALE_FACTOR;
        Vec2 anchor = { 0.5f, 0.0f };
        Vec2Int size = gameState->animationKid.frameTextures[0].size * REF_SCALE_FACTOR;
        Vec2Int meTextOffset = { -20, 20 };
        meTextOffset *= REF_SCALE_FACTOR;
        gameState->animationKid.Draw(gameState->texturedRectGL, screenInfo,
            pos + objectOffset, anchor, size, !gameState->facingRight);
        gameState->animationMe.Draw(gameState->texturedRectGL, screenInfo,
            pos + meTextOffset + objectOffset, anchor, size, false);
    }

#if GAME_INTERNAL
    if (gameState->editor) {
        DrawClickableBoxes(&gameState->guys.box, 1, gameState->rectGL, screenInfo);
        DrawClickableBoxes(&gameState->bush.box, 1, gameState->rectGL, screenInfo);
    }
#endif

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

    // Render pillarbox
    const float32 ASPECT_RATIO = 4.0f / 3.0f;
    const Vec4 PILLARBOX_COLOR = { 0.0f, 0.0f, 0.0f, 1.0f }; 
    int targetWidth = (int)(screenInfo.size.y * ASPECT_RATIO);
    int pillarboxWidth = (screenInfo.size.x - targetWidth) / 2;
    Vec2Int pillarboxPos1 = Vec2Int::zero;
    Vec2Int pillarboxPos2 = { screenInfo.size.x - pillarboxWidth, 0 };
    Vec2 pillarboxAnchor = Vec2 { 0.0f, 0.0f };
    Vec2Int pillarboxSize = { pillarboxWidth, screenInfo.size.y };
    if (pillarboxWidth > 0) {
        DrawRect(gameState->rectGL, screenInfo,
            pillarboxPos1, pillarboxAnchor, pillarboxSize, PILLARBOX_COLOR);
        DrawRect(gameState->rectGL, screenInfo,
            pillarboxPos2, pillarboxAnchor, pillarboxSize, PILLARBOX_COLOR);
    }

    // ---------------------------- End Rendering -----------------------------
    glEnable(GL_BLEND);

    OutputAudio(audio, gameState, input, memory->transient);

#if GAME_INTERNAL
    if (WasKeyPressed(input, KM_KEY_G)) {
        gameState->debugView = !gameState->debugView;
    }
    if (WasKeyPressed(input, KM_KEY_H)) {
        gameState->editor = !gameState->editor;
    }

    const Vec4 DEBUG_FONT_COLOR = { 0.05f, 0.05f, 0.05f, 1.0f };
    const Vec2Int MARGIN = { 15, 10 };

    if (gameState->debugView) {
        char fpsStr[128];
        sprintf(fpsStr, "%.2f FPS", 1.0f / deltaTime);
        Vec2Int fpsPos = {
            screenInfo.size.x - pillarboxWidth - MARGIN.x,
            screenInfo.size.y - MARGIN.y,
        };
        DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
            fpsStr, fpsPos, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);
    }
    if (gameState->editor) {
        Vec2Int editorStrPos = {
            pillarboxWidth + MARGIN.x,
            screenInfo.size.y - MARGIN.y,
        };
        Vec4 editorFontColor = { 1.0f, 0.1f, 1.0f, 1.0f };
        DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
            "EDITOR", editorStrPos, Vec2 { 0.0f, 1.0f }, editorFontColor, memory->transient);
    }

    DrawDebugAudioInfo(audio, gameState, input, screenInfo, memory->transient, DEBUG_FONT_COLOR);
#endif

#if GAME_SLOW
    // Catch-all site for OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        DEBUG_PRINT("OpenGL error: 0x%x\n", err);
    }
#endif
}

#include "animation.cpp"
#include "audio.cpp"
#include "gui.cpp"
#include "km_input.cpp"
#include "km_lib.cpp"
#include "km_string.cpp"
#include "load_png.cpp"
#include "load_wav.cpp"
#include "opengl_base.cpp"
#include "particles.cpp"
#include "post.cpp"
#include "text.cpp"