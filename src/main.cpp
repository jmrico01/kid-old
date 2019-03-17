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
#include "render.h"

#define CAMERA_HEIGHT ((REF_PIXEL_SCREEN_HEIGHT) / (REF_PIXELS_PER_UNIT))
#define CAMERA_OFFSET_Y (-CAMERA_HEIGHT / 6.0f)
#define CAMERA_OFFSET_VEC2 (Vec2 { 0.0f, CAMERA_OFFSET_Y })
#define CAMERA_OFFSET_VEC3 (Vec3 { 0.0f, CAMERA_OFFSET_Y, 0.0f })

#define FLOOR_LEVEL 0.0f

#define PLAYER_RADIUS 0.4f
#define PLAYER_HEIGHT 1.3f

#define FLOOR_LINE_MARGIN 0.05f

struct FloorColliderIntersect
{
	Vec2 pos;
	const FloorCollider* collider;
};

inline float32 RandFloat32()
{
	return (float32)rand() / RAND_MAX;
}
inline float32 RandFloat32(float32 min, float32 max)
{
	DEBUG_ASSERT(max > min);
	return RandFloat32() * (max - min) + min;
}

int GetPillarboxWidth(ScreenInfo screenInfo)
{
	int targetWidth = (int)((float32)screenInfo.size.y * TARGET_ASPECT_RATIO);
	return (screenInfo.size.x - targetWidth) / 2;
}

Mat4 CalculateWorldMatrix(ScreenInfo screenInfo)
{
	const float32 ASPECT_RATIO = (float32)screenInfo.size.x / screenInfo.size.y;
	const Vec3 SCREEN_CENTER = {
		CAMERA_HEIGHT * ASPECT_RATIO / 2.0f,
		CAMERA_HEIGHT / 2.0f,
		0.0f
	};
	const Vec3 SCALE_TO_NDC = {
		2.0f * REF_PIXELS_PER_UNIT / REF_PIXEL_SCREEN_HEIGHT / ASPECT_RATIO,
		2.0f * REF_PIXELS_PER_UNIT / REF_PIXEL_SCREEN_HEIGHT,
		1.0f
	};

	return Translate(Vec3 { -1.0f, -1.0f, 0.0f })
		* Scale(SCALE_TO_NDC) * Translate(SCREEN_CENTER);
}

Vec2 ScreenToWorld(Vec2Int screenPos, ScreenInfo screenInfo, Vec2 cameraPos)
{
    float32 screenScale = (float32)screenInfo.size.y / REF_PIXEL_SCREEN_HEIGHT;

	const float32 ASPECT_RATIO = (float32)screenInfo.size.x / screenInfo.size.y;
	const Vec2 SCREEN_CENTER = {
		CAMERA_HEIGHT * ASPECT_RATIO / 2.0f,
		CAMERA_HEIGHT / 2.0f
	};

	Vec2 result = ToVec2(screenPos) / screenScale / REF_PIXELS_PER_UNIT
        - SCREEN_CENTER - CAMERA_OFFSET_VEC2 + cameraPos;
	return result;
}

Vec2 ScreenToWorldScaleOnly(Vec2Int screenPos, ScreenInfo screenInfo)
{
    float32 screenScale = (float32)screenInfo.size.y / REF_PIXEL_SCREEN_HEIGHT;
	return ToVec2(screenPos) / screenScale / REF_PIXELS_PER_UNIT;
}

void DrawObjectStatic(const ObjectStatic& objectStatic, SpriteDataGL* spriteDataGL)
{
	Vec2 size = objectStatic.scale * ToVec2(objectStatic.texture.size) / REF_PIXELS_PER_UNIT;
	PushSprite(spriteDataGL, objectStatic.pos, size,
		objectStatic.anchor, 1.0f,
        false, objectStatic.texture.textureID);
}

bool GetFloorColliderHeight(const FloorCollider* floorCollider, Vec2 refPos, float32* outHeight)
{
	DEBUG_ASSERT(floorCollider->numVertices > 1);

	int numVertices = floorCollider->numVertices;
	Vec2 vertPrev = floorCollider->vertices[0];
	for (int i = 1; i < numVertices; i++) {
		Vec2 vert = floorCollider->vertices[i];
		if (vertPrev.x <= refPos.x && vert.x >= refPos.x) {
			float32 t = (refPos.x - vertPrev.x) / (vert.x - vertPrev.x);
			*outHeight = vertPrev.y + t * (vert.y - vertPrev.y);
			return true;
		}

		vertPrev = vert;
	}
	return false;
}

