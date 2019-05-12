#pragma once

#include "animation.h"
#include "audio.h"
#include "collision.h"
#include "framebuffer.h"
#include "gui.h"
#include "km_math.h"
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

#define LINE_COLLIDERS_MAX 16

#define INVENTORY_SIZE 8

int GetPillarboxWidth(ScreenInfo screenInfo);

enum PlayerState
{
	PLAYER_STATE_GROUNDED,
	PLAYER_STATE_JUMPING,
	PLAYER_STATE_FALLING
};

struct TextureWithPosition
{
	Vec2 pos;
	Vec2 anchor;
    float32 scale;
	TextureGL texture;
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

struct InventoryItem
{
    Vec2 playerOffset;
    Vec2 anchor;
    TextureGL* textureWorld;
    TextureGL* textureIcon;
};

struct GameState
{
	AudioState audioState;

	Vec2 cameraPos;
	Quat cameraRot;
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

    uint64 selectedItem;
    FixedArray<InventoryItem, INVENTORY_SIZE> inventoryItems;
    TextureGL jonItemWorld1;
    TextureGL jonItemIcon1;
    TextureGL jonItemWorld2;
    TextureGL jonItemIcon2;
    TextureGL jonItemWorld3;
    TextureGL jonItemIcon3;
    TextureGL jonItemWorld4;
    TextureGL jonItemIcon4;

	Vec2 barrelCoords;

    FloorCollider floor;

    int levelLoaded;

    FixedArray<LineCollider, LINE_COLLIDERS_MAX> lineColliders;

	float32 grainTime;

#if GAME_INTERNAL
	bool32 debugView;
	bool32 editor;
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

    AnimatedSprite spriteBarrel;
    AnimatedSprite spriteCrystal;

    AnimatedSpriteInstance kid;
    AnimatedSpriteInstance paper;

    AnimatedSpriteInstance barrel;

    TextureWithPosition background;

    Rock rock;
    TextureGL rockTexture;

    TextureGL frame;
    TextureGL pixelTexture;

    //TextureGL lutBase;
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
