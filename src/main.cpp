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
#include "temp_data.h"

#define CAMERA_HEIGHT_UNITS ((REF_PIXEL_SCREEN_HEIGHT) / (REF_PIXELS_PER_UNIT))
#define CAMERA_OFFSET_Y (-CAMERA_HEIGHT_UNITS / 6.0f)
#define CAMERA_OFFSET_VEC3 (Vec3 { 0.0f, CAMERA_OFFSET_Y, 0.0f })

#define PLAYER_RADIUS 0.4f
#define PLAYER_HEIGHT 1.3f

#define LINE_COLLIDER_MARGIN 0.05f

inline float32 RandFloat32()
{
	return (float32)rand() / RAND_MAX;
}
inline float32 RandFloat32(float32 min, float32 max)
{
	DEBUG_ASSERT(max > min);
	return RandFloat32() * (max - min) + min;
}

inline int RandInt(int min, int max)
{
	DEBUG_ASSERT(max > min);
	return rand() % (max - min) + min;
}

#if GAME_INTERNAL
internal float32 ScaleExponentToWorldScale(float32 exponent)
{
	const float32 SCALE_MIN = 0.1f;
	const float32 SCALE_MAX = 10.0f;
	const float32 c = (SCALE_MAX * SCALE_MIN - 1.0f) / (SCALE_MAX + SCALE_MIN - 2.0f);
	const float32 a = SCALE_MIN - c;
	const float32 b = log2f((SCALE_MAX - c) / a);
	return a * powf(2.0f, b * exponent) + c;
}
#endif

int GetPillarboxWidth(ScreenInfo screenInfo)
{
	int targetWidth = (int)((float32)screenInfo.size.y * TARGET_ASPECT_RATIO);
	return (screenInfo.size.x - targetWidth) / 2;
}

internal Mat4 CalculateProjectionMatrix(ScreenInfo screenInfo)
{
	float32 aspectRatio = (float32)screenInfo.size.x / screenInfo.size.y;
	Vec3 scaleToNDC = {
		2.0f / (CAMERA_HEIGHT_UNITS * aspectRatio),
		2.0f / CAMERA_HEIGHT_UNITS,
		1.0f
	};

	return Scale(scaleToNDC);
}

internal Mat4 CalculateInverseViewMatrix(Vec2 cameraPos, Quat cameraRot)
{
	return Translate(ToVec3(cameraPos, 0.0f))
		* UnitQuatToMat4(cameraRot)
		* Translate(-CAMERA_OFFSET_VEC3);
}

internal Mat4 CalculateViewMatrix(Vec2 cameraPos, Quat cameraRot)
{
	return Translate(CAMERA_OFFSET_VEC3)
		* UnitQuatToMat4(Inverse(cameraRot))
		* Translate(ToVec3(-cameraPos, 0.0f));
}

internal Vec2Int WorldToScreen(Vec2 worldPos, ScreenInfo screenInfo,
	Vec2 cameraPos, Quat cameraRot, float32 editorWorldScale)
{
	Vec2Int screenCenter = screenInfo.size / 2;
	Vec4 afterView = Scale(editorWorldScale)
		* CalculateViewMatrix(cameraPos, cameraRot)
		* ToVec4(worldPos, 0.0f, 1.0f);
	Vec2 result = Vec2 { afterView.x, afterView.y } / CAMERA_HEIGHT_UNITS * (float32)screenInfo.size.y;
	return ToVec2Int(result) + screenCenter;
}

internal Vec2 ScreenToWorld(Vec2Int screenPos, ScreenInfo screenInfo,
	Vec2 cameraPos, Quat cameraRot, float32 editorWorldScale)
{
	Vec2Int screenCenter = screenInfo.size / 2;
	Vec2 beforeView = ToVec2(screenPos - screenCenter) / (float32)screenInfo.size.y
		* CAMERA_HEIGHT_UNITS;
	Vec4 result = CalculateInverseViewMatrix(cameraPos, cameraRot)
		* Scale(1.0f / editorWorldScale)
		* ToVec4(beforeView, 0.0f, 1.0f);
	return Vec2 { result.x, result.y };
}

internal bool32 LoadFloorVertices(const ThreadContext* thread,
    FloorCollider* floorCollider, const char* filePath,
    DEBUGPlatformReadFileFunc DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc DEBUGPlatformFreeFileMemory)
{
    DEBUGReadFileResult levelFile = DEBUGPlatformReadFile(thread, filePath);
    if (!levelFile.data) {
        DEBUG_PRINT("Failed to load level file %s\n", filePath);
        return false;
    }

    floorCollider->line.size = 0;
    const char* element = (const char*)levelFile.data;
    int length = (int)levelFile.size;
    while (true) {
        int elementLength;
        const char* next;
        if (!ReadElementInSplitString(element, length, '\n', &elementLength, &next)) {
            break;
        }

        const char* trimmed;
        int trimmedLength;
        TrimWhitespace(element, elementLength, &trimmed, &trimmedLength);
        if (trimmedLength == 0) {
            break;
        }

        Vec2 pos;
        int parsedElements;
        if (!StringToElementArray(trimmed, trimmedLength, ',', true,
            StringToFloat32, 2, pos.e, &parsedElements)) {
            DEBUG_PRINT("Failed to parse floor position %.*s (%s)\n",
                trimmedLength, trimmed, filePath);
            return false;
        }
        if (parsedElements != 2) {
            DEBUG_PRINT("Not enough coordinates in floor position %.*s (%s)\n",
                trimmedLength, trimmed, filePath);
            return false;
        }

        floorCollider->line.Append(pos);

        length -= (int)(next - element);
        element = next;
    }

    DEBUGPlatformFreeFileMemory(thread, &levelFile);

    floorCollider->PrecomputeSampleVerticesFromLine();

    return true;
}