inline float32 Cross2D(Vec2 v1, Vec2 v2)
{
	return v1.x * v2.y - v1.y * v2.x;
}

// Source: https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
bool32 Intersection(Vec2 line1Start, Vec2 line1Dir, Vec2 line2Start, Vec2 line2Dir,
	Vec2* outIntersect)
{
	Vec2 startDiff = line2Start - line1Start;
	float32 crossDirs12 = Cross2D(line1Dir, line2Dir);
	float32 crossDiffDir1 = Cross2D(startDiff, line1Dir);

	if (crossDirs12 == 0.0f && crossDiffDir1 == 0.0f) {
		// collinear
		float32 magDir1 = MagSq(line1Dir);
		float32 dotDirs = Dot(line1Dir, line2Dir);
		float32 t = Dot(startDiff, line1Dir) / magDir1;
		float32 tt = t + dotDirs / magDir1;
		if ((t < 0.0f && tt < 0.0f) || (t > 1.0f && tt > 1.0f)) {
			return false;
		}

		// Could return any point in interval [t, tt]
		*outIntersect = line1Start + t * line1Dir;
		return true;
	}
	else if (crossDirs12 == 0.0f) {
		// parallel
		return false;
	}
	else {
		float32 t1 = Cross2D(startDiff, line2Dir) / crossDirs12;
		float32 t2 = crossDiffDir1 / crossDirs12;

		if (0.0f <= t1 && t1 <= 1.0f && 0.0f <= t2 && t2 <= 1.0f) {
			// intersection
			*outIntersect = line1Start + t1 * line1Dir;
			return true;
		}
		else {
			// no intersection
			return false;
		}
	}
}

void GetFloorColliderIntersections(const FloorCollider floorColliders[], int numFloorColliders,
	Vec2 pos, Vec2 deltaPos, float32 movementMargin,
	FloorColliderIntersect outIntersects[], int* outNumIntersects)
{
	*outNumIntersects = 0;
	float32 deltaPosMag = Mag(deltaPos);
	if (deltaPosMag == 0.0f) {
		return;
	}
	Vec2 dir = deltaPos / deltaPosMag;
	Vec2 playerDir = deltaPos + dir * movementMargin;

	for (int c = 0; c < numFloorColliders; c++) {
		DEBUG_ASSERT(floorColliders[c].numVertices >= 2);
		Vec2 vertPrev = floorColliders[c].vertices[0];
		for (int v = 1; v < floorColliders[c].numVertices; v++) {
			Vec2 vert = floorColliders[c].vertices[v];
			Vec2 intersectPoint;
			if (Intersection(pos, playerDir, vertPrev, vert - vertPrev, &intersectPoint)) {
				int intersectInd = *outNumIntersects;
				outIntersects[intersectInd].pos = intersectPoint;
				outIntersects[intersectInd].collider = &floorColliders[c];
				(*outNumIntersects)++;
				break;
			}
			vertPrev = vert;
		}
	}
}

void PlayerMovementInput(GameState* gameState, float32 deltaTime, const GameInput* input)
{
	const float32 PLAYER_WALK_SPEED = 3.2f;

	if (IsKeyPressed(input, KM_KEY_A)) {
		gameState->playerVel.x -= PLAYER_WALK_SPEED;
		gameState->facingRight = false;
	}
	if (IsKeyPressed(input, KM_KEY_D)) {
		gameState->playerVel.x += PLAYER_WALK_SPEED;
		gameState->facingRight = true;
	}

	HashKey ANIM_LAND;
	ANIM_LAND.WriteString("Land");

	if (gameState->playerState == PLAYER_STATE_GROUNDED
	&& WasKeyPressed(input, KM_KEY_SPACE)
	&& !KeyCompare(gameState->spriteKid.activeAnimation, ANIM_LAND)) {
		gameState->playerState = PLAYER_STATE_JUMPING;
		gameState->audioState.soundKick.playing = true;
		gameState->audioState.soundKick.sampleIndex = 0;
	}
}

