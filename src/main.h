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

#define TARGET_ASPECT_RATIO     (4.0f / 3.0f)
#define REF_PIXEL_SCREEN_HEIGHT 1080
#define REF_PIXELS_PER_UNIT     120

#define NUM_FRAMEBUFFERS_COLOR_DEPTH  1
#define NUM_FRAMEBUFFERS_COLOR        2
#define NUM_FRAMEBUFFERS_GRAY         1

#define LINE_COLLIDER_MAX_VERTICES 32
#define LINE_COLLIDERS_MAX 4

#define FISHING_OBSTACLES_MAX 1000

int GetPillarboxWidth(ScreenInfo screenInfo);

enum PlayerState
{
	PLAYER_STATE_GROUNDED,
	PLAYER_STATE_JUMPING,
	PLAYER_STATE_FALLING
};

enum Scene
{
	SCENE_TOWN,
	SCENE_FISHING
};

struct LineCollider
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
	const LineCollider* floorCollider;
	PlayerState playerState;
	bool32 facingRight;

    int numLineColliders;
    LineCollider lineColliders[LINE_COLLIDERS_MAX];

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