internal void UpdateTown(GameState* gameState, float32 deltaTime, const GameInput* input)
{
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

	const float32 PLAYER_WALK_SPEED = 3.5f;
	const float32 PLAYER_JUMP_HOLD_DURATION_MIN = 0.02f;
	const float32 PLAYER_JUMP_HOLD_DURATION_MAX = 0.3f;
	const float32 PLAYER_JUMP_MAG_MAX = 1.5f;
	const float32 PLAYER_JUMP_MAG_MIN = 0.5f;

	gameState->playerVel.x = 0.0f;
	bool skipPlayerInput = false;
#if GAME_INTERNAL
	if (gameState->editor) {
		skipPlayerInput = true;
	}
#endif

	if (!skipPlayerInput) {
		float32 speed = PLAYER_WALK_SPEED;
		if (gameState->playerState == PLAYER_STATE_JUMPING) {
			speed /= Lerp(gameState->playerJumpMag, 1.0f, 0.3f);
		}

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

		bool fallPressed = IsKeyPressed(input, KM_KEY_S)
			|| IsKeyPressed(input, KM_KEY_ARROW_DOWN)
			|| (input->controllers[0].isConnected && input->controllers[0].leftEnd.y < 0.0f);
		if (gameState->playerState == PLAYER_STATE_GROUNDED && fallPressed
		&& gameState->currentPlatform != nullptr) {
			gameState->playerState = PLAYER_STATE_FALLING;
			gameState->currentPlatform = nullptr;
			gameState->playerCoords.y -= LINE_COLLIDER_MARGIN;
		}

		bool jumpPressed = IsKeyPressed(input, KM_KEY_SPACE)
			|| IsKeyPressed(input, KM_KEY_ARROW_UP)
			|| (input->controllers[0].isConnected && input->controllers[0].a.isDown);
		if (gameState->playerState == PLAYER_STATE_GROUNDED && jumpPressed
		&& !KeyCompare(gameState->kid.activeAnimation, ANIM_FALL) /* TODO fall anim + grounded state seems sketchy */) {
			gameState->playerState = PLAYER_STATE_JUMPING;
			gameState->currentPlatform = nullptr;
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

	float32 animDeltaTime = deltaTime;
	if (gameState->playerState == PLAYER_STATE_JUMPING) {
		animDeltaTime /= Lerp(gameState->playerJumpMag, 1.0f, 0.5f);
	}
	const HashKey* nextAnimations = nextAnims;
	Vec2 rootMotion = gameState->kid.Update(animDeltaTime, numNextAnims, nextAnimations);
	if (gameState->playerState == PLAYER_STATE_JUMPING) {
		rootMotion *= gameState->playerJumpMag;
	}

	if (gameState->playerState == PLAYER_STATE_JUMPING
	&& KeyCompare(gameState->kid.activeAnimation, ANIM_FALL)) {
		gameState->playerState = PLAYER_STATE_FALLING;
	}

	Vec2 deltaCoords = gameState->playerVel * deltaTime + rootMotion;

	Vec2 playerPos = gameState->floor.GetWorldPosFromCoords(gameState->playerCoords);
	Vec2 playerPosNew = gameState->floor.GetWorldPosFromCoords(
		gameState->playerCoords + deltaCoords);
	Vec2 deltaPos = playerPosNew - playerPos;

	LineColliderIntersect intersects[LINE_COLLIDERS_MAX];
	int numIntersects;
	GetLineColliderIntersections(gameState->lineColliders, gameState->numLineColliders,
		playerPos, deltaPos, LINE_COLLIDER_MARGIN,
		intersects, &numIntersects);
	for (int i = 0; i < numIntersects; i++) {
		if (gameState->currentPlatform == intersects[i].collider) {
			continue;
		}

		float32 tX = (intersects[i].pos.x - playerPos.x) / deltaPos.x;
		float32 newDeltaCoordX = deltaCoords.x * tX;
		Vec2 newFloorPos, newFloorNormal;
		gameState->floor.GetInfoFromCoordX(gameState->playerCoords.x + newDeltaCoordX,
			&newFloorPos, &newFloorNormal);

		const float32 COS_WALK_ANGLE = cosf(PI_F / 4.0f);
		float32 dotCollisionFloorNormal = Dot(newFloorNormal, intersects[i].normal);
		DEBUG_PRINT("dot %.3f\n", dotCollisionFloorNormal);
		DEBUG_PRINT("normals: floor %.3f, %.3f ; collision %.3f, %.3f\n",
			newFloorNormal.x, newFloorNormal.y,
			intersects[i].normal.x, intersects[i].normal.y);
		if (AbsFloat32(dotCollisionFloorNormal) >= COS_WALK_ANGLE) {
			// Walkable floor
			if (gameState->playerState == PLAYER_STATE_FALLING) {
				gameState->currentPlatform = intersects[i].collider;
				deltaCoords.x = newDeltaCoordX;
			}
		}
		else {
			// Floor at steep angle (wall)
			deltaCoords = Vec2::zero;
		}
	}

	float32 floorHeightCoord = 0.0f;
	if (gameState->currentPlatform != nullptr) {
		float32 platformHeight;
		bool32 playerOverPlatform = GetLineColliderCoordYFromFloorCoordX(
			*gameState->currentPlatform,
			gameState->floor, gameState->playerCoords.x + deltaCoords.x,
			&platformHeight);
		if (playerOverPlatform) {
			floorHeightCoord = platformHeight;
		}
		else {
			gameState->currentPlatform = nullptr;
			gameState->playerState = PLAYER_STATE_FALLING;
		}
	}

	Vec2 playerCoordsNew = gameState->playerCoords + deltaCoords;
	if (playerCoordsNew.y < floorHeightCoord || gameState->currentPlatform != nullptr) {
		playerCoordsNew.y = floorHeightCoord;
		gameState->prevFloorCoordY = floorHeightCoord;
		gameState->playerState = PLAYER_STATE_GROUNDED;
	}

	bool32 pushPullKeyPressed = IsKeyPressed(input, KM_KEY_SHIFT)
		|| (input->controllers[0].isConnected && input->controllers[0].b.isDown);

	{ // barrel
		const float32 BARREL_INTERACT_DIST_MIN = PLAYER_RADIUS * 2.5f;
		const float32 BARREL_INTERACT_DIST_MAX = PLAYER_RADIUS * 3.0f;
		Vec2 toBarrelCoords = gameState->barrelCoords - gameState->playerCoords;
		float32 distToBarrel = Mag(toBarrelCoords);
		if (pushPullKeyPressed
		&& BARREL_INTERACT_DIST_MIN <= distToBarrel
		&& distToBarrel <= BARREL_INTERACT_DIST_MAX) {
			gameState->barrelCoords.x += playerCoordsNew.x - gameState->playerCoords.x;
		}

		int numBarrelNextAnims = 0;
		HashKey barrelNextAnims[1];
		if (WasKeyPressed(input, KM_KEY_X)) {
			numBarrelNextAnims = 1;
			barrelNextAnims[0].WriteString("Explode");
		}
		gameState->barrel.Update(deltaTime, numBarrelNextAnims, barrelNextAnims);
	}

	{ // rock
		Vec2 size = ToVec2(gameState->rockTexture.size) / REF_PIXELS_PER_UNIT;
		float32 radius = size.y / 2.0f * 0.8f;
		gameState->rock.coords.y = radius;

		const float32 INTERACT_DIST_MIN = radius * 1.0f;
		const float32 INTERACT_DIST_MAX = radius * 2.0f;
		Vec2 toCoords = gameState->rock.coords - gameState->playerCoords;
		float32 dist = Mag(toCoords);
		if (pushPullKeyPressed && INTERACT_DIST_MIN <= dist && dist <= INTERACT_DIST_MAX) {
			float32 deltaX = playerCoordsNew.x - gameState->playerCoords.x;
			gameState->rock.coords.x += deltaX;
			gameState->rock.angle += -deltaX / radius;
		}

		Vec2 floorPos, floorNormal;
		gameState->floor.GetInfoFromCoordX(gameState->rock.coords.x, &floorPos, &floorNormal);
		Vec2 floorTangent = { floorNormal.y, -floorNormal.x };
		LineCollider* rockPlatform = &gameState->lineColliders[gameState->numLineColliders - 1];
		Vec2 pos = floorPos + gameState->rock.coords.y * floorNormal;
		float32 platformWidth = radius * 0.5f;
		rockPlatform->line.array[0] = pos + radius * floorNormal
			- floorTangent * platformWidth;
		rockPlatform->line.array[1] = pos + radius * floorNormal
			+ floorTangent * platformWidth;
	}

	{ // rock launcher
		const float32 INTERACT_DIST_MIN = PLAYER_RADIUS * 4.0f;
		const float32 INTERACT_DIST_MAX = PLAYER_RADIUS * 4.5f;
		Vec2 toCoords = gameState->rockLauncher.coords - gameState->playerCoords;
		float32 dist = Mag(toCoords);
		if (pushPullKeyPressed && INTERACT_DIST_MIN <= dist && dist <= INTERACT_DIST_MAX) {
			float32 deltaX = playerCoordsNew.x - gameState->playerCoords.x;
			gameState->rockLauncher.coords.x += deltaX;
		}
	}

	gameState->playerCoords = playerCoordsNew;

	gameState->paper.Update(deltaTime, 0, nullptr);

#if GAME_INTERNAL
	if (gameState->editor) {
		return;
	}
#endif

	const float32 CAMERA_FOLLOW_ACCEL_DIST_MIN = 3.0f;
	const float32 CAMERA_FOLLOW_ACCEL_DIST_MAX = 10.0f;
	float32 cameraFollowLerpMag = 0.08f;
	Vec2 cameraCoordsTarget = gameState->playerCoords;
	if (cameraCoordsTarget.y > gameState->prevFloorCoordY) {
		cameraCoordsTarget.y = gameState->prevFloorCoordY;
	}
	Vec2 distVector = cameraCoordsTarget - gameState->cameraCoords;
	float32 dist = Mag(distVector);
	if (dist > CAMERA_FOLLOW_ACCEL_DIST_MIN) {
		float32 lerpMagAccelT = (dist - CAMERA_FOLLOW_ACCEL_DIST_MIN)
			/ (CAMERA_FOLLOW_ACCEL_DIST_MAX - CAMERA_FOLLOW_ACCEL_DIST_MIN);
		lerpMagAccelT = ClampFloat32(lerpMagAccelT, 0.0f, 1.0f);
		cameraFollowLerpMag += (1.0f - cameraFollowLerpMag) * lerpMagAccelT;
	}
	gameState->cameraCoords = Lerp(gameState->cameraCoords, cameraCoordsTarget,
		cameraFollowLerpMag);

	gameState->cameraPos = gameState->floor.GetWorldPosFromCoords(gameState->cameraCoords);
	Vec2 camFloorPos, camFloorNormal;
	gameState->floor.GetInfoFromCoordX(gameState->cameraCoords.x, &camFloorPos, &camFloorNormal);
	gameState->cameraRot = QuatRotBetweenVectors(Vec3::unitY, ToVec3(camFloorNormal, 0.0f));
}

internal void DrawTown(GameState* gameState, SpriteDataGL* spriteDataGL,
	Mat4 projection, ScreenInfo screenInfo)
{
	spriteDataGL->numSprites = 0;

	{ // background
		Vec2 size = gameState->background.scale
			* ToVec2(gameState->background.texture.size) / REF_PIXELS_PER_UNIT;
		Mat4 transform = CalculateTransform(gameState->background.pos, size,
			gameState->background.anchor, Quat::one);
		PushSprite(spriteDataGL, transform, 1.0f, false, gameState->background.texture.textureID);
	}

	{ // rock
		Vec2 pos = gameState->floor.GetWorldPosFromCoords(gameState->rock.coords);
		Vec2 size = ToVec2(gameState->rockTexture.size) / REF_PIXELS_PER_UNIT;
		Quat rot = QuatFromAngleUnitAxis(gameState->rock.angle, Vec3::unitZ);
		Mat4 transform = CalculateTransform(pos, size, gameState->rock.anchor, rot);
		PushSprite(spriteDataGL, transform, 1.0f, false, gameState->rockTexture.textureID);
	}

	{ // rock launcher
		Vec2 pos = gameState->floor.GetWorldPosFromCoords(gameState->rockLauncher.coords);
		Vec2 size = ToVec2(gameState->rockLauncherTexture.size) / REF_PIXELS_PER_UNIT * 0.8f;
		Vec2 floorPos, floorNormal;
		gameState->floor.GetInfoFromCoordX(gameState->rockLauncher.coords.x,
			&floorPos, &floorNormal);
		Quat rot = QuatRotBetweenVectors(Vec3::unitY, ToVec3(floorNormal, 0.0f));
		Mat4 transform = CalculateTransform(pos, size, gameState->rockLauncher.anchor, rot);
		PushSprite(spriteDataGL, transform, 1.0f, false, gameState->rockLauncherTexture.textureID);
	}

	{ // barrel
		Vec2 pos = gameState->floor.GetWorldPosFromCoords(gameState->barrelCoords);
		Vec2 size = ToVec2(gameState->barrel.animatedSprite->textureSize) / REF_PIXELS_PER_UNIT;
		Vec2 barrelFloorPos, barrelFloorNormal;
		gameState->floor.GetInfoFromCoordX(gameState->barrelCoords.x,
			&barrelFloorPos, &barrelFloorNormal);
		Quat barrelRot = QuatRotBetweenVectors(Vec3::unitY, ToVec3(barrelFloorNormal, 0.0f));
		gameState->barrel.Draw(spriteDataGL, pos, size, Vec2 { 0.5f, 0.25f }, barrelRot,
			1.0f, false);
	}

	{ // kid
		Vec2 pos = gameState->floor.GetWorldPosFromCoords(gameState->playerCoords);
		Vec2 anchorUnused = Vec2::zero;
		Vec2 size = ToVec2(gameState->kid.animatedSprite->textureSize) / REF_PIXELS_PER_UNIT;
		gameState->kid.Draw(spriteDataGL, pos, size, anchorUnused, gameState->cameraRot,
			1.0f, !gameState->facingRight);
	}

	Mat4 view = CalculateViewMatrix(gameState->cameraPos, gameState->cameraRot);
	DrawSprites(gameState->renderState, *spriteDataGL, projection * view);

#if GAME_INTERNAL
	if (gameState->editor) {
		return;
	}
#endif

	spriteDataGL->numSprites = 0;

	const float32 ASPECT_RATIO = (float32)screenInfo.size.x / screenInfo.size.y;
	Vec2 screenSizeWorld = { CAMERA_HEIGHT_UNITS * ASPECT_RATIO, CAMERA_HEIGHT_UNITS };
	gameState->paper.Draw(spriteDataGL,
		Vec2::zero, screenSizeWorld, Vec2::one / 2.0f, Quat::one, 0.5f,
		false);

	{ // frame
		// TODO this sounds like I need another batch transform matrix
		float32 scale = (float32)REF_PIXEL_SCREEN_HEIGHT / gameState->frame.size.y;
		Vec2 size = scale * ToVec2(gameState->frame.size) / REF_PIXELS_PER_UNIT;
		Mat4 transform = CalculateTransform(Vec2::zero, size, Vec2::one / 2.0f, Quat::one);
		PushSprite(spriteDataGL, transform, 1.0f, false, gameState->frame.textureID);
	}

	DrawSprites(gameState->renderState, *spriteDataGL, projection);
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
		gameState->playerCoords = Vec2 { 72.0f, 0.0f };
		gameState->playerVel = Vec2::zero;
		gameState->cameraCoords = gameState->playerCoords;
		gameState->playerState = PLAYER_STATE_FALLING;
		gameState->facingRight = true;
		gameState->currentPlatform = nullptr;

		gameState->barrelCoords = gameState->playerCoords - Vec2::unitX * 7.0f;

        if (!LoadFloorVertices(thread, &gameState->floor,
            "data/levels/level0.kml",
            platformFuncs->DEBUGPlatformReadFile,
            platformFuncs->DEBUGPlatformFreeFileMemory)) {
            DEBUG_PANIC("Failed to load level 0");
        }

		gameState->numLineColliders = 0;
		LineCollider* lineCollider;

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 10.51f, 46.69f });
		lineCollider->line.Append(Vec2 { 11.24f, 46.73f });
		lineCollider->line.Append(Vec2 { 11.25f, 48.03f });
		lineCollider->line.Append(Vec2 { 12.68f, 48.07f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 8.54f, 47.79f });
		lineCollider->line.Append(Vec2 { 8.98f, 48.09f });
		lineCollider->line.Append(Vec2 { 9.58f, 48.09f });
		lineCollider->line.Append(Vec2 { 9.73f, 47.70f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 6.33f, 50.50f });
		lineCollider->line.Append(Vec2 { 6.79f, 50.51f });
		lineCollider->line.Append(Vec2 { 6.93f, 50.73f });
		lineCollider->line.Append(Vec2 { 7.58f, 50.69f });
		lineCollider->line.Append(Vec2 { 7.94f, 49.96f });
		lineCollider->line.Append(Vec2 { 8.33f, 49.69f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { -1.89f, 52.59f });
		lineCollider->line.Append(Vec2 { -1.39f, 52.64f });
		lineCollider->line.Append(Vec2 { -0.56f, 52.86f });
		lineCollider->line.Append(Vec2 {  0.19f, 52.86f });
		lineCollider->line.Append(Vec2 {  0.79f, 52.51f });
		lineCollider->line.Append(Vec2 {  1.25f, 52.43f });
		lineCollider->line.Append(Vec2 {  2.95f, 51.57f });
		lineCollider->line.Append(Vec2 {  4.18f, 51.59f });
		lineCollider->line.Append(Vec2 {  5.16f, 51.54f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 27.44f, 28.73f });
		lineCollider->line.Append(Vec2 { 27.44f, 32.64f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 30.44f, 28.73f });
		lineCollider->line.Append(Vec2 { 30.44f, 32.64f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 33.44f, 28.73f });
		lineCollider->line.Append(Vec2 { 33.44f, 32.64f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 36.44f, 28.73f });
		lineCollider->line.Append(Vec2 { 36.44f, 32.64f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 39.44f, 28.73f });
		lineCollider->line.Append(Vec2 { 39.44f, 32.64f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 42.44f, 28.73f });
		lineCollider->line.Append(Vec2 { 42.44f, 32.64f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 38.40f, 17.41f });
		lineCollider->line.Append(Vec2 { 41.55f, 17.41f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 38.40f, 20.41f });
		lineCollider->line.Append(Vec2 { 41.55f, 20.41f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 38.40f, 23.41f });
		lineCollider->line.Append(Vec2 { 41.55f, 23.41f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 38.40f, 26.41f });
		lineCollider->line.Append(Vec2 { 41.55f, 26.41f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 38.40f, 29.41f });
		lineCollider->line.Append(Vec2 { 41.55f, 29.41f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 38.40f, 32.41f });
		lineCollider->line.Append(Vec2 { 41.55f, 32.41f });

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 38.40f, 35.41f });
		lineCollider->line.Append(Vec2 { 41.55f, 35.41f });

		// reserved for rock
		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->line.Append(Vec2 { 0.0f, 0.0f });
		lineCollider->line.Append(Vec2 { 0.0f, 0.0f });

		DEBUG_ASSERT(gameState->numLineColliders <= LINE_COLLIDERS_MAX);
		for (int i = 0; i < gameState->numLineColliders; i++) {
			DEBUG_ASSERT(gameState->lineColliders[i].line.size <= LINE_COLLIDER_MAX_VERTICES);
		}

		gameState->grainTime = 0.0f;