void UpdateTown(GameState* gameState, float32 deltaTime, const GameInput* input)
{
	gameState->playerVel.x = 0.0f;
#if GAME_INTERNAL
	if (!gameState->editor) {
		PlayerMovementInput(gameState, deltaTime, input);
	}
#else
	PlayerMovementInput(gameState, deltaTime, input);
#endif

	HashKey ANIM_IDLE;
	ANIM_IDLE.WriteString("Idle");
	HashKey ANIM_WALK;
	ANIM_WALK.WriteString("Walk");
	HashKey ANIM_JUMP;
	ANIM_JUMP.WriteString("Jump");
	HashKey ANIM_FALL;
	ANIM_FALL.WriteString("Fall");
	HashKey ANIM_LAND;
	ANIM_LAND.WriteString("Land");

	HashKey nextAnims[4];
	int numNextAnims = 0;
	if (gameState->playerState == PLAYER_STATE_JUMPING) {
		if (!KeyCompare(gameState->spriteKid.activeAnimation, ANIM_JUMP)) {
			numNextAnims = 1;
			nextAnims[0] = ANIM_JUMP;
		}
		else {
			numNextAnims = 1;
			nextAnims[0] = ANIM_FALL;
		}
	}
	else if (gameState->playerState == PLAYER_STATE_FALLING) {
		numNextAnims = 1;
		nextAnims[0] = ANIM_FALL;
	}
	else if (gameState->playerState == PLAYER_STATE_GROUNDED) {
		if (gameState->playerVel.x != 0) {
			numNextAnims = 2;
			nextAnims[0] = ANIM_WALK;
			nextAnims[1] = ANIM_LAND;
		}
		else {
			numNextAnims = 2;
			nextAnims[0] = ANIM_IDLE;
			nextAnims[1] = ANIM_LAND;
		}
	}

	if (gameState->playerState == PLAYER_STATE_JUMPING
	&& KeyCompare(gameState->spriteKid.activeAnimation, ANIM_FALL)) {
		gameState->playerState = PLAYER_STATE_FALLING;
	}

	const HashKey* nextAnimations = nextAnims;
	Vec2 rootMotion = gameState->spriteKid.Update(deltaTime, numNextAnims, nextAnimations);

	Vec2 deltaPos = gameState->playerVel * deltaTime + rootMotion;
	Vec2 newPlayerPos = gameState->playerPos + deltaPos;

	FloorColliderIntersect intersects[FLOOR_COLLIDERS_MAX];
	int numIntersects;
	GetFloorColliderIntersections(gameState->floorColliders, gameState->numFloorColliders,
		gameState->playerPos, deltaPos, 0.05f,
		intersects, &numIntersects);
	if (numIntersects > 0) {
		float32 minDist = Mag(intersects[0].pos - gameState->playerPos);
		int minDistInd = 0;
		for (int i = 1; i < numIntersects; i++) {
			float32 dist = Mag(intersects[i].pos - gameState->playerPos);
			if (dist < minDist) {
				minDist = dist;
				minDistInd = i;
			}
		}

		gameState->currentFloor = intersects[minDistInd].collider;

		if (gameState->playerState == PLAYER_STATE_FALLING) {
			newPlayerPos = intersects[minDistInd].pos;
			gameState->playerState = PLAYER_STATE_GROUNDED;
		}
	}

    for (int i = 0; i < gameState->numWallColliders; i++) {
        const WallCollider& wallCollider = gameState->wallColliders[i];
        Rect playerRect;
        playerRect.minX = newPlayerPos.x - PLAYER_RADIUS;
        playerRect.maxX = newPlayerPos.x + PLAYER_RADIUS;
        playerRect.minY = newPlayerPos.y;
        playerRect.maxY = newPlayerPos.y + PLAYER_HEIGHT;
        float32 wallX = wallCollider.bottomPos.x;
        float32 wallMinY = wallCollider.bottomPos.y;
        float32 wallMaxY = wallMinY + wallCollider.height;

        if (playerRect.minX <= wallX && wallX <= playerRect.maxX) {
            if (!((playerRect.minY <= wallMinY && playerRect.maxY <= wallMinY)
            || (playerRect.minY >= wallMaxY && playerRect.maxY >= wallMaxY))) {
                float32 pushDir = 1.0f;
                if (newPlayerPos.x < wallX) {
                    pushDir = -1.0f;
                }

                newPlayerPos.x += pushDir * FLOOR_LINE_MARGIN;
            }
        }
    }

	if (gameState->playerState == PLAYER_STATE_GROUNDED
	&& gameState->currentFloor != nullptr) {
		float32 height;
		if (!GetFloorColliderHeight(gameState->currentFloor, newPlayerPos, &height)) {
			gameState->playerState = PLAYER_STATE_FALLING;
		}
		else {
			newPlayerPos.y = height;
		}
	}

	gameState->playerPos = newPlayerPos;

    gameState->spritePaper.Update(deltaTime, 0, nullptr);

#if GAME_INTERNAL
	if (gameState->editor) {
        return;
	}
#endif

    gameState->cameraPos.x = gameState->playerPos.x;
    gameState->cameraPos.y = 0.0f;
}

