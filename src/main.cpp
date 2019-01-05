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

void PlayerMovementInput(GameState* gameState, float32 deltaTime, const GameInput* input)
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

void MovePlayer(GameState* gameState, float32 deltaTime)
{
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
    Vec2Int size = objectAnimated.sprite.textureSize * scaleFactor;
    objectAnimated.sprite.Draw(texturedRectGL, screenInfo,
        pos + objectOffset, objectAnimated.anchor, size, false);
}

void UpdateTown(GameState* gameState, float32 deltaTime, const GameInput* input)
{
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
        PlayerMovementInput(gameState, deltaTime, input);
    }
#else
    PlayerMovementInput(gameState, deltaTime, input);
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

    const int NEXT_ANIMS[1] = { 0 };
    gameState->guys.sprite.Update(deltaTime, 1, NEXT_ANIMS);
    gameState->bush.sprite.Update(deltaTime, 1, NEXT_ANIMS);

    // TODO ideally animation IDs would be strings
    const int KID_IDLE_ANIMS[2] = { 0, 1 };
    const int KID_WALK_ANIMS[1] = { 2 };
    bool32 isWalking = !gameState->falling && gameState->playerVel.x != 0;
    int numNextAnims = isWalking ? 1 : 2;
    const int* nextAnims = isWalking ? KID_WALK_ANIMS : KID_IDLE_ANIMS;
    gameState->spriteKid.Update(deltaTime, numNextAnims, nextAnims);
    gameState->spriteMe.Update(deltaTime, numNextAnims, nextAnims);
}

void DrawTown(GameState* gameState, ScreenInfo screenInfo)
{
#if GAME_INTERNAL
    if (!gameState->editor) {
        gameState->cameraPos = gameState->playerPos;
    }
#else
    gameState->cameraPos = gameState->playerPos;
#endif

    const float32 REF_SCALE_FACTOR = screenInfo.size.y / 1080.0f;
    const Vec2Int CAMERA_OFFSET = { screenInfo.size.x / 2, screenInfo.size.y / 4 };
    Vec2Int objectOffset = CAMERA_OFFSET - gameState->cameraPos * REF_SCALE_FACTOR;

    DrawObjectStatic(gameState->background, REF_SCALE_FACTOR, objectOffset,
        gameState->texturedRectGL, screenInfo);
    /*DrawObjectStatic(gameState->clouds, REF_SCALE_FACTOR, objectOffset,
        gameState->texturedRectGL, screenInfo);

    DrawObjectAnimated(gameState->guys, REF_SCALE_FACTOR, objectOffset,
        gameState->texturedRectGL, screenInfo);
    DrawObjectAnimated(gameState->bush, REF_SCALE_FACTOR, objectOffset,
        gameState->texturedRectGL, screenInfo);*/

    { // kid & me text
        Vec2Int pos = gameState->playerPos * REF_SCALE_FACTOR;
        Vec2 anchor = { 0.5f, 0.0f };
        Vec2Int size = gameState->spriteKid.textureSize * REF_SCALE_FACTOR;
        Vec2Int meTextOffset = { -20, 20 };
        meTextOffset *= REF_SCALE_FACTOR;

        gameState->spriteKid.Draw(gameState->texturedRectGL, screenInfo,
            pos + objectOffset, anchor, size, !gameState->facingRight);
        /*gameState->spriteMe.Draw(gameState->texturedRectGL, screenInfo,
            pos + meTextOffset + objectOffset, anchor, size, false);*/
    }

#if GAME_INTERNAL
    if (gameState->editor) {
        gameState->guys.box.origin = gameState->guys.pos * REF_SCALE_FACTOR + objectOffset;
        gameState->guys.box.size = gameState->guys.sprite.textureSize * REF_SCALE_FACTOR;
        DrawClickableBoxes(&gameState->guys.box, 1, gameState->rectGL, screenInfo);
        gameState->bush.box.origin = gameState->bush.pos * REF_SCALE_FACTOR + objectOffset;
        gameState->bush.box.size = gameState->bush.sprite.textureSize * REF_SCALE_FACTOR;
        DrawClickableBoxes(&gameState->bush.box, 1, gameState->rectGL, screenInfo);
    }
#endif
}

