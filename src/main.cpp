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
#include "load_obj.h"
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

void DrawMeshGL(const MeshGL& meshGL,
    Mat4 proj, Mat4 view,
    Vec3 pos, Quat rot, Vec4 color,
    Vec3 lightDir)
{
    GLint loc;
    glUseProgram(meshGL.programID);
    
    Mat4 model = Translate(pos) * UnitQuatToMat4(rot);
    Mat4 mvp = proj * view * model;
    loc = glGetUniformLocation(meshGL.programID, "mvp");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &mvp.e[0][0]);
    loc = glGetUniformLocation(meshGL.programID, "model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &model.e[0][0]);
    loc = glGetUniformLocation(meshGL.programID, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &view.e[0][0]);
    loc = glGetUniformLocation(meshGL.programID, "lightDir");
    glUniform3fv(loc, 1, &lightDir.e[0]);
    loc = glGetUniformLocation(meshGL.programID, "color");
    glUniform4fv(loc, 1, &color.e[0]);

    glBindVertexArray(meshGL.vertexArray);
    glDrawArrays(GL_TRIANGLES, 0, meshGL.vertexCount);
    glBindVertexArray(0);
}

void PostProcessSSAO(GLuint gPosition, GLuint gNormal, GLuint noiseTexture,
    float32 radius, float32 bias,
    GLuint framebufferOutGray,
    GLuint framebufferScratchGray, GLuint colorScratchGray,
    GLuint screenQuadVertexArray, GLuint shader, GLuint blurShader,
    Mat4 view, Mat4 proj, ScreenInfo screenInfo)
{
    // SSAO pass
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferScratchGray);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(screenQuadVertexArray);
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    GLint loc = glGetUniformLocation(shader, "gPosition");
    glUniform1i(loc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    loc = glGetUniformLocation(shader, "gNormal");
    glUniform1i(loc, 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    loc = glGetUniformLocation(shader, "noise");
    glUniform1i(loc, 2);

    const int SSAO_KERNEL_SIZE = 64;

    std::uniform_real_distribution<float32> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    std::vector<Vec3> ssaoKernel;
    for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        Vec3 sample = {
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator)
        };
        sample = Normalize(sample);
        sample *= randomFloats(generator);
        float32 scale = (float32)i / SSAO_KERNEL_SIZE;
        scale = Lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    loc = glGetUniformLocation(shader, "samples");
    glUniform3fv(loc, SSAO_KERNEL_SIZE, &ssaoKernel[0].e[0]);

    loc = glGetUniformLocation(shader, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &view.e[0][0]);
    loc = glGetUniformLocation(shader, "proj");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &proj.e[0][0]);

    Vec2 noiseScale = {
        (float32)screenInfo.size.x / 4.0f,
        (float32)screenInfo.size.y / 4.0f
    };
    loc = glGetUniformLocation(shader, "noiseScale");
    glUniform2fv(loc, 1, &noiseScale.e[0]);

    loc = glGetUniformLocation(shader, "radius");
    glUniform1f(loc, radius);
    loc = glGetUniformLocation(shader, "bias");
    glUniform1f(loc, bias);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Blur pass
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferOutGray);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(screenQuadVertexArray);
    glUseProgram(blurShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorScratchGray);
    loc = glGetUniformLocation(blurShader, "ssao");
    glUniform1i(loc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    loc = glGetUniformLocation(blurShader, "gPosition");
    glUniform1i(loc, 1);

    loc = glGetUniformLocation(blurShader, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &view.e[0][0]);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PostProcessFXAA(GLuint colorBufferIn, GLuint framebufferOut,
    GLuint screenQuadVertexArray, GLuint shader, ScreenInfo screenInfo)
{
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferOut);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(screenQuadVertexArray);
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorBufferIn);
    GLint loc = glGetUniformLocation(shader, "framebufferTexture");
    glUniform1i(loc, 0);
    loc = glGetUniformLocation(shader, "invScreenSize");
    glUniform2f(loc, 1.0f / screenInfo.size.x, 1.0f / screenInfo.size.y);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PostProcessBloom(GLuint colorBufferIn,
    GLuint framebufferOut, GLuint framebufferScratch,
    GLuint colorBufferOut, GLuint colorBufferScratch,
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
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferScratch);
    //glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(screenQuadVertexArray);
    glUseProgram(extractShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorBufferIn);
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
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferOut);
        //glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(screenQuadVertexArray);
        glUseProgram(blurShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBufferScratch);
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
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferScratch);
        //glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(screenQuadVertexArray);
        glUseProgram(blurShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBufferOut);
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
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferOut);
    //glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(screenQuadVertexArray);
    glUseProgram(blendShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorBufferIn);
    loc = glGetUniformLocation(blendShader, "scene");
    glUniform1i(loc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, colorBufferScratch);
    loc = glGetUniformLocation(blendShader, "bloomBlur");
    glUniform1i(loc, 1);
    loc = glGetUniformLocation(blendShader, "bloomMag");
    glUniform1f(loc, bloomMag);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PostProcessGrain(GLuint colorBufferIn, GLuint framebufferOut,
    GLuint screenQuadVertexArray, GLuint shader, float32 grainTime)
{
    float32 grainMag = 0.2f;
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferOut);
    //glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(screenQuadVertexArray);
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorBufferIn);
    GLint loc = glGetUniformLocation(shader, "scene");
    glUniform1i(loc, 0);
    loc = glGetUniformLocation(shader, "grainMag");
    glUniform1f(loc, grainMag);
    loc = glGetUniformLocation(shader, "time");
    glUniform1f(loc, grainTime * 100003.0f);

    glDrawArrays(GL_TRIANGLES, 0, 6);
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
        glClearColor(0.05f, 0.05f, 0.1f, 0.0f);
        //glClearColor(0.8f, 0.8f, 0.8f, 0.0f);

        // Very explicit depth testing setup (DEFAULT VALUES)
        // NDC is left-handed with this setup
        // (subtle left-handedness definition:
        //  front objects have z = -1, far objects have z = 1)
        glEnable(GL_DEPTH_TEST);
        // Nearer objects have less z than farther objects
        glDepthFunc(GL_LEQUAL);
        // Depth buffer clears to farthest z-value (1)
        glClearDepth(1.0);
        // Depth buffer transforms -1 to 1 range to 0 to 1 range
        glDepthRange(0.0, 1.0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);

        /*InitAudioState(thread, &gameState->audioState, audio,
            &memory->transient,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);*/

        // Game data
		gameState->pos = Vec3::unitY * 4.0f;
        gameState->yaw = 1.0f;
        gameState->pitch = -0.8f;

        gameState->lightRot = Quat::one;

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
            "data/fonts/ibm-plex-mono/regular.ttf", 18,
            memory->transient,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->fontFaceMedium = LoadFontFace(thread, gameState->ftLibrary,
            "data/fonts/ibm-plex-mono/regular.ttf", 24,
            memory->transient,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        // TODO probably want to abstract this out
        glGenFramebuffers(NUM_FRAMEBUFFERS_COLOR,
            gameState->framebuffersColor);
        for (int i = 0; i < NUM_FRAMEBUFFERS_COLOR; i++) {
            gameState->colorBuffersColor[i] = 0;
        }
        glGenFramebuffers(NUM_FRAMEBUFFERS_GRAY,
            gameState->framebuffersGray);
        for (int i = 0; i < NUM_FRAMEBUFFERS_GRAY; i++) {
            gameState->colorBuffersGray[i] = 0;
        }
        glGenFramebuffers(1, &gameState->gBuffer);
        gameState->gPosition = 0;
        gameState->gNormal = 0;
        gameState->gColor = 0;
        gameState->depthBuffer = 0;

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

        gameState->gbufferShader = LoadShaders(thread,
            "shaders/gbuffer.vert", "shaders/gbuffer.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->deferredShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/deferred.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->screenShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/screen.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->ssaoShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/ssao.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->ssaoBlurShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/ssaoBlur.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        gameState->fxaaShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/fxaa.frag",
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

        std::uniform_real_distribution<float32> randomFloats(0.0, 1.0);
        std::default_random_engine generator;
        Vec3 ssaoNoise[16];
        for (int i = 0; i < 16; i++) {
            ssaoNoise[i] = {
                randomFloats(generator) * 2.0f - 1.0f,
                randomFloats(generator) * 2.0f - 1.0f,
                0.0f
            };
        }

        glGenTextures(1, &gameState->ssaoNoiseTexture);
        glBindTexture(GL_TEXTURE_2D, gameState->ssaoNoiseTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
            4, 4, 0,
            GL_RGB,
            GL_FLOAT,
            &ssaoNoise[0]
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        gameState->pTexBase = LoadPNGOpenGL(thread,
            "data/textures/base.png",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        gameState->testMeshGL = LoadOBJOpenGL(thread,
            //"data/models/tile-test-1.obj",
            //"data/models/cube.obj",
            "data/models/test-scene.obj",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

		memory->isInitialized = true;
	}
    if (screenInfo.changed) {
        // TODO not ideal to check for changed screen every frame
        // probably not that big of a deal, but might also be easy to avoid
        // later on with a more callback-y mechanism?

        // Generate color framebuffer attachments
        if (gameState->colorBuffersColor[0] != 0) {
            glDeleteTextures(NUM_FRAMEBUFFERS_COLOR,
                gameState->colorBuffersColor);
        }
        glGenTextures(NUM_FRAMEBUFFERS_COLOR, gameState->colorBuffersColor);

        for (int i = 0; i < NUM_FRAMEBUFFERS_COLOR; i++) {
            glBindTexture(GL_TEXTURE_2D, gameState->colorBuffersColor[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                screenInfo.size.x, screenInfo.size.y,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                NULL
            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColor[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, gameState->colorBuffersColor[i], 0);

            GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
                DEBUG_PRINT("Incomplete framebuffer (%d), status %x\n",
                    i, fbStatus);
            }
        }

        // Generate gray framebuffer attachments
        if (gameState->colorBuffersGray[0] != 0) {
            glDeleteTextures(NUM_FRAMEBUFFERS_GRAY,
                gameState->colorBuffersGray);
        }
        glGenTextures(NUM_FRAMEBUFFERS_GRAY, gameState->colorBuffersGray);

        for (int i = 0; i < NUM_FRAMEBUFFERS_GRAY; i++) {
            glBindTexture(GL_TEXTURE_2D, gameState->colorBuffersGray[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                screenInfo.size.x, screenInfo.size.y,
                0,
                GL_RED,
                GL_FLOAT,
                NULL
            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersGray[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, gameState->colorBuffersGray[i], 0);

            GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
                DEBUG_PRINT("Incomplete framebuffer (%d), status %x\n",
                    i, fbStatus);
            }
        }

        // G-BUFFER
        if (gameState->gPosition != 0) {
            glDeleteTextures(1, &gameState->gPosition);
            glDeleteTextures(1, &gameState->gNormal);
            glDeleteTextures(1, &gameState->gColor);
            glDeleteTextures(1, &gameState->depthBuffer);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, gameState->gBuffer);
          
        // g-buffer: position
        glGenTextures(1, &gameState->gPosition);
        glBindTexture(GL_TEXTURE_2D, gameState->gPosition);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
            screenInfo.size.x, screenInfo.size.y,
            0,
            GL_RGB,
            GL_FLOAT,
            NULL
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, gameState->gPosition, 0);
          
        // g-buffer: normal
        glGenTextures(1, &gameState->gNormal);
        glBindTexture(GL_TEXTURE_2D, gameState->gNormal);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F,
            screenInfo.size.x, screenInfo.size.y,
            0,
            GL_RGB,
            GL_FLOAT,
            NULL
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
            GL_TEXTURE_2D, gameState->gNormal, 0);
          
        // g-buffer: color
        glGenTextures(1, &gameState->gColor);
        glBindTexture(GL_TEXTURE_2D, gameState->gColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
            screenInfo.size.x, screenInfo.size.y,
            0,
            GL_RGB,
            GL_UNSIGNED_BYTE,
            NULL
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
            GL_TEXTURE_2D, gameState->gColor, 0);
        
        GLuint attachments[3] = {
            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
        };
        glDrawBuffers(3, attachments);

        // g-buffer: depth
        glGenTextures(1, &gameState->depthBuffer);
        glBindTexture(GL_TEXTURE_2D, gameState->depthBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8,
            screenInfo.size.x, screenInfo.size.y,
            0,
            GL_DEPTH_STENCIL,
            GL_UNSIGNED_INT_24_8,
            NULL
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D, gameState->depthBuffer, 0);

        GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
            DEBUG_PRINT("Incomplete framebuffer (0), status %x\n", fbStatus);
        }

        DEBUG_PRINT("Updated screen-size-dependent info\n");
    }

    gameState->grainTime = fmod(gameState->grainTime + deltaTime, 5.0f);

    if (input->mouseButtons[0].isDown) {
        float32 lightRotSpeed = 0.01f;
        gameState->lightRot =
            QuatFromAngleUnitAxis(input->mouseDelta.x * lightRotSpeed,
                Vec3::unitY)
            * QuatFromAngleUnitAxis(-input->mouseDelta.y * lightRotSpeed,
                Vec3::unitX)
            * gameState->lightRot;
        gameState->lightRot = Normalize(gameState->lightRot);
    }

    float32 rotSpeed = 0.01f;
    gameState->yaw += input->mouseDelta.x * rotSpeed;
    gameState->yaw = fmod(gameState->yaw, 2.0f * PI_F);
    gameState->pitch -= input->mouseDelta.y * rotSpeed;
    gameState->pitch = ClampFloat32(gameState->pitch,
        -PI_F / 2.0f, PI_F / 2.0f);

    Quat rotYaw = QuatFromAngleUnitAxis(gameState->yaw, Vec3::unitY);
    Quat rotPitch = QuatFromAngleUnitAxis(gameState->pitch, Vec3::unitX);
    Quat rot = Normalize(rotPitch * rotYaw);

    // Debug camera position control
    float32 speed = 4.0f;
    if (IsKeyPressed(input, KM_KEY_SHIFT)) {
        speed *= 2.0f;
    }
    bool move = false;
    Vec3 vel = Vec3::zero;
    if (IsKeyPressed(input, KM_KEY_W)) {
        vel -= Vec3::unitZ * speed;
        move = true;
    }
    if (IsKeyPressed(input, KM_KEY_S)) {
        vel += Vec3::unitZ * speed;
        move = true;
    }
    if (IsKeyPressed(input, KM_KEY_A)) {
        vel -= Vec3::unitX * speed;
        move = true;
    }
    if (IsKeyPressed(input, KM_KEY_D)) {
        vel += Vec3::unitX * speed;
        move = true;
    }
    if (IsKeyPressed(input, KM_KEY_Q)) {
        vel -= Vec3::unitY * speed;
        move = true;
    }
    if (IsKeyPressed(input, KM_KEY_E)) {
        vel += Vec3::unitY * speed;
        move = true;
    }
    if (move) {
        gameState->pos += Inverse(rotYaw) * (vel * deltaTime);
    }

    // Toggle global mute
    if (WasKeyReleased(input, KM_KEY_M)) {
        gameState->audioState.globalMute = !gameState->audioState.globalMute;
    }

    // ------------------------- Begin Rendering -------------------------
    // Deferred rendering pipeline
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    GLint loc;
    glBindFramebuffer(GL_FRAMEBUFFER, gameState->gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    Mat4 proj = Projection(70.0f,
        (float32)screenInfo.size.x / (float32)screenInfo.size.y,
        0.1f, 50.0f);
    Mat4 view = UnitQuatToMat4(rot) * Translate(-gameState->pos);

    glUseProgram(gameState->gbufferShader);

    MeshGL meshGL = gameState->testMeshGL;

    Mat4 model = Mat4::one;
    Mat4 vp = proj * view;
    Vec4 color = Vec4 { 1.0f, 1.0f, 1.0f, 1.0f };
    loc = glGetUniformLocation(gameState->gbufferShader, "vp");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &vp.e[0][0]);
    loc = glGetUniformLocation(gameState->gbufferShader, "model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, &model.e[0][0]);
    loc = glGetUniformLocation(gameState->gbufferShader, "color");
    glUniform4fv(loc, 1, &color.e[0]);

    glBindVertexArray(meshGL.vertexArray);
    glDrawArrays(GL_TRIANGLES, 0, meshGL.vertexCount);
    glBindVertexArray(0);

    // --------------------- Post processing passes ---------------------
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    PostProcessSSAO(gameState->gPosition, gameState->gNormal,
        gameState->ssaoNoiseTexture,
        gameState->pos.y / 4.0f, 0.05f,
        gameState->framebuffersGray[0],
        gameState->framebuffersGray[1], gameState->colorBuffersGray[1],
        gameState->screenQuadVertexArray,
        gameState->ssaoShader, gameState->ssaoBlurShader,
        view, proj, screenInfo);

    Vec3 lightDir = gameState->lightRot * -Vec3::unitY;

    glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColor[0]);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindVertexArray(gameState->screenQuadVertexArray);
    glUseProgram(gameState->deferredShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gameState->gPosition);
    loc = glGetUniformLocation(gameState->deferredShader, "gPosition");
    glUniform1i(loc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gameState->gNormal);
    loc = glGetUniformLocation(gameState->deferredShader, "gNormal");
    glUniform1i(loc, 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gameState->gColor);
    loc = glGetUniformLocation(gameState->deferredShader, "gColor");
    glUniform1i(loc, 2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gameState->colorBuffersGray[0]);
    loc = glGetUniformLocation(gameState->deferredShader, "ssao");
    glUniform1i(loc, 3);
    loc = glGetUniformLocation(gameState->deferredShader,
        "lightDirWorldSpace");
    glUniform3fv(loc, 1, &lightDir.e[0]);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // TODO maybe create a framebuffer object with buffer + attachments
    // Apply filters
    PostProcessFXAA(gameState->colorBuffersColor[0], gameState->framebuffersColor[2],
        gameState->screenQuadVertexArray, gameState->fxaaShader, screenInfo);

    /*PostProcessBloom(gameState->colorBuffersColor[2],
        gameState->framebuffersColor[0], gameState->framebuffersColor[1],
        gameState->colorBuffersColor[0], gameState->colorBuffersColor[1],
        gameState->screenQuadVertexArray, gameState->bloomExtractShader,
        gameState->blurShader, gameState->bloomBlendShader);*/

    PostProcessGrain(gameState->colorBuffersColor[2], gameState->framebuffersColor[1],
        gameState->screenQuadVertexArray,
        gameState->grainShader, gameState->grainTime);

    // Render to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindVertexArray(gameState->screenQuadVertexArray);
    glUseProgram(gameState->screenShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gameState->colorBuffersColor[1]);
    loc = glGetUniformLocation(gameState->screenShader,
        "framebufferTexture");
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
        DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
            fpsStr, fpsPos, Vec2 { 1.0f, 1.0f }, Vec4::one, memory->transient);
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
#include "load_obj.cpp"
#include "load_png.cpp"
#include "particles.cpp"
#include "load_wav.cpp"
#include "audio.cpp"