void DrawTown(GameState* gameState, SpriteDataGL* spriteDataGL,
    Mat4 worldMatrix, ScreenInfo screenInfo)
{
    spriteDataGL->numSprites = 0;

    DrawObjectStatic(gameState->background, spriteDataGL);

    DrawObjectStatic(gameState->tractor1, spriteDataGL);
    DrawObjectStatic(gameState->tractor2, spriteDataGL);

	{ // kid & me text
		Vec2 anchor = { 0.5f, 0.1f };
		Vec2 size = ToVec2(gameState->spriteKid.textureSize) / REF_PIXELS_PER_UNIT;
		// Vec2 meTextOffset = { -0.1f, 0.1f };

		gameState->spriteKid.Draw(spriteDataGL, gameState->playerPos, size,
			anchor, 1.0f, !gameState->facingRight);
	}

	Vec3 cameraPos = { gameState->cameraPos.x, gameState->cameraPos.y, 0.0f };
	Mat4 view = Translate(CAMERA_OFFSET_VEC3 + -cameraPos);
	DrawSprites(gameState->renderState, *spriteDataGL, view, worldMatrix);

    spriteDataGL->numSprites = 0;

    const float32 ASPECT_RATIO = (float32)screenInfo.size.x / screenInfo.size.y;
    Vec2 screenSizeWorld = { CAMERA_HEIGHT * ASPECT_RATIO, CAMERA_HEIGHT };

    gameState->spritePaper.Draw(spriteDataGL,
        Vec2::zero, screenSizeWorld, Vec2::one / 2.0f, 0.5f,
        false);

    gameState->frame.pos = Vec2::zero;
    gameState->frame.anchor = Vec2::one / 2.0f;
    gameState->frame.scale = 1.0f;
    DrawObjectStatic(gameState->frame, spriteDataGL);

    DrawSprites(gameState->renderState, *spriteDataGL, Mat4::one, worldMatrix);

#if 0
#if GAME_INTERNAL
	if (gameState->editor) {
		gameState->guys.box.origin = ToVec2Int(gameState->guys.pos * REF_SCALE_FACTOR)
			+ objectOffset;
		gameState->guys.box.size = gameState->guys.sprite.textureSize * REF_SCALE_FACTOR;
		DrawClickableBoxes(&gameState->guys.box, 1, gameState->rectGL, screenInfo);
		gameState->bush.box.origin = ToVec2Int(gameState->bush.pos * REF_SCALE_FACTOR)
			+ objectOffset;
		gameState->bush.box.size = gameState->bush.sprite.textureSize * REF_SCALE_FACTOR;
		DrawClickableBoxes(&gameState->bush.box, 1, gameState->rectGL, screenInfo);
	}
#endif
#endif
}

void UpdateFishing(GameState* gameState, float32 deltaTime,
	const GameInput* input, ScreenInfo screenInfo)
{
	const int PLAYER_MOVE_SPEED = 600;
	const Vec2Int PLAYER_SIZE = { 170, 250 };
	const int REF_X = (int)(screenInfo.size.x * 1080.0f / screenInfo.size.y);
	const int MARGIN_X = (REF_X - 1440) / 2;

	gameState->obstacleTimer += deltaTime;
	if (gameState->obstacleTimer >= 0.25f) {
		if (gameState->numObstacles < FISHING_OBSTACLES_MAX) {
			FishingObstacle& newObstacle = gameState->obstacles[gameState->numObstacles];
			newObstacle.pos = Vec2Int { rand() % REF_X, 0 };
			newObstacle.vel = Vec2Int { 0, PLAYER_MOVE_SPEED - 100 + (rand() % 400) };
			newObstacle.size = Vec2Int { 100, 100 };
			gameState->numObstacles++;
			gameState->obstacleTimer = 0.0f;
		}
	}
	for (int i = 0; i < gameState->numObstacles; i++) {
		gameState->obstacles[i].pos += gameState->obstacles[i].vel * deltaTime;
	}

	if (IsKeyPressed(input, KM_KEY_A)) {
		gameState->playerPosX -= (int)(PLAYER_MOVE_SPEED * deltaTime);
	}
	if (IsKeyPressed(input, KM_KEY_D)) {
		gameState->playerPosX += (int)(PLAYER_MOVE_SPEED * deltaTime);
	}

	int minX = MARGIN_X + PLAYER_SIZE.x / 2;
	int maxX = REF_X - MARGIN_X - PLAYER_SIZE.x / 2;
	if (gameState->playerPosX < minX) {
		gameState->playerPosX = minX;
	}
	if (gameState->playerPosX > maxX) {
		gameState->playerPosX = maxX;
	}
}

