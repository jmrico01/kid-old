#pragma once

#include <km_common/km_math.h>

#include "animation.h"
#include "audio.h"
#include "collision.h"
#include "framebuffer.h"
#include "load_level.h"
#include "load_png.h"
#include "opengl.h"
#include "opengl_base.h"
#include "text.h"

#define TARGET_ASPECT_RATIO     (4.0f / 3.0f)
#define REF_PIXEL_SCREEN_HEIGHT 1440
#define REF_PIXELS_PER_UNIT     120

#define NUM_FRAMEBUFFERS_COLOR_DEPTH  1
#define NUM_FRAMEBUFFERS_COLOR        2
#define NUM_FRAMEBUFFERS_GRAY         1

int GetPillarboxWidth(ScreenInfo screenInfo);

enum PlayerState
{
	PLAYER_STATE_GROUNDED,
	PLAYER_STATE_JUMPING,
	PLAYER_STATE_FALLING
};

struct Rock
{
	Vec2 coords;
	float32 angle;
};

struct GrabbedObjectInfo
{
    Vec2* coordsPtr;
    Vec2 rangeX, rangeY;
};

struct LiftedObjectInfo
{
    TextureWithPosition* spritePtr;
    Vec2 offset;
    float32 placementOffsetX;
    float32 coordYPrev;
};

struct GameState
{
	AudioState audioState;

	Vec2 cameraPos;
	Quat cameraRot; // TODO Come on dude, who needs Quaternions in a 2D game
	Vec2 cameraCoords;
	float32 prevFloorCoordY;
    Vec2 playerCoords;
	Vec2 playerVel;
	PlayerState playerState;
	const LineCollider* currentPlatform;
	bool32 facingRight;
    float32 playerJumpMag;
    bool playerJumpHolding;
    float32 playerJumpHold;

    GrabbedObjectInfo grabbedObject;
    LiftedObjectInfo liftedObject;

    uint64 activeLevel;
    LevelData levels[LEVELS_MAX];

	float32 grainTime;

#if GAME_INTERNAL
	bool32 kmKey;
	bool32 debugView;
    float32 editorScaleExponent;

    // Editor
    int floorVertexSelected;
#endif

    RenderState renderState;

	RectGL rectGL;
	TexturedRectGL texturedRectGL;
	LineGL lineGL;
	TextGL textGL;

    AnimatedSprite spriteKid;
    AnimatedSprite spritePaper;

    AnimatedSpriteInstance kid;
    AnimatedSpriteInstance paper;

    Rock rock;
    TextureGL rockTexture;

    TextureGL frame;
    TextureGL pixelTexture;

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
