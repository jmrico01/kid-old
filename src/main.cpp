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
#define CAMERA_OFFSET_Y 0.0f
//#define CAMERA_OFFSET_Y (-CAMERA_HEIGHT / 6.0f)
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

    if (refPos.x < floorCollider->vertices[0].x) {
        *outHeight = floorCollider->vertices[0].y;
    }
    if (refPos.x > floorCollider->vertices[floorCollider->numVertices - 1].x) {
        *outHeight = floorCollider->vertices[floorCollider->numVertices - 1].y;
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

void GetFloorInfo(const FloorCollider& floorCollider, float32 tX,
    Vec2* outFloorPos, Vec2* outTangent, Vec2* outNormal)
{
    DEBUG_ASSERT(floorCollider.numVertices >= 2);

    const int EDGE_NEIGHBORS = 3;
    float32 edgeWeights[EDGE_NEIGHBORS * 2 + 1];
    for (int n = -EDGE_NEIGHBORS; n <= EDGE_NEIGHBORS; n++) {
        edgeWeights[n + EDGE_NEIGHBORS] = powf(E_F, (float32)(n * n));
    }

    *outFloorPos = Vec2::zero;
    *outTangent = Vec2::unitX;
    *outNormal = Vec2::unitY;

    float32 t = 0.0f;
    Vec2 dir = floorCollider.vertices[1] - floorCollider.vertices[0];
    for (int i = 1; i < floorCollider.numVertices; i++) {
        Vec2 edge = floorCollider.vertices[i] - floorCollider.vertices[i - 1];
        float32 edgeLength = Mag(edge);
        if (t + edgeLength >= tX) {
            Vec2 sumTangents = Vec2::zero;
            Vec2 sumNormals = Vec2::zero;
            for (int n = -EDGE_NEIGHBORS; n <= EDGE_NEIGHBORS; n++) {
                int ind1 = ClampInt(i + n - 1, 0, floorCollider.numVertices - 1);
                int ind2 = ClampInt(i + n,     1, floorCollider.numVertices);
                Vec2 edgeTangent = Normalize(floorCollider.vertices[ind2]
                    - floorCollider.vertices[ind1]);
                sumTangents += edgeTangent * edgeWeights[n + EDGE_NEIGHBORS];
                sumNormals += Vec2 { -edgeTangent.y, edgeTangent.x }
                    * edgeWeights[n + EDGE_NEIGHBORS];
            }
            *outFloorPos = floorCollider.vertices[i - 1] + edge / edgeLength * (tX - t);
            *outTangent = Normalize(sumTangents);
            *outNormal = Normalize(sumNormals);
            return;
        }

        t += edgeLength;
    }
}

Vec2 FloorCoordsToWorldPos(const FloorCollider& floorCollider, Vec2 coords)
{
    Vec2 floorPos, floorTangent, floorNormal;
    GetFloorInfo(floorCollider, coords.x, &floorPos, &floorTangent, &floorNormal);
    return floorPos + floorNormal * coords.y;
}

void PlayerMovementInput(GameState* gameState, float32 deltaTime, const GameInput* input)
{
	const float32 PLAYER_WALK_SPEED = 3.5f;
    const float32 PLAYER_JUMP_HOLD_DURATION_MIN = 0.02f;
    const float32 PLAYER_JUMP_HOLD_DURATION_MAX = 0.3f;
    const float32 PLAYER_JUMP_MAG_MAX = 1.5f;
    const float32 PLAYER_JUMP_MAG_MIN = 0.5f;

    float32 speed = PLAYER_WALK_SPEED;
    if (gameState->playerState == PLAYER_STATE_JUMPING) {
        speed /= Lerp(gameState->playerJumpMag, 1.0f, 0.3f);
    }

    gameState->playerVel.x = 0.0f;
	if (IsKeyPressed(input, KM_KEY_A) || IsKeyPressed(input, KM_KEY_ARROW_LEFT)) {
		gameState->playerVel.x = -speed;
        gameState->facingRight = false;
	}
	if (IsKeyPressed(input, KM_KEY_D) || IsKeyPressed(input, KM_KEY_ARROW_RIGHT)) {
		gameState->playerVel.x = speed;
        gameState->facingRight = true;
	}
    if (input->controllers[0].isConnected) {
        float32 leftStickX = input->controllers[0].leftEnd.x;
        if (leftStickX < 0.0f) {
            gameState->playerVel.x = -speed;
            gameState->facingRight = false;
        }
        else if (leftStickX > 0.0f) {
            gameState->playerVel.x = speed;
            gameState->facingRight = true;
        }
    }

	HashKey ANIM_LAND;
	ANIM_LAND.WriteString("Land");

    bool jumpPressed = IsKeyPressed(input, KM_KEY_SPACE)
        || IsKeyPressed(input, KM_KEY_ARROW_UP)
        || (input->controllers[0].isConnected && input->controllers[0].a.isDown);
	if (gameState->playerState == PLAYER_STATE_GROUNDED && jumpPressed
	&& !KeyCompare(gameState->kid.activeAnimation, ANIM_LAND)) {
		gameState->playerState = PLAYER_STATE_JUMPING;
        gameState->playerJumpHolding = true;
        gameState->playerJumpHold = 0.0f;
        gameState->playerJumpMag = PLAYER_JUMP_MAG_MAX;
		gameState->audioState.soundKick.playing = true;
		gameState->audioState.soundKick.sampleIndex = 0;
	}

    if (gameState->playerJumpHolding) {
        gameState->playerJumpHold += deltaTime;
        if (gameState->playerState == PLAYER_STATE_JUMPING && !jumpPressed) {
            gameState->playerJumpHolding = false;
            float32 timeT = gameState->playerJumpHold - PLAYER_JUMP_HOLD_DURATION_MIN
                / (PLAYER_JUMP_HOLD_DURATION_MAX - PLAYER_JUMP_HOLD_DURATION_MIN);
            timeT = ClampFloat32(timeT, 0.0f, 1.0f);
            timeT = sqrtf(timeT);
            gameState->playerJumpMag = Lerp(PLAYER_JUMP_MAG_MIN, PLAYER_JUMP_MAG_MAX, timeT);
        }
    }
}

void UpdateTown(GameState* gameState, float32 deltaTime, const GameInput* input)
{
#if GAME_INTERNAL
	if (!gameState->editor) {
		PlayerMovementInput(gameState, deltaTime, input);
	}
#else
	PlayerMovementInput(gameState, deltaTime, input);
#endif

#if 0
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
		if (!KeyCompare(gameState->kid.activeAnimation, ANIM_JUMP)) {
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
	&& KeyCompare(gameState->kid.activeAnimation, ANIM_FALL)) {
		gameState->playerState = PLAYER_STATE_FALLING;
	}

    float32 animDeltaTime = deltaTime;
    if (gameState->playerState == PLAYER_STATE_JUMPING) {
        deltaTime /= Lerp(gameState->playerJumpMag, 1.0f, 0.5f);
    }
	const HashKey* nextAnimations = nextAnims;
	Vec2 rootMotion = gameState->kid.Update(deltaTime, numNextAnims, nextAnimations);
    if (gameState->playerState == PLAYER_STATE_JUMPING) {
        rootMotion *= gameState->playerJumpMag;
    }

	Vec2 deltaPos = gameState->playerVel * deltaTime + rootMotion;
	Vec2 newPlayerPos = gameState->playerPos + deltaPos;

	FloorColliderIntersect intersects[FLOOR_COLLIDERS_MAX];
	int numIntersects;
	GetFloorColliderIntersections(gameState->floorColliders, gameState->numFloorColliders,
		gameState->playerPos, deltaPos, 0.05f,
		intersects, &numIntersects);
	if (deltaPos.y < 0.0f && numIntersects > 0) {
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

                // TODO this magic "5" just has to be greater than walk speed
                newPlayerPos.x += pushDir * 5.0f * deltaTime;
            }
        }
    }

    float32 height = 0.0f;
    bool isOverFloor = false;
    if (gameState->currentFloor != nullptr) {
        isOverFloor = GetFloorColliderHeight(gameState->currentFloor, newPlayerPos, &height);
    }

	if (gameState->playerState == PLAYER_STATE_GROUNDED) {
		if (isOverFloor) {
            newPlayerPos.y = height;
		}
		else {
            gameState->playerState = PLAYER_STATE_FALLING;
		}
	}