void DrawFishing(GameState* gameState, SpriteDataGL* spriteDataGL, ScreenInfo screenInfo)
{
	const float32 REF_SCALE_FACTOR = screenInfo.size.y / 1080.0f;
	const Vec2Int PLAYER_SIZE = { 170, 250 };

	Vec2Int playerPos = { gameState->playerPosX, 700 };
	playerPos *= REF_SCALE_FACTOR;
	Vec2Int playerSize = PLAYER_SIZE * REF_SCALE_FACTOR;
	DrawRect(gameState->rectGL, screenInfo,
		playerPos, Vec2 { 0.5f, 0.0f }, playerSize,
		Vec4 { 1.0f, 0.0f, 1.0f, 1.0f });

	for (int i = 0; i < gameState->numObstacles; i++) {
		Vec2Int obstaclePos = gameState->obstacles[i].pos;
		obstaclePos *= REF_SCALE_FACTOR;
		Vec2Int obstacleSize = gameState->obstacles[i].size;
		obstacleSize *= REF_SCALE_FACTOR;
		DrawRect(gameState->rectGL, screenInfo,
			obstaclePos, Vec2 { 0.5f, 1.0f }, obstacleSize,
			Vec4 { 0.0f, 0.0f, 0.0f, 1.0f });
	}
}

