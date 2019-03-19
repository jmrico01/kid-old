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

#define FLOOR_COLLIDER_MAX_VERTICES 16
#define FLOOR_COLLIDERS_MAX 32

#define WALL_COLLIDERS_MAX 4

int GetPillarboxWidth(ScreenInfo screenInfo);

enum PlayerState
{
	PLAYER_STATE_GROUNDED,
	PLAYER_STATE_JUMPING,
	PLAYER_STATE_FALLING
};

struct FloorCollider
{
    int numVertices;
    Vec2 vertices[FLOOR_COLLIDER_MAX_VERTICES];
};

struct WallCollider
{
    Vec2 bottomPos;
    float32 height;
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
    float32 scale;
	TextureGL texture;
};

struct GameState
{
	AudioState audioState;

	// Game state
	Vec2 cameraPos;
    float32 cameraVelY;
	Vec2 playerPos;
	Vec2 playerVel;
	const FloorCollider* currentFloor;
	PlayerState playerState;
	bool32 facingRight;
    float32 playerJumpMag;
    bool playerJumpHolding;
    float32 playerJumpHold;

    int numFloorColliders;
    FloorCollider floorColliders[FLOOR_COLLIDERS_MAX];

    int numWallColliders;
    WallCollider wallColliders[WALL_COLLIDERS_MAX];

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
    AnimatedSprite spritePaper;

    AnimatedSprite spriteBarrel;
    AnimatedSprite spriteCrystal;

    ObjectStatic background;
    ObjectStatic frame;

    ObjectStatic tractor1;
    ObjectStatic tractor2;

    TextureGL lutBase;
    TextureGL lut1;

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
    GLuint lutShader;
};