void UpdateFishing(GameState* gameState, float32 deltaTime, const GameInput* input)
{
    int delta = 0;
    if (WasKeyPressed(input, KM_KEY_A)) {
        delta = -1;
    }
    if (WasKeyPressed(input, KM_KEY_D)) {
        delta = 1;
    }
    int newRot = (int)gameState->rotation + delta;
    if (newRot < 0) {
        newRot += 4;
    }
    newRot %= 4;
    gameState->rotation = (FishingRotation)newRot;
}

void DrawFishing(GameState* gameState, ScreenInfo screenInfo)
{
    const float32 REF_SCALE_FACTOR = screenInfo.size.y / 1080.0f;
    const Vec2Int PLAYER_SIZE = { 80, 110 };
    const Vec2Int NET_SIZE = { 120, 120 };
    const int NET_DISTANCE = PLAYER_SIZE.y / 2 + NET_SIZE.y / 2 + 5;

    Vec2Int playerPos = screenInfo.size / 2;
    Vec2Int playerSize = PLAYER_SIZE;
    if (gameState->rotation == FISHING_ROT_LEFT || gameState->rotation == FISHING_ROT_RIGHT) {
        playerSize = Vec2Int { PLAYER_SIZE.y, PLAYER_SIZE.x };
    }
    playerSize *= REF_SCALE_FACTOR;
    DrawRect(gameState->rectGL, screenInfo,
        playerPos, Vec2 { 0.5f, 0.5f }, playerSize,
        Vec4 { 1.0f, 0.0f, 1.0f, 1.0f });

    Vec2Int netDir = Vec2Int::unitY;
    if (gameState->rotation == FISHING_ROT_LEFT) {
        netDir = -Vec2Int::unitX;
    }
    if (gameState->rotation == FISHING_ROT_DOWN) {
        netDir = -Vec2Int::unitY;
    }
    if (gameState->rotation == FISHING_ROT_RIGHT) {
        netDir = Vec2Int::unitX;
    }
    Vec2Int netPos = netDir * NET_DISTANCE;
    netPos = netPos * REF_SCALE_FACTOR + playerPos;
    Vec2Int netSize = NET_SIZE;
    if (gameState->rotation == FISHING_ROT_LEFT || gameState->rotation == FISHING_ROT_RIGHT) {
        netSize = Vec2Int { NET_SIZE.y, NET_SIZE.x };
    }
    netSize *= REF_SCALE_FACTOR;
    DrawRect(gameState->rectGL, screenInfo,
        netPos, Vec2 { 0.5f, 0.5f }, netSize,
        Vec4 { 0.0f, 0.0f, 0.0f, 1.0f });

#if GAME_INTERNAL
    if (gameState->debugView) {
        for (int i = 0; i < 20; i++) {
            int offsetX = (-NET_SIZE.x * REF_SCALE_FACTOR) * (i - 10.5f) + playerPos.x;
            int offsetY = (-NET_SIZE.y * REF_SCALE_FACTOR) * (i - 10.5f) + playerPos.y;
            DrawRect(gameState->rectGL, screenInfo,
                Vec2Int { offsetX, 0 }, Vec2 { 0.5f, 0.0f }, Vec2Int { 1, screenInfo.size.y },
                Vec4 { 0.0f, 0.0f, 0.0f, 1.0f }
            );
            DrawRect(gameState->rectGL, screenInfo,
                Vec2Int { 0, offsetY }, Vec2 { 0.0f, 0.5f }, Vec2Int { screenInfo.size.x, 1 },
                Vec4 { 0.0f, 0.0f, 0.0f, 1.0f }
            );
        }
    }
#endif
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
        gameState->activeScene = SCENE_TOWN;

        // town data init
        gameState->playerPos = Vec2Int { 0, 100 };
        gameState->playerVel = Vec2Int { 0, 0 };
        gameState->falling = true;
        gameState->facingRight = true;

        // fishing data init
        gameState->rotation = FISHING_ROT_UP;

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

        bool32 loadBackground = LoadPNGOpenGL(thread,
            "data/sprites/bg.png", gameState->background.texture,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadBackground) {
            DEBUG_PANIC("Failed to load background");
        }
        gameState->background.pos = { -gameState->background.texture.size.x / 2, -300 };
        gameState->background.anchor = { 0.0f, 0.0f };
        bool32 loadClouds = LoadPNGOpenGL(thread,
            "data/sprites/clouds.png", gameState->clouds.texture,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadClouds) {
            DEBUG_PANIC("Failed to load clouds");
        }
        gameState->clouds.pos = { -gameState->clouds.texture.size.x / 2, -75 };
        gameState->clouds.anchor = { 0.0f, 0.0f };

        bool32 loadKidAnim = LoadAnimatedSprite(thread, "data/animations/kid/animation",
            gameState->spriteKid,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadKidAnim) {
            DEBUG_PANIC("Failed to load kid animation sprite");
        }
        bool32 loadMeAnim = LoadAnimatedSprite(thread, "data/animations/me/animation",
            gameState->spriteMe,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadMeAnim) {
            DEBUG_PANIC("Failed to load me animation sprite");
        }

        bool32 loadGuysAnim = LoadAnimatedSprite(thread, "data/animations/guys/animation",
            gameState->guys.sprite,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadGuysAnim) {
            DEBUG_PANIC("Failed to load guys animation sprite");
        }
        gameState->guys.pos = Vec2Int { 1300, -75 };
        gameState->guys.anchor = Vec2 { 0.5f, 0.0f };

        bool32 loadBushAnim = LoadAnimatedSprite(thread, "data/animations/bush/animation",
            gameState->bush.sprite,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadBushAnim) {
            DEBUG_PANIC("Failed to load bush animation sprite");
        }
        gameState->bush.pos = Vec2Int { -150, 20 };
        gameState->bush.anchor = Vec2 { 0.5f, 0.0f };

#if GAME_INTERNAL
        gameState->guys.box = CreateClickableBox(gameState->guys.pos,
            gameState->guys.sprite.textureSize,
            gameState->guys.anchor,
            Vec4 { 0.0f, 0.0f, 0.0f, 0.1f },
            Vec4 { 0.0f, 0.0f, 0.0f, 0.4f },
            Vec4 { 0.0f, 0.0f, 0.0f, 0.7f }
        );
        gameState->bush.box = CreateClickableBox(gameState->bush.pos,
            gameState->bush.sprite.textureSize,
            gameState->bush.anchor,
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

    // gameState->grainTime = fmod(gameState->grainTime + deltaTime, 5.0f);

    if (WasKeyPressed(input, KM_KEY_1)) {
        gameState->activeScene = SCENE_TOWN;
    }
    if (WasKeyPressed(input, KM_KEY_2)) {
        gameState->activeScene = SCENE_FISHING;
    }

    // Toggle global mute
    if (WasKeyPressed(input, KM_KEY_M)) {
        gameState->audioState.globalMute = !gameState->audioState.globalMute;
    }

    switch (gameState->activeScene) {
        case SCENE_TOWN: {
            UpdateTown(gameState, deltaTime, input);
        } break;
        case SCENE_FISHING: {
            UpdateFishing(gameState, deltaTime, input);
        } break;
    }

    // ---------------------------- Begin Rendering ---------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    //glBlendFunc(GL_DST_COLOR, GL_ZERO);
    glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColorDepth[0].framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    switch (gameState->activeScene) {
        case SCENE_TOWN: {
            DrawTown(gameState, screenInfo);
        } break;
        case SCENE_FISHING: {
            DrawFishing(gameState, screenInfo);
        } break;
    }

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

    DrawRect(gameState->rectGL, screenInfo,
        input->mousePos, Vec2 { 0.5f, 0.5f }, Vec2Int { 5, 5 },
        Vec4 { 0.1f, 0.1f, 0.1f, 1.0f });

    // ---------------------------- End Rendering -----------------------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
#include "framebuffer.cpp"
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