#if GAME_INTERNAL
		gameState->debugView = false;
		gameState->editor = false;
		gameState->editorScaleExponent = 0.5f;

		/*for (uint32 i = 0; i < gameState->floor.line.size; i++) {
			Vec4 colorIdle = Vec4 { 0.0f, 0.0f, 1.0f, 1.0f };
			Vec4 colorHover = Vec4 { 0.75f, 0.0f, 1.0f, 1.0f };
			Vec4 colorPress = Vec4 { 1.0f, 0.0f, 1.0f, 1.0f };
			InitClickableBox(&gameState->floorVertexBoxes[i],
				colorIdle, colorHover, colorPress);
		}
        gameState->boxSelected = nullptr;*/
        gameState->floorVertexSelected = -1;
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
		if (!LoadPNGOpenGL(thread,
		"data/sprites/pixel.png",
		GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->background.texture, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load background");
		}

		gameState->rock.coords = { 90.0f, 0.0f };
		gameState->rock.anchor = { 0.5f, 0.5f };
		gameState->rock.angle = 0.0f;
		if (!LoadPNGOpenGL(thread,
		"data/sprites/rock.png",
		GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->rockTexture, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load rock");
		}

		gameState->rockLauncher.coords = { 85.0f, 0.0f };
		gameState->rockLauncher.anchor = { 0.5f, 0.15f };
		if (!LoadPNGOpenGL(thread,
		"data/sprites/machine2.png",
		GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->rockLauncherTexture, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load rock launcher");
		}

		if (!LoadAnimatedSprite(thread,
		"data/animations/kid/kid.kma",
		gameState->spriteKid, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load kid animation sprite");
		}

		if (!LoadAnimatedSprite(thread,
		"data/animations/barrel/barrel.kma",
		gameState->spriteBarrel, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load barrel animation sprite");
		}

		/*bool32 loadCrystalAnim = LoadAnimatedSprite(thread,
			"data/animations/crystal/crystal.kma",
			gameState->spriteCrystal, memory->transient,
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

		if (!LoadAnimatedSprite(thread,
		"data/animations/paper/paper.kma",
		gameState->spritePaper, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load paper animation sprite");
		}

		gameState->paper.animatedSprite = &gameState->spritePaper;
		gameState->paper.activeAnimation = gameState->spritePaper.startAnimation;
		gameState->paper.activeFrame = 0;
		gameState->paper.activeFrameRepeat = 0;
		gameState->paper.activeFrameTime = 0.0f;

		if (!LoadPNGOpenGL(thread,
		"data/sprites/frame.png",
		GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->frame, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load frame");
		}

		if (!LoadPNGOpenGL(thread,
		"data/luts/lutbase.png",
		GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->lutBase, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
			DEBUG_PANIC("Failed to load base LUT");
		}

		if (!LoadPNGOpenGL(thread,
		"data/luts/kodak5205.png",
		GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
		gameState->lut1, memory->transient,
		platformFuncs->DEBUGPlatformReadFile,
		platformFuncs->DEBUGPlatformFreeFileMemory)) {
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

    for (int i = 0; i < 10; i++) {
        if (WasKeyPressed(input, (KeyInputCode)(KM_KEY_0 + i))) {
            char levelFilePath[32];
            snprintf(levelFilePath, 32, "data/levels/level%d.kml", i);
            if (!LoadFloorVertices(thread, &gameState->floor, levelFilePath,
                platformFuncs->DEBUGPlatformReadFile,
                platformFuncs->DEBUGPlatformFreeFileMemory)) {
                DEBUG_PANIC("Failed to load level %d", i);
            }
            gameState->playerCoords = { 0.0f, 0.0f };
        }
    }

	UpdateTown(gameState, deltaTime, input);

	// ---------------------------- Begin Rendering ---------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColorDepth[0].framebuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Mat4 projection = CalculateProjectionMatrix(screenInfo);
#if GAME_INTERNAL
	if (gameState->editor) {
		projection = projection * Scale(ScaleExponentToWorldScale(gameState->editorScaleExponent));
	}
#endif

	DEBUG_ASSERT(memory->transient.size >= sizeof(SpriteDataGL));
	SpriteDataGL* spriteDataGL = (SpriteDataGL*)memory->transient.memory;

	DrawTown(gameState, spriteDataGL, projection, screenInfo);

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

	#if 0
	// Render pillarbox
	int pillarboxWidth = GetPillarboxWidth(screenInfo);
	const Vec4 PILLARBOX_COLOR = { 0.965f, 0.957f, 0.91f, 1.0f };
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
	#endif

	// DrawRect(gameState->rectGL, screenInfo,
	// 	input->mousePos, Vec2 { 0.5f, 0.5f }, Vec2Int { 5, 5 },
	// 	Vec4 { 0.1f, 0.1f, 0.1f, 1.0f });

	// ---------------------------- End Rendering -----------------------------
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	OutputAudio(audio, gameState, input, memory->transient);

#if GAME_INTERNAL
	int pillarboxWidth = GetPillarboxWidth(screenInfo);

	bool32 wasDebugKeyPressed = WasKeyPressed(input, KM_KEY_G)
		|| (input->controllers[0].isConnected
		&& input->controllers[0].x.isDown && input->controllers[0].x.transitions == 1);
	if (wasDebugKeyPressed) {
		gameState->debugView = !gameState->debugView;
	}
	if (WasKeyPressed(input, KM_KEY_H)) {
		gameState->editor = !gameState->editor;
		gameState->editorScaleExponent = 0.5f;
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

		float32 fps = 1.0f / deltaTime;
		sprintf(textStr, "%.2f --- FPS", fps);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		textPosRight.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f -- CRDS", gameState->playerCoords.x, gameState->playerCoords.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		Vec2 playerPosWorld = gameState->floor.GetWorldPosFromCoords(gameState->playerCoords);
		sprintf(textStr, "%.2f|%.2f --- POS", playerPosWorld.x, playerPosWorld.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		sprintf(textStr, "%d - STATE", gameState->playerState);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		HashKey& kidActiveAnim = gameState->kid.activeAnimation;
		sprintf(textStr, "%.*s -- ANIM", kidActiveAnim.length, kidActiveAnim.string);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		textPosRight.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f --- CAM", gameState->cameraPos.x, gameState->cameraPos.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPosRight.y -= textFont.height;
		textPosRight.y -= textFont.height;
		Vec2 mouseWorld = ScreenToWorld(input->mousePos, screenInfo,
			gameState->cameraPos, gameState->cameraRot,
			ScaleExponentToWorldScale(gameState->editorScaleExponent));
		sprintf(textStr, "%.2f|%.2f - MOUSE", mouseWorld.x, mouseWorld.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPosRight, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		if (input->mouseButtons[1].isDown && input->mouseButtons[1].transitions == 1) {
			DEBUG_PRINT("Mouse: %.2f, %.2f\n", mouseWorld.x, mouseWorld.y);
		}

		DEBUG_ASSERT(memory->transient.size >= sizeof(LineGLData));
		LineGLData* lineData = (LineGLData*)memory->transient.memory;

		Mat4 view = CalculateViewMatrix(gameState->cameraPos, gameState->cameraRot);

		{ // line colliders
			Vec4 lineColliderColor = { 0.0f, 0.6f, 0.6f, 1.0f };
			for (int i = 0; i < gameState->numLineColliders; i++) {
				const LineCollider& lineCollider = gameState->lineColliders[i];
				lineData->count = lineCollider.line.size;
				for (uint32 v = 0; v < lineCollider.line.size; v++) {
					lineData->pos[v] = ToVec3(lineCollider.line.array[v], 0.0f);
				}
				DrawLine(gameState->lineGL, projection, view, lineData, lineColliderColor);
			}
		}

		{ // floor
			Vec4 floorSmoothColorMax = { 0.4f, 0.4f, 0.5f, 1.0f };
			Vec4 floorSmoothColorMin = { 0.4f, 0.4f, 0.5f, 0.2f };
			const float32 FLOOR_SMOOTH_STEPS = 0.2f;
			const int FLOOR_HEIGHT_NUM_STEPS = 10;
			const float32 FLOOR_HEIGHT_STEP = 0.5f;
			const float32 FLOOR_NORMAL_LENGTH = FLOOR_HEIGHT_STEP;
			const float32 FLOOR_LENGTH = gameState->floor.length;
			for (int i = 0; i < FLOOR_HEIGHT_NUM_STEPS; i++) {
				float32 height = i * FLOOR_HEIGHT_STEP;
				lineData->count = 0;
				for (float32 floorX = 0.0f; floorX < FLOOR_LENGTH; floorX += FLOOR_SMOOTH_STEPS) {
					Vec2 fPos, fNormal;
					gameState->floor.GetInfoFromCoordX(floorX, &fPos, &fNormal);
					Vec2 pos = fPos + fNormal * height;
					lineData->pos[lineData->count++] = ToVec3(pos, 0.0f);
					lineData->pos[lineData->count++] = ToVec3(
						pos + fNormal * FLOOR_NORMAL_LENGTH, 0.0f);
					lineData->pos[lineData->count++] = ToVec3(pos, 0.0f);
				}
				DrawLine(gameState->lineGL, projection, view, lineData,
					Lerp(floorSmoothColorMax, floorSmoothColorMin,
						(float32)i / (FLOOR_HEIGHT_NUM_STEPS - 1)));
			}
		}

		{ // player
			const float32 CROSS_RADIUS = 0.2f;
			Vec4 playerColor = { 1.0f, 0.2f, 0.2f, 1.0f };
			Vec3 playerPosWorld3 = ToVec3(playerPosWorld, 0.0f);
			lineData->count = 2;
			lineData->pos[0] = playerPosWorld3 - Vec3::unitX * CROSS_RADIUS;
			lineData->pos[1] = playerPosWorld3 + Vec3::unitX * CROSS_RADIUS;
			DrawLine(gameState->lineGL, projection, view, lineData, playerColor);
			lineData->pos[0] = playerPosWorld3 - Vec3::unitY * CROSS_RADIUS;
			lineData->pos[1] = playerPosWorld3 + Vec3::unitY * CROSS_RADIUS;
			DrawLine(gameState->lineGL, projection, view, lineData, playerColor);
		}
	}
	if (gameState->editor) {
		Vec2Int editorStrPos = {
			pillarboxWidth + MARGIN.x,
			MARGIN.y,
		};
		Vec4 editorFontColor = { 1.0f, 0.1f, 1.0f, 1.0f };
		DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
			"EDITOR", editorStrPos, Vec2 { 0.0f, 0.0f }, editorFontColor, memory->transient);

        bool32 newVertexPressed = false;
        {
            const Vec4 BOX_COLOR_BASE = Vec4 { 0.1f, 0.5f, 1.0f, 1.0f };
            const float32 IDLE_ALPHA = 0.5f;
            const float32 HOVER_ALPHA = 0.8f;
            const float32 SELECTED_ALPHA = 1.0f;
            const Vec2Int BOX_SIZE = Vec2Int { 20, 20 };
            const Vec2 BOX_ANCHOR = Vec2::one / 2.0f;
            const Vec2Int ANCHOR_OFFSET = Vec2Int {
                (int)(BOX_SIZE.x * BOX_ANCHOR.x),
                (int)(BOX_SIZE.y * BOX_ANCHOR.y)
            };
            Vec2Int mousePosPlusAnchor = input->mousePos + ANCHOR_OFFSET;
            for (uint32 i = 0; i < gameState->floor.line.size; i++) {
                Vec2Int boxPos = WorldToScreen(gameState->floor.line.array[i], screenInfo,
                    gameState->cameraPos, gameState->cameraRot,
                    ScaleExponentToWorldScale(gameState->editorScaleExponent));

                Vec4 boxColor = BOX_COLOR_BASE;
                boxColor.a = IDLE_ALPHA;
                if ((mousePosPlusAnchor.x >= boxPos.x
                && mousePosPlusAnchor.x <= boxPos.x + BOX_SIZE.x) &&
                (mousePosPlusAnchor.y >= boxPos.y
                && mousePosPlusAnchor.y <= boxPos.y + BOX_SIZE.y)) {
                    if (input->mouseButtons[0].isDown
                    && input->mouseButtons[0].transitions == 1) {
                        newVertexPressed = true;
                        gameState->floorVertexSelected = (int)i;
                    }
                    else {
                        boxColor.a = HOVER_ALPHA;
                    }
                }
                if ((int)i == gameState->floorVertexSelected) {
                    boxColor.a = SELECTED_ALPHA;
                }

                DrawRect(gameState->rectGL, screenInfo,
                    boxPos, BOX_ANCHOR, BOX_SIZE, boxColor);
            }
        }

        if (input->mouseButtons[0].isDown && input->mouseButtons[0].transitions == 1
        && !newVertexPressed) {
            gameState->floorVertexSelected = -1;
        }

        Vec2 mouseWorldPosStart = ScreenToWorld(input->mousePos, screenInfo,
            gameState->cameraPos, gameState->cameraRot,
            ScaleExponentToWorldScale(gameState->editorScaleExponent));
        Vec2 mouseWorldPosEnd = ScreenToWorld(input->mousePos + input->mouseDelta, screenInfo,
            gameState->cameraPos, gameState->cameraRot,
            ScaleExponentToWorldScale(gameState->editorScaleExponent));
        Vec2 mouseWorldDelta = mouseWorldPosEnd - mouseWorldPosStart;

        if (input->mouseButtons[0].isDown) {
            if (gameState->floorVertexSelected == -1) {
                gameState->cameraPos -= mouseWorldDelta;
            }
            else {
                gameState->floor.line.array[gameState->floorVertexSelected] += mouseWorldDelta;
                gameState->floor.PrecomputeSampleVerticesFromLine();
            }
        }

        if (gameState->floorVertexSelected != -1) {
            if (WasKeyPressed(input, KM_KEY_R)) {
                gameState->floor.line.Remove(gameState->floorVertexSelected);
                gameState->floor.PrecomputeSampleVerticesFromLine();
                gameState->floorVertexSelected = -1;
            }
        }

        if (input->mouseButtons[1].isDown && input->mouseButtons[1].transitions == 1) {
            if (gameState->floorVertexSelected == -1) {
                gameState->floor.line.Append(mouseWorldPosEnd);
                gameState->floorVertexSelected = (int)(gameState->floor.line.size - 1);
            }
            else {
                gameState->floor.line.AppendAfter(mouseWorldPosEnd,
                    gameState->floorVertexSelected);
                gameState->floorVertexSelected += 1;
            }
            gameState->floor.PrecomputeSampleVerticesFromLine();
        }

        if (WasKeyPressed(input, KM_KEY_P)) {
            uint64 stringSize = 0;
            uint64 stringCapacity = memory->transient.size;
            char* string = (char*)memory->transient.memory;
            for (uint32 i = 0; i < gameState->floor.line.size; i++) {
                uint64 n = snprintf(string + stringSize, stringCapacity - stringSize,
                    "{ %.2ff, %.2ff },\r\n",
                    gameState->floor.line.array[i].x, gameState->floor.line.array[i].y);
                stringSize += n;
                DEBUG_ASSERT(stringSize < stringCapacity);
            }

            char fileName[32];
            snprintf(fileName, 32, "floor_vertices%d.txt", RandInt(0, 1000));
            platformFuncs->DEBUGPlatformWriteFile(thread, fileName, (uint32)stringSize, string);
            DEBUG_PRINT("Floor vertices written to file\n");
        }

		float32 editorScaleExponentDelta = input->mouseWheelDelta * 0.0002f;
		gameState->editorScaleExponent = ClampFloat32(
			gameState->editorScaleExponent + editorScaleExponentDelta, 0.0f, 1.0f);
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
#include "collision.cpp"
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