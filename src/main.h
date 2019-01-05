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

enum Scene
{
	SCENE_TOWN,
	SCENE_FISHING
};

enum FishingRotation
{
	FISHING_ROT_LEFT = 0,
	FISHING_ROT_UP,
	FISHING_ROT_RIGHT,
	FISHING_ROT_DOWN
};

struct ColliderBox
{
	Vec2Int pos;
	Vec2Int size;
};

struct ObjectStatic
{
	Vec2Int pos;
	Vec2 anchor;
	TextureGL texture;
};

struct ObjectAnimated
{
	Vec2Int pos;
	Vec2 anchor;
	AnimatedSprite sprite;
	
#if GAME_INTERNAL
	ClickableBox box;
#endif
};

struct Fish
{
	Vec2Int pos;
	Vec2Int size;
};

struct GameState
{
	AudioState audioState;

	Scene activeScene;

	// Overworld state
	Vec2Int cameraPos;
	Vec2Int playerPos;
	Vec2Int playerVel;
	bool32 falling;
	bool32 facingRight;

	// Fishing state
	FishingRotation rotation;
	Fish fish[10];

	float32 grainTime;

#if GAME_INTERNAL
	bool32 debugView;
	bool32 editor;
#endif

	RectGL rectGL;
	TexturedRectGL texturedRectGL;
	LineGL lineGL;
	TextGL textGL;

	AnimatedSprite spriteKid;
	AnimatedSprite spriteMe;

	ObjectStatic background;
	ObjectStatic clouds;

	ObjectAnimated guys;
	ObjectAnimated bush;

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