#endif
	gameState->playerPos += gameState->playerVel * deltaTime;

    gameState->paper.Update(deltaTime, 0, nullptr);

#if GAME_INTERNAL
	if (gameState->editor) {
        return;
	}
#endif

    /*int numBarrelNextAnims = 0;
    HashKey barrelNextAnims[1];
    if (WasKeyPressed(input, KM_KEY_Z)) {
        numBarrelNextAnims = 1;
        barrelNextAnims[0].WriteString("Carry");
    }
    if (WasKeyPressed(input, KM_KEY_X)) {
        numBarrelNextAnims = 1;
        barrelNextAnims[0].WriteString("Explode");
    }
    gameState->barrel.Update(deltaTime, numBarrelNextAnims, barrelNextAnims);*/

    /*int numCrystalNextAnims = 0;
    HashKey crystalNextAnims[1];
    if (WasKeyPressed(input, KM_KEY_C)) {
        numCrystalNextAnims = 1;
        crystalNextAnims[0].WriteString("Explode");
    }
    gameState->spriteCrystal.Update(deltaTime, numCrystalNextAnims, crystalNextAnims);*/

    Vec2 cameraTarget = FloorCoordsToWorldPos(gameState->floorColliders[0], gameState->playerPos);
    #if 0
    if (cameraTarget.y > height) {
        cameraTarget.y = height;
    }
    #endif

    const float32 CAMERA_FOLLOW_LERP_MAG = 0.08f;
    const float32 CAMERA_MIN_Y = -2.6f;
    const float32 CAMERA_MAX_Y = 3.8f;

    gameState->cameraPos = Lerp(gameState->cameraPos, cameraTarget, CAMERA_FOLLOW_LERP_MAG);
    gameState->cameraPos = cameraTarget;
    //gameState->cameraPos.y = ClampFloat32(gameState->cameraPos.y, CAMERA_MIN_Y, CAMERA_MAX_Y);
}

void DrawTown(GameState* gameState, SpriteDataGL* spriteDataGL,
    Mat4 worldMatrix, ScreenInfo screenInfo)
{
#if GAME_INTERNAL
    if (gameState->editor) {
        worldMatrix = worldMatrix * Scale(gameState->editorWorldScale);
    }
#endif
    spriteDataGL->numSprites = 0;

    DrawObjectStatic(gameState->background, spriteDataGL);

    //DrawObjectStatic(gameState->tractor1, spriteDataGL);
    //DrawObjectStatic(gameState->tractor2, spriteDataGL);

    /*Vec2 barrelSize = ToVec2(gameState->barrel.animatedSprite->textureSize) / REF_PIXELS_PER_UNIT;
    gameState->barrel.Draw(spriteDataGL, Vec2 { 1.0f, -1.0f }, barrelSize, Vec2::zero,
        1.0f, false);*/

    /*Vec2 crystalSize = ToVec2(gameState->spriteCrystal.textureSize) / REF_PIXELS_PER_UNIT;
    gameState->spriteCrystal.Draw(spriteDataGL, Vec2 { -3.0f, -1.0f }, crystalSize, Vec2::zero,
        1.0f, false);*/

    Vec2 floorPos, floorTangent, floorNormal;
    GetFloorInfo(gameState->floorColliders[0], gameState->playerPos.x,
        &floorPos, &floorTangent, &floorNormal);
	Mat4 view = Translate(CAMERA_OFFSET_VEC3 + -ToVec3(gameState->cameraPos, 0.0f));
    Mat4 viewRotated = UnitQuatToMat4(QuatRotBetweenVectors(
        ToVec3(floorNormal, 0.0f), Vec3::unitY)) * view;
	DrawSprites(gameState->renderState, *spriteDataGL, viewRotated, worldMatrix);

    spriteDataGL->numSprites = 0;

    { // kid & me text
        Vec2 pos = FloorCoordsToWorldPos(gameState->floorColliders[0], gameState->playerPos);
        Vec2 anchor = { 0.5f, 0.1f };
        Vec2 size = ToVec2(gameState->kid.animatedSprite->textureSize) / REF_PIXELS_PER_UNIT;
        // Vec2 meTextOffset = { -0.1f, 0.1f };

        gameState->kid.Draw(spriteDataGL, pos, size,
            anchor, 1.0f, !gameState->facingRight);
    }

    DrawSprites(gameState->renderState, *spriteDataGL, view, worldMatrix);

#if GAME_INTERNAL
    if (gameState->editor) {
        return;
    }
#endif

    spriteDataGL->numSprites = 0;

    const float32 ASPECT_RATIO = (float32)screenInfo.size.x / screenInfo.size.y;
    Vec2 screenSizeWorld = { CAMERA_HEIGHT * ASPECT_RATIO, CAMERA_HEIGHT };

    gameState->paper.Draw(spriteDataGL,
        Vec2::zero, screenSizeWorld, Vec2::one / 2.0f, 0.5f,
        false);

    gameState->frame.pos = Vec2::zero;
    gameState->frame.anchor = Vec2::one / 2.0f;
    gameState->frame.scale = 1.0f;
    DrawObjectStatic(gameState->frame, spriteDataGL);

    DrawSprites(gameState->renderState, *spriteDataGL, Mat4::one, worldMatrix);
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
        gameState->playerPos = Vec2 { 0.5f, 0.0f };
        gameState->playerVel = Vec2::zero;
        gameState->cameraPos = gameState->playerVel;
        gameState->cameraVelY = 0.0f;
		gameState->currentFloor = nullptr;
		gameState->playerState = PLAYER_STATE_FALLING;
		gameState->facingRight = true;

		gameState->numFloorColliders = 0;
		FloorCollider* floorCollider;

        floorCollider = &gameState->floorColliders[gameState->numFloorColliders++];
        floorCollider->numVertices = 0;
        floorCollider->vertices[floorCollider->numVertices++] = { -16.36f, 45.39f };
        floorCollider->vertices[floorCollider->numVertices++] = { -14.33f, 45.36f };
        floorCollider->vertices[floorCollider->numVertices++] = { -11.56f, 45.45f };
        floorCollider->vertices[floorCollider->numVertices++] = { -8.43f, 45.40f };
        floorCollider->vertices[floorCollider->numVertices++] = { -4.93f, 45.36f };
        floorCollider->vertices[floorCollider->numVertices++] = { -0.96f, 45.36f };
        floorCollider->vertices[floorCollider->numVertices++] = { 2.18f, 45.36f };
        floorCollider->vertices[floorCollider->numVertices++] = { 5.38f, 45.38f };
        floorCollider->vertices[floorCollider->numVertices++] = { 8.31f, 45.41f };
        floorCollider->vertices[floorCollider->numVertices++] = { 10.33f, 45.37f };
        floorCollider->vertices[floorCollider->numVertices++] = { 13.80f, 45.28f };
        floorCollider->vertices[floorCollider->numVertices++] = { 17.09f, 45.28f };
        floorCollider->vertices[floorCollider->numVertices++] = { 18.52f, 44.97f };
        floorCollider->vertices[floorCollider->numVertices++] = { 19.60f, 44.57f };
        floorCollider->vertices[floorCollider->numVertices++] = { 20.65f, 44.04f };
        floorCollider->vertices[floorCollider->numVertices++] = { 21.46f, 43.52f };
        floorCollider->vertices[floorCollider->numVertices++] = { 22.05f, 42.96f };
        floorCollider->vertices[floorCollider->numVertices++] = { 22.58f, 42.33f };
        floorCollider->vertices[floorCollider->numVertices++] = { 23.09f, 41.69f };
        floorCollider->vertices[floorCollider->numVertices++] = { 23.58f, 41.06f };
        floorCollider->vertices[floorCollider->numVertices++] = { 23.97f, 40.19f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.26f, 39.40f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.42f, 38.60f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.57f, 37.61f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.64f, 36.96f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.61f, 35.82f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.66f, 34.09f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.76f, 32.13f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.65f, 29.76f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.64f, 27.01f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.57f, 25.14f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.64f, 22.89f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.79f, 21.78f };
        floorCollider->vertices[floorCollider->numVertices++] = { 24.93f, 21.04f };
        floorCollider->vertices[floorCollider->numVertices++] = { 25.10f, 20.39f };
        floorCollider->vertices[floorCollider->numVertices++] = { 25.44f, 19.76f };
        floorCollider->vertices[floorCollider->numVertices++] = { 25.72f, 19.15f };
        floorCollider->vertices[floorCollider->numVertices++] = { 26.04f, 18.60f };
        floorCollider->vertices[floorCollider->numVertices++] = { 26.18f, 18.49f };
        floorCollider->vertices[floorCollider->numVertices++] = { 26.63f, 17.76f };
        floorCollider->vertices[floorCollider->numVertices++] = { 27.27f, 17.18f };
        floorCollider->vertices[floorCollider->numVertices++] = { 27.72f, 16.75f };
        floorCollider->vertices[floorCollider->numVertices++] = { 28.34f, 16.29f };
        floorCollider->vertices[floorCollider->numVertices++] = { 28.99f, 15.88f };
        floorCollider->vertices[floorCollider->numVertices++] = { 29.58f, 15.58f };
        floorCollider->vertices[floorCollider->numVertices++] = { 30.24f, 15.24f };
        floorCollider->vertices[floorCollider->numVertices++] = { 31.05f, 15.05f };
        floorCollider->vertices[floorCollider->numVertices++] = { 32.19f, 14.83f };
        floorCollider->vertices[floorCollider->numVertices++] = { 33.27f, 14.76f };
        floorCollider->vertices[floorCollider->numVertices++] = { 34.79f, 14.87f };
        floorCollider->vertices[floorCollider->numVertices++] = { 36.21f, 14.82f };
        floorCollider->vertices[floorCollider->numVertices++] = { 38.38f, 14.84f };
        floorCollider->vertices[floorCollider->numVertices++] = { 41.54f, 14.88f };
        floorCollider->vertices[floorCollider->numVertices++] = { 43.48f, 14.88f };
        floorCollider->vertices[floorCollider->numVertices++] = { 45.74f, 14.79f };
        floorCollider->vertices[floorCollider->numVertices++] = { 47.27f, 14.61f };
        floorCollider->vertices[floorCollider->numVertices++] = { 48.13f, 14.43f };
        floorCollider->vertices[floorCollider->numVertices++] = { 48.90f, 14.14f };
        floorCollider->vertices[floorCollider->numVertices++] = { 49.58f, 13.85f };
        floorCollider->vertices[floorCollider->numVertices++] = { 50.56f, 13.45f };
        floorCollider->vertices[floorCollider->numVertices++] = { 51.27f, 12.91f };
        floorCollider->vertices[floorCollider->numVertices++] = { 51.85f, 12.35f };
        floorCollider->vertices[floorCollider->numVertices++] = { 52.41f, 11.78f };
        floorCollider->vertices[floorCollider->numVertices++] = { 52.84f, 11.04f };
        floorCollider->vertices[floorCollider->numVertices++] = { 53.14f, 10.35f };
        floorCollider->vertices[floorCollider->numVertices++] = { 53.53f, 9.63f };
        floorCollider->vertices[floorCollider->numVertices++] = { 53.85f, 8.88f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.10f, 7.65f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.22f, 6.57f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.30f, 5.42f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.28f, 3.18f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.32f, 0.89f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.43f, -1.98f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.38f, -4.91f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.38f, -6.79f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.34f, -9.02f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.38f, -10.78f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.30f, -14.38f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.31f, -16.44f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.26f, -17.57f };
        floorCollider->vertices[floorCollider->numVertices++] = { 54.26f, -18.35f };

		gameState->grainTime = 0.0f;

#if GAME_INTERNAL
		gameState->debugView = false;
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
        gameState->background.anchor = { 0.5f, 0.5f };
        gameState->background.scale = 1.0f;
        bool32 loadBackground = LoadPNGOpenGL(thread,
            "data/sprites/playground.png",
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

        /*bool32 loadBarrelAnim = LoadAnimatedSprite(thread,
            "data/animations/barrel/barrel.kma",
            gameState->spriteBarrel,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadBarrelAnim) {
            DEBUG_PANIC("Failed to load barrel animation sprite");
        }

        bool32 loadCrystalAnim = LoadAnimatedSprite(thread,
            "data/animations/crystal/crystal.kma",
            gameState->spriteCrystal,
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory);
        if (!loadCrystalAnim) {
            DEBUG_PANIC("Failed to load crystal animation sprite");
        }*/

        gameState->kid.animatedSprite = &gameState->spriteKid;
        gameState->kid.activeAnimation = gameState->spriteKid.startAnimation;
        gameState->kid.activeFrame = 0;
        gameState->kid.activeFrameRepeat = 0;
        gameState->kid.activeFrameTime = 0.0f;

        gameState->barrel.animatedSprite = &gameState->spriteBarrel;
        gameState->barrel.activeAnimation = gameState->spriteBarrel.startAnimation;
        gameState->barrel.activeFrame = 0;
        gameState->barrel.activeFrameRepeat = 0;
        gameState->barrel.activeFrameTime = 0.0f;

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

        gameState->paper.animatedSprite = &gameState->spritePaper;
        gameState->paper.activeAnimation = gameState->spritePaper.startAnimation;
        gameState->paper.activeFrame = 0;
        gameState->paper.activeFrameRepeat = 0;
        gameState->paper.activeFrameTime = 0.0f;

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

	// Toggle global mute
	if (WasKeyPressed(input, KM_KEY_M)) {
		gameState->audioState.globalMute = !gameState->audioState.globalMute;
	}

	if (gameState->editor) {
		if (input->mouseButtons[0].isDown) {
			gameState->cameraPos -= ScreenToWorldScaleOnly(input->mouseDelta, screenInfo)
                / gameState->editorWorldScale;
		}
        float32 editorWorldScaleDelta = input->mouseWheelDelta * 0.001f;
        gameState->editorWorldScale = ClampFloat32(
            gameState->editorWorldScale + editorWorldScaleDelta, 0.01f, 10.0f);
	}

	UpdateTown(gameState, deltaTime, input);

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

	DrawTown(gameState, spriteDataGL, worldMatrix, screenInfo);

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
        gameState->editorWorldScale = 1.0f;
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
        sprintf(textStr, "%.2f|%.2f -- CRDS", gameState->playerPos.x, gameState->playerPos.y);
        DrawText(gameState->textGL, textFont, screenInfo,
            textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

        textPosRight.y -= textFont.height;
        Vec2 playerPosWorld = FloorCoordsToWorldPos(gameState->floorColliders[0],
            gameState->playerPos);
        sprintf(textStr, "%.2f|%.2f --- POS", playerPosWorld.x, playerPosWorld.y);
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
        gameState->editorWorldScale = 1.0f;
        Vec2 mouseWorld = ScreenToWorld(input->mousePos, screenInfo, gameState->cameraPos);
        sprintf(textStr, "%.2f|%.2f - MOUSE", mouseWorld.x, mouseWorld.y);
        DrawText(gameState->textGL, textFont, screenInfo,
            textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

        if (input->mouseButtons[1].isDown && input->mouseButtons[1].transitions == 1) {
            DEBUG_PRINT("Mouse: %.2f, %.2f\n", mouseWorld.x, mouseWorld.y);
        }

		DEBUG_ASSERT(memory->transient.size >= sizeof(LineGLData));
		LineGLData* lineData = (LineGLData*)memory->transient.memory;

        Vec2 floorPos, floorTangent, floorNormal;
        GetFloorInfo(gameState->floorColliders[0], gameState->playerPos.x,
            &floorPos, &floorTangent, &floorNormal);
        Mat4 view = Translate(CAMERA_OFFSET_VEC3 + -ToVec3(gameState->cameraPos, 0.0f));
        Mat4 viewRotated = UnitQuatToMat4(QuatRotBetweenVectors(
            ToVec3(floorNormal, 0.0f), Vec3::unitY)) * view;

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
			DrawLine(gameState->lineGL, worldMatrix, viewRotated, lineData, floorColliderColor);
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
			DrawLine(gameState->lineGL, worldMatrix, viewRotated, lineData, currentFloorColor);
		}

		Vec4 wallColliderColor = { 0.0f, 0.6f, 0.6f, 1.0f };
		for (int i = 0; i < gameState->numWallColliders; i++) {
			WallCollider* wall = &gameState->wallColliders[i];
            
			lineData->count = 2;
            lineData->pos[0] = Vec3 { wall->bottomPos.x, wall->bottomPos.y, 0.0f };
            lineData->pos[1] = lineData->pos[0];
            lineData->pos[1].y += wall->height;
			DrawLine(gameState->lineGL, worldMatrix, viewRotated, lineData, wallColliderColor);
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
		DrawLine(gameState->lineGL, worldMatrix, viewRotated, lineData, playerPosColor);

		lineData->pos[0] = playerPos;
		lineData->pos[0].y -= crossSize;
		lineData->pos[1] = playerPos;
		lineData->pos[1].y += crossSize;
		DrawLine(gameState->lineGL, worldMatrix, viewRotated, lineData, playerPosColor);

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
		DrawLine(gameState->lineGL, worldMatrix, viewRotated, lineData, playerColliderColor);
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