extern "C" GAME_UPDATE_AND_RENDER_FUNC(GameUpdateAndRender)
{
	// NOTE: for clarity
	// A call to this function means the following has happened, in order:
	//  1. A frame has been displayed to the user
	//  2. The latest user input has been processed by the platform layer
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

		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
		glCullFace(GL_BACK);

		glLineWidth(2.0f);

		InitAudioState(thread, &gameState->audioState, audio,
			&memory->transient,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		// Game data
		gameState->activeScene = SCENE_TOWN;

		// town data init
		gameState->playerPos = Vec2 { 0.0f, FLOOR_LEVEL + 0.5f };
		gameState->playerVel = Vec2 { 0.0f, 0.0f };
		gameState->currentFloor = nullptr;
		gameState->playerState = PLAYER_STATE_FALLING;
		gameState->facingRight = true;

		gameState->numFloorColliders = 0;
		FloorCollider* floorCollider;

		floorCollider = &gameState->floorColliders[gameState->numFloorColliders++];
		floorCollider->numVertices = 2;
		floorCollider->vertices[0] = { -31.5f, FLOOR_LEVEL + 0.1f };
		floorCollider->vertices[1] = { 31.5f, FLOOR_LEVEL - 0.1f };

        /*floorCollider = &gameState->floorColliders[gameState->numFloorColliders++];
        floorCollider->numVertices = 4;
        floorCollider->vertices[0] = { -8.1f, 1.2f };
        floorCollider->vertices[1] = { -5.4f, 1.05f };
        floorCollider->vertices[2] = { -4.8f, 1.6f };
        floorCollider->vertices[3] = { -2.9f, 1.55f };

        floorCollider = &gameState->floorColliders[gameState->numFloorColliders++];
        floorCollider->numVertices = 2;
        floorCollider->vertices[0] = { -0.22f, 2.75f };
        floorCollider->vertices[1] = { 1.6f, 2.75f };

        floorCollider = &gameState->floorColliders[gameState->numFloorColliders++];
        floorCollider->numVertices = 2;
        floorCollider->vertices[0] = { 1.6f, 1.8f };
        floorCollider->vertices[1] = { 4.45f, 1.83f };*/

		DEBUG_ASSERT(gameState->numFloorColliders <= FLOOR_COLLIDERS_MAX);

        gameState->numWallColliders = 0;
        /*WallCollider* wallCollider;

        wallCollider = &gameState->wallColliders[gameState->numWallColliders++];
        wallCollider->bottomPos = { -10.0f, 0.0f };
        wallCollider->height = 2.0f;*/

        DEBUG_ASSERT(gameState->numWallColliders <= WALL_COLLIDERS_MAX);

		// fishing data init
		gameState->playerPosX = screenInfo.size.x;
		gameState->obstacleTimer = 0.0f;
		gameState->numObstacles = 0;

		gameState->grainTime = 0.0f;

#if GAME_INTERNAL
		gameState->debugView = true;
		gameState->editor = false;
#endif

		// Rendering stuff
		InitRenderState(gameState->renderState, thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

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
        gameState->lutShader = LoadShaders(thread,
            "shaders/screen.vert", "shaders/lut.frag",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);

        gameState->background.pos = { 0.0f, -5.5f };
        gameState->background.anchor = { 0.5f, 0.0f };
        gameState->background.scale = 1.0f;
        bool32 loadBackground = LoadPNGOpenGL(thread,
            "data/sprites/quarry3.png",
            GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            gameState->background.texture,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadBackground) {
            DEBUG_PANIC("Failed to load background");
        }

        bool32 loadKidAnim = LoadAnimatedSprite(thread,
            "data/animations/kid/kid.kma",
            gameState->spriteKid,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadKidAnim) {
            DEBUG_PANIC("Failed to load kid animation sprite");
        }

        /*gameState->tractor1.pos = { 2.0f, -0.2f };
        gameState->tractor1.anchor = { 0.5f, 0.0f };
        gameState->tractor1.scale = 1.0f;
        bool32 loadTractor1 = LoadPNGOpenGL(thread,
            "data/sprites/tractor1.png",
            GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            gameState->tractor1.texture,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadTractor1) {
            DEBUG_PANIC("Failed to load tractor 1");
        }
        gameState->tractor2.pos = { -5.0f, -0.45f };
        gameState->tractor2.anchor = { 0.5f, 0.0f };
        gameState->tractor2.scale = 1.0f;
        bool32 loadTractor2 = LoadPNGOpenGL(thread,
            "data/sprites/tractor2.png",
            GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            gameState->tractor2.texture,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadTractor2) {
            DEBUG_PANIC("Failed to load tractor 2");
        }*/

        bool32 loadPaperAnim = LoadAnimatedSprite(thread,
            "data/animations/paper/paper.kma",
            gameState->spritePaper,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadPaperAnim) {
            DEBUG_PANIC("Failed to load paper animation sprite");
        }

        bool32 loadFrame = LoadPNGOpenGL(thread,
            "data/sprites/frame.png",
            GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            gameState->frame.texture,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadFrame) {
            DEBUG_PANIC("Failed to load frame");
        }

        bool32 loadLutBase = LoadPNGOpenGL(thread,
            "data/luts/lutbase.png",
            GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            gameState->lutBase,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadLutBase) {
            DEBUG_PANIC("Failed to load base LUT");
        }

        bool32 loadLut1 = LoadPNGOpenGL(thread,
            "data/luts/kodak5205.png",
            GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
            gameState->lut1,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadLut1) {
            DEBUG_PANIC("Failed to load base LUT");
        }

#if GAME_INTERNAL
		/*gameState->guys.box = CreateClickableBox(gameState->guys.pos,
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
		);*/
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
			if (gameState->editor) {
				if (input->mouseButtons[0].isDown) {
					gameState->cameraPos -= ScreenToWorldScaleOnly(input->mouseDelta, screenInfo);
				}
			}
			UpdateTown(gameState, deltaTime, input);
		} break;
		case SCENE_FISHING: {
			UpdateFishing(gameState, deltaTime, input, screenInfo);
		} break;
	}

	// ---------------------------- Begin Rendering ---------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColorDepth[0].framebuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Mat4 worldMatrix = CalculateWorldMatrix(screenInfo);

	DEBUG_ASSERT(memory->transient.size >= sizeof(SpriteDataGL));
	SpriteDataGL* spriteDataGL = (SpriteDataGL*)memory->transient.memory;

	switch (gameState->activeScene) {
		case SCENE_TOWN: {
			DrawTown(gameState, spriteDataGL, worldMatrix, screenInfo);
		} break;
		case SCENE_FISHING: {
			DrawFishing(gameState, spriteDataGL, screenInfo);
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

    PostProcessLUT(gameState->framebuffersColorDepth[0],
        gameState->framebuffersColor[0],
        gameState->screenQuadVertexArray,
        gameState->lutShader, gameState->lut1);

	// Render to screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindVertexArray(gameState->screenQuadVertexArray);
	glUseProgram(gameState->screenShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gameState->framebuffersColor[0].color);
	GLint loc = glGetUniformLocation(gameState->screenShader, "framebufferTexture");
	glUniform1i(loc, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	// Render pillarbox
	const Vec4 PILLARBOX_COLOR = { 0.965f, 0.957f, 0.91f, 1.0f };
	int pillarboxWidth = GetPillarboxWidth(screenInfo);
	Vec2Int pillarboxPos1 = Vec2Int::zero;
	Vec2Int pillarboxPos2 = { screenInfo.size.x - pillarboxWidth, 0 };
	Vec2 pillarboxAnchor = Vec2 { 0.0f, 0.0f };
	Vec2Int pillarboxSize = { pillarboxWidth, screenInfo.size.y };
	if (pillarboxWidth > 0) {
		/*DrawRect(gameState->rectGL, screenInfo,
			pillarboxPos1, pillarboxAnchor, pillarboxSize, PILLARBOX_COLOR);
		DrawRect(gameState->rectGL, screenInfo,
			pillarboxPos2, pillarboxAnchor, pillarboxSize, PILLARBOX_COLOR);*/
	}

	// DrawRect(gameState->rectGL, screenInfo,
	// 	input->mousePos, Vec2 { 0.5f, 0.5f }, Vec2Int { 5, 5 },
	// 	Vec4 { 0.1f, 0.1f, 0.1f, 1.0f });

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
	const Vec2Int MARGIN = { 30, 45 };

	if (gameState->debugView) {
		FontFace& textFont = gameState->fontFaceSmall;
		FontFace& textFontSmall = gameState->fontFaceSmall;
		char textStr[128];
		Vec2Int textPosLeft = {
			pillarboxWidth + MARGIN.x,
			screenInfo.size.y - MARGIN.y,
		};
		Vec2Int textPosRight = {
			screenInfo.size.x - pillarboxWidth - MARGIN.x,
			screenInfo.size.y - MARGIN.y,
		};

		sprintf(textStr, "[G] to toggle debug view");
		DrawText(gameState->textGL, textFontSmall, screenInfo,
			textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosLeft.y -= textFontSmall.height;
		sprintf(textStr, "[H] to toggle editor");
		DrawText(gameState->textGL, textFontSmall, screenInfo,
			textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosLeft.y -= textFontSmall.height;
		textPosLeft.y -= textFontSmall.height;
		sprintf(textStr, "[K] to toggle debug audio view");
		DrawText(gameState->textGL, textFontSmall, screenInfo,
			textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

        textPosLeft.y -= textFontSmall.height;
        sprintf(textStr, "[M] to toggle global audio mute");
        DrawText(gameState->textGL, textFontSmall, screenInfo,
            textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

        textPosLeft.y -= textFontSmall.height;
        textPosLeft.y -= textFontSmall.height;
        sprintf(textStr, "[F11] to toggle fullscreen");
        DrawText(gameState->textGL, textFontSmall, screenInfo,
            textStr, textPosLeft, Vec2 { 0.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		sprintf(textStr, "%.2f --- FPS", 1.0f / deltaTime);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		textPosRight.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f --- POS", gameState->playerPos.x, gameState->playerPos.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f --- VEL", gameState->playerVel.x, gameState->playerVel.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		sprintf(textStr, "%d - STATE", gameState->playerState);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

        textPosRight.y -= textFont.height;
        textPosRight.y -= textFont.height;
        sprintf(textStr, "%.2f|%.2f --- CAM", gameState->cameraPos.x, gameState->cameraPos.y);
        DrawText(gameState->textGL, textFont, screenInfo,
            textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

        textPosRight.y -= textFont.height;
        textPosRight.y -= textFont.height;
        Vec2 mouseWorld = ScreenToWorld(input->mousePos, screenInfo, gameState->cameraPos);
        sprintf(textStr, "%.2f|%.2f - MOUSE", mouseWorld.x, mouseWorld.y);
        DrawText(gameState->textGL, textFont, screenInfo,
            textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

        if (input->mouseButtons[0].isDown) {
            DEBUG_PRINT("Mouse: %.2f, %.2f\n", mouseWorld.x, mouseWorld.y);
        }

		DEBUG_ASSERT(memory->transient.size >= sizeof(LineGLData));
		LineGLData* lineData = (LineGLData*)memory->transient.memory;

        Vec3 cameraPos = { gameState->cameraPos.x, gameState->cameraPos.y, 0.0f };
        Mat4 view = Translate(CAMERA_OFFSET_VEC3 + -cameraPos);

		Vec4 floorColliderColor = { 0.6f, 0.0f, 0.6f, 1.0f };
		for (int i = 0; i < gameState->numFloorColliders; i++) {
			FloorCollider& floor = gameState->floorColliders[i];
			DEBUG_ASSERT(floor.numVertices <= MAX_LINE_POINTS);

			lineData->count = floor.numVertices;
			for (int v = 0; v < floor.numVertices; v++) {
				lineData->pos[v] = Vec3 {
					floor.vertices[v].x,
					floor.vertices[v].y,
					0.0f
				};
			}
			DrawLine(gameState->lineGL, worldMatrix, view, lineData, floorColliderColor);
		}

		Vec4 currentFloorColor = { 1.0f, 0.0f, 1.0f, 1.0f };
		const FloorCollider* floorCollider = gameState->currentFloor;
		if (floorCollider != nullptr) {
			DEBUG_ASSERT(floorCollider->numVertices <= MAX_LINE_POINTS);

			lineData->count = floorCollider->numVertices;
			for (int v = 0; v < floorCollider->numVertices; v++) {
				lineData->pos[v] = Vec3 {
					floorCollider->vertices[v].x,
					floorCollider->vertices[v].y,
					0.0f
				};
			}
			DrawLine(gameState->lineGL, worldMatrix, view, lineData, currentFloorColor);
		}

		Vec4 wallColliderColor = { 0.0f, 0.6f, 0.6f, 1.0f };
		for (int i = 0; i < gameState->numWallColliders; i++) {
			WallCollider* wall = &gameState->wallColliders[i];
            
			lineData->count = 2;
            lineData->pos[0] = Vec3 { wall->bottomPos.x, wall->bottomPos.y, 0.0f };
            lineData->pos[1] = lineData->pos[0];
            lineData->pos[1].y += wall->height;
			DrawLine(gameState->lineGL, worldMatrix, view, lineData, wallColliderColor);
		}

		// Player position cross
		lineData->count = 2;
		float32 crossSize = 0.1f;
		Vec3 playerPos = { gameState->playerPos.x, gameState->playerPos.y, 0.0f };
		Vec4 playerPosColor = { 1.0f, 0.0f, 1.0f, 1.0f };

		lineData->pos[0] = playerPos;
		lineData->pos[0].x -= crossSize;
		lineData->pos[1] = playerPos;
		lineData->pos[1].x += crossSize;
		DrawLine(gameState->lineGL, worldMatrix, view, lineData, playerPosColor);

		lineData->pos[0] = playerPos;
		lineData->pos[0].y -= crossSize;
		lineData->pos[1] = playerPos;
		lineData->pos[1].y += crossSize;
		DrawLine(gameState->lineGL, worldMatrix, view, lineData, playerPosColor);

		// Player collision box
		lineData->count = 5;
		Vec4 playerColliderColor = { 1.0f, 0.4f, 0.0f, 1.0f };

		lineData->pos[0] = playerPos;
		lineData->pos[0].x -= PLAYER_RADIUS;
		lineData->pos[1] = playerPos;
		lineData->pos[1].x += PLAYER_RADIUS;
		lineData->pos[2] = lineData->pos[1];
		lineData->pos[2].y += PLAYER_HEIGHT;
		lineData->pos[3] = lineData->pos[0];
		lineData->pos[3].y += PLAYER_HEIGHT;
		lineData->pos[4] = lineData->pos[0];
		DrawLine(gameState->lineGL, worldMatrix, view, lineData, playerColliderColor);
	}
	if (gameState->editor) {
		Vec2Int editorStrPos = {
			pillarboxWidth + MARGIN.x,
			MARGIN.y,
		};
		Vec4 editorFontColor = { 1.0f, 0.1f, 1.0f, 1.0f };
		DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
			"EDITOR", editorStrPos, Vec2 { 0.0f, 0.0f }, editorFontColor, memory->transient);
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
#include "render.cpp"
#include "text.cpp"