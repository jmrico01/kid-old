#pragma once

#include "animation.h"
#include "audio.h"
#include "framebuffer.h"
#include "gui.h"
#include "km_math.h"
#include "load_png.h"
#include "opengl.h"
#include "opengl_base.h"
#include "text.h"

#define NUM_FRAMEBUFFERS_COLOR_DEPTH  1
#define NUM_FRAMEBUFFERS_COLOR        2
#define NUM_FRAMEBUFFERS_GRAY         1

#define LINE_COLLIDER_MAX_VERTICES 32
#define LINE_COLLIDERS_MAX 4

enum Scene
{
	SCENE_TOWN,
	SCENE_FISHING
};

struct ColliderLine
{
    int numVertices;
    Vec2 vertices[LINE_COLLIDER_MAX_VERTICES];
};

struct ColliderBox
{
	Vec2 pos;
	Vec2 size;
};

struct ObjectStatic
{
	Vec2 pos;
	Vec2 anchor;
	TextureGL texture;
};

struct ObjectAnimated
{
	Vec2 pos;
	Vec2 anchor;
	AnimatedSprite sprite;
	
#if GAME_INTERNAL
	ClickableBox box;
#endif
};

#define FISHING_OBSTACLES_MAX 1000

struct FishingObstacle
{
	Vec2Int pos;
	Vec2Int vel;
	Vec2Int size;
};

struct GameState
{
	AudioState audioState;

	Scene activeScene;

	// Overworld state
	Vec2 cameraPos;
	Vec2 playerPos;
	Vec2 playerVel;
    bool32 falling;
	bool32 facingRight;

    int numLineColliders;
    ColliderLine lineColliders[LINE_COLLIDERS_MAX];

#if GAME_INTERNAL
    float32 floorHeight;
#endif

	// Fishing state
	int playerPosX;
	float32 obstacleTimer;
	int numObstacles;
	FishingObstacle obstacles[FISHING_OBSTACLES_MAX];

    // Other
	float32 grainTime;

#if GAME_INTERNAL
	bool32 debugView;
	bool32 editor;
#endif

    RenderState renderState;

	RectGL rectGL;
	TexturedRectGL texturedRectGL;
	LineGL lineGL;
	TextGL textGL;

	AnimatedSprite spriteKid;
	AnimatedSprite spriteMe;

	ObjectStatic background;

	FT_Library ftLibrary;
	FontFace fontFaceSmall;
	FontFace fontFaceMedium;

	Framebuffer framebuffersColorDepth[NUM_FRAMEBUFFERS_COLOR_DEPTH];
	Framebuffer framebuffersColor[NUM_FRAMEBUFFERS_COLOR];
	Framebuffer framebuffersGray[NUM_FRAMEBUFFERS_GRAY];
	// TODO make a vertex array struct probably
	GLuint screenQuadVertexArray;
	GLuint screenQuadVertexBuffer;
	GLuint screenQuadUVBuffer;

	GLuint screenShader;
	GLuint bloomExtractShader;
	GLuint bloomBlendShader;
	GLuint blurShader;
	GLuint grainShader;
};
