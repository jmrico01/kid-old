#include "main.h"

#include <km_common/km_debug.h>
#include <km_common/km_defines.h>
#include <km_common/km_input.h>
#include <km_common/km_log.h>
#include <km_common/km_math.h>
#include <km_common/km_memory.h>
#include <km_common/km_os.h>
#include <km_common/km_string.h>
#include <km_platform/main_platform.h>
#undef internal
#include <random>
#define internal static
#include <stb_image.h>
#include <stb_sprintf.h>

#include "imgui.h"
#include "opengl.h"
#include "opengl_funcs.h"
#include "opengl_base.h"
#include "post.h"
#include "render.h"

#define CAMERA_OFFSET_VEC3(screenHeightUnits, cameraOffsetFracY) \
(Vec3 { 0.0f, cameraOffsetFracY * screenHeightUnits, 0.0f })

const float32 PLAYER_RADIUS = 0.4f;
const float32 PLAYER_HEIGHT = 1.3f;

const float32 LINE_COLLIDER_MARGIN = 0.05f;

global_var const char* KID_ANIMATION_IDLE = "idle";
global_var const char* KID_ANIMATION_WALK = "walk";
global_var const char* KID_ANIMATION_JUMP = "jump";
global_var const char* KID_ANIMATION_FALL = "fall";
global_var const char* KID_ANIMATION_LAND = "land";

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

internal float32 ScaleExponentToWorldScale(float32 exponent)
{
	const float32 SCALE_MIN = 0.1f;
	const float32 SCALE_MAX = 10.0f;
	const float32 c = (SCALE_MAX * SCALE_MIN - 1.0f) / (SCALE_MAX + SCALE_MIN - 2.0f);
	const float32 a = SCALE_MIN - c;
	const float32 b = log2f((SCALE_MAX - c) / a);
	return a * powf(2.0f, b * exponent) + c;
}

Vec2Int GetBorderSize(ScreenInfo screenInfo, float32 targetAspectRatio, float32 minBorderFrac)
{
	Vec2Int border = Vec2Int::zero;
	float32 aspectRatio = (float32)screenInfo.size.x / (float32)screenInfo.size.y;
	int minBorderX = (int)(screenInfo.size.x * minBorderFrac);
	int minBorderY = (int)(screenInfo.size.y * minBorderFrac);
	if (aspectRatio < targetAspectRatio) {
		border.x = minBorderX;
		int targetHeight = (int)((screenInfo.size.x - minBorderX * 2) / targetAspectRatio);
		border.y = (screenInfo.size.y - targetHeight) / 2;
		if (border.y < minBorderY) {
			border.y = minBorderY;
		}
	}
	else {
		border.y = minBorderY;
		int targetWidth = (int)((screenInfo.size.y - minBorderY * 2) * targetAspectRatio);
		border.x = (screenInfo.size.x - targetWidth) / 2;
		if (border.x < minBorderX) {
			border.x = minBorderX;
		}
	}
	return border;
}

// Scales world-space coordinates into NDC
Mat4 CalculateProjectionMatrix(ScreenInfo screenInfo,
                               int pixelScreenHeight, float32 pixelsPerUnit)
{
	float32 screenHeightUnits = (float32)pixelScreenHeight / pixelsPerUnit;
	float32 aspectRatio = (float32)screenInfo.size.x / screenInfo.size.y;
	Vec3 scaleToNDC = {
		2.0f / (screenHeightUnits * aspectRatio),
		2.0f / screenHeightUnits,
		1.0f
	};
    
	return Scale(scaleToNDC);
}

Mat4 CalculateInverseViewMatrix(Vec2 cameraPos, Quat cameraRot,
                                int pixelScreenHeight, float32 pixelsPerUnit, float32 cameraOffsetFracY)
{
	float32 screenHeightUnits = (float32)pixelScreenHeight / pixelsPerUnit;
	Vec3 cameraOffset = Vec3 { 0.0f, cameraOffsetFracY * screenHeightUnits, 0.0f };
	return Translate(ToVec3(cameraPos, 0.0f))
		* UnitQuatToMat4(cameraRot)
		* Translate(-cameraOffset);
}

Mat4 CalculateViewMatrix(Vec2 cameraPos, Quat cameraRot,
                         int pixelScreenHeight, float32 pixelsPerUnit, float32 cameraOffsetFracY)
{
	float32 screenHeightUnits = (float32)pixelScreenHeight / pixelsPerUnit;
	Vec3 cameraOffset = Vec3 { 0.0f, cameraOffsetFracY * screenHeightUnits, 0.0f };
	return Translate(cameraOffset)
		* UnitQuatToMat4(Inverse(cameraRot))
		* Translate(ToVec3(-cameraPos, 0.0f));
}

Vec2Int WorldToScreen(Vec2 worldPos, ScreenInfo screenInfo,
                      Vec2 cameraPos, Quat cameraRot, float32 editorWorldScale,
                      int pixelScreenHeight, float32 pixelsPerUnit, float32 cameraOffsetFracY)
{
	float32 screenHeightUnits = (float32)pixelScreenHeight / pixelsPerUnit;
	Vec2Int screenCenter = screenInfo.size / 2;
	Vec4 afterView = Scale(editorWorldScale)
		* CalculateViewMatrix(cameraPos, cameraRot, pixelScreenHeight, pixelsPerUnit, cameraOffsetFracY)
		* ToVec4(worldPos, 0.0f, 1.0f);
	Vec2 result = Vec2 { afterView.x, afterView.y } / screenHeightUnits * (float32)screenInfo.size.y;
	return ToVec2Int(result) + screenCenter;
}

Vec2 ScreenToWorld(Vec2Int screenPos, ScreenInfo screenInfo,
                   Vec2 cameraPos, Quat cameraRot, float32 editorWorldScale,
                   int pixelScreenHeight, float32 pixelsPerUnit, float32 cameraOffsetFracY)
{
	float32 screenHeightUnits = (float32)pixelScreenHeight / pixelsPerUnit;
	Vec2Int screenCenter = screenInfo.size / 2;
	Vec2 beforeView = ToVec2(screenPos - screenCenter) / (float32)screenInfo.size.y
		* screenHeightUnits;
	Vec4 result = CalculateInverseViewMatrix(cameraPos, cameraRot, pixelScreenHeight, pixelsPerUnit, cameraOffsetFracY)
		* Scale(1.0f / editorWorldScale)
		* ToVec4(beforeView, 0.0f, 1.0f);
	return Vec2 { result.x, result.y };
}

internal bool SetActiveLevel(GameState* gameState, LevelId levelId, Vec2 startCoords, MemoryBlock transient)
{
	LevelData* levelData = GetLevelData(&gameState->assets, levelId);
	if (!levelData->loaded) {
		if (!LoadLevelData(levelData, levelId, gameState->refPixelsPerUnit, transient)) {
			LOG_ERROR("Failed to load level data for level %d\n", levelId);
			return false;
		}
	}
    
	gameState->activeLevelId = levelId;
    
	gameState->playerCoords = startCoords;
	gameState->playerVel = Vec2::zero;
	if (startCoords.y > 0.0f) {
		gameState->playerState = PlayerState::FALLING;
	}
	else {
		gameState->playerState = PlayerState::GROUNDED;
		gameState->kid.activeAnimationKey.WriteString(KID_ANIMATION_IDLE);
		gameState->kid.activeFrame = 0;
		gameState->kid.activeFrameRepeat = 0;
		gameState->kid.activeFrameTime = 0.0f;
	}
	if (levelData->lockedCamera) {
		gameState->cameraCoords = levelData->cameraCoords;
	}
	else {
		gameState->cameraCoords = startCoords;
	}
    
	return true;
}

internal Vec2 WrappedWorldOffset(Vec2 fromCoords, Vec2 toCoords, float32 floorLength)
{
	Vec2 offset = toCoords - fromCoords;
	float32 distX = AbsFloat32(offset.x);
    
	float32 distXAlt = AbsFloat32(offset.x + floorLength);
	if (distXAlt < distX) {
		offset.x += floorLength;
		return offset;
	}
    
	distXAlt = AbsFloat32(offset.x - floorLength);
	if (distXAlt < distX) {
		offset.x -= floorLength;
		return offset;
	}
    
	return offset;
}

internal bool IsGrabbableObjectInRange(Vec2 playerCoords, GrabbedObjectInfo object,
                                       float32 floorLength)
{
	DEBUG_ASSERT(object.coordsPtr != nullptr);
    
	Vec2 toCoords = WrappedWorldOffset(playerCoords, *object.coordsPtr, floorLength);
	float32 distX = AbsFloat32(toCoords.x);
	float32 distY = AbsFloat32(toCoords.y);
	return object.rangeX.x <= distX && distX <= object.rangeX.y
		&& object.rangeY.x <= distY && distY <= object.rangeY.y;
}

internal void UpdateWorld(GameState* gameState, float32 deltaTime, const GameInput& input,
                          MemoryBlock transient)
{
    bool isInteractKeyPressed = IsKeyPressed(input, KM_KEY_E)
		|| (input.controllers[0].isConnected && input.controllers[0].b.isDown);
	bool wasInteractKeyPressed = WasKeyPressed(input, KM_KEY_E)
		|| (input.controllers[0].isConnected && input.controllers[0].b.isDown
            && input.controllers[0].b.transitions == 1);
    
	if (wasInteractKeyPressed) {
		const LevelData* levelData = GetLevelData(gameState->assets, gameState->activeLevelId);
		for (uint64 i = 0; i < levelData->levelTransitions.size; i++) {
			Vec2 toPlayer = WrappedWorldOffset(gameState->playerCoords, levelData->levelTransitions[i].coords,
                                               levelData->floor.length);
			if (AbsFloat32(toPlayer.x) <= levelData->levelTransitions[i].range.x
                && AbsFloat32(toPlayer.y) <= levelData->levelTransitions[i].range.y) {
				LevelId newLevelId = (LevelId)levelData->levelTransitions[i].toLevel;
				Vec2 startCoords = levelData->levelTransitions[i].toCoords;
				if (!SetActiveLevel(gameState, newLevelId, startCoords, transient)) {
					DEBUG_PANIC("Failed to load level %llu\n", i);
				}
				break;
			}
		}
	}
    
	const LevelData* levelData = GetLevelData(gameState->assets, gameState->activeLevelId);
    
	HashKey ANIM_IDLE(KID_ANIMATION_IDLE);
	HashKey ANIM_WALK(KID_ANIMATION_WALK);
	HashKey ANIM_JUMP(KID_ANIMATION_JUMP);
	HashKey ANIM_FALL(KID_ANIMATION_FALL);
	HashKey ANIM_LAND(KID_ANIMATION_LAND);
    
	const float32 PLAYER_WALK_SPEED = 3.6f;
	const float32 PLAYER_JUMP_HOLD_DURATION_MIN = 0.02f;
	const float32 PLAYER_JUMP_HOLD_DURATION_MAX = 0.3f;
	const float32 PLAYER_JUMP_MAG_MAX = 1.2f;
	const float32 PLAYER_JUMP_MAG_MIN = 0.4f;
    
	float32 speedMultiplier = 1.0f;
    
	gameState->playerVel.x = 0.0f;
    
	// Skip player input on INTERNAL build if kmKey flag is on
	if (!gameState->kmKey) {
        float32 speed = PLAYER_WALK_SPEED * speedMultiplier;
        if (gameState->playerState == PlayerState::JUMPING) {
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
        if (input.controllers[0].isConnected) {
            float32 leftStickX = input.controllers[0].leftEnd.x;
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
            || (input.controllers[0].isConnected && input.controllers[0].leftEnd.y < 0.0f);
        if (gameState->playerState == PlayerState::GROUNDED && fallPressed
            && gameState->currentPlatform != nullptr) {
            gameState->playerState = PlayerState::FALLING;
            gameState->currentPlatform = nullptr;
            gameState->playerCoords.y -= LINE_COLLIDER_MARGIN;
        }
        
        bool jumpPressed = IsKeyPressed(input, KM_KEY_SPACE)
            || IsKeyPressed(input, KM_KEY_ARROW_UP)
            || (input.controllers[0].isConnected && input.controllers[0].a.isDown);
        if (gameState->playerState == PlayerState::GROUNDED && jumpPressed
            && !KeyCompare(gameState->kid.activeAnimationKey, ANIM_FALL) /* TODO fall anim + grounded state seems sketchy */) {
            gameState->playerState = PlayerState::JUMPING;
            gameState->currentPlatform = nullptr;
            gameState->playerJumpHolding = true;
            gameState->playerJumpHold = 0.0f;
            gameState->playerJumpMag = PLAYER_JUMP_MAG_MAX;
            gameState->audioState.soundJump.playing = true;
            gameState->audioState.soundJump.sampleIndex = 0;
        }
        
        if (gameState->playerJumpHolding) {
            gameState->playerJumpHold += deltaTime;
            if (gameState->playerState == PlayerState::JUMPING && !jumpPressed) {
                gameState->playerJumpHolding = false;
                float32 timeT = gameState->playerJumpHold - PLAYER_JUMP_HOLD_DURATION_MIN
                    / (PLAYER_JUMP_HOLD_DURATION_MAX - PLAYER_JUMP_HOLD_DURATION_MIN);
                timeT = ClampFloat32(timeT, 0.0f, 1.0f);
                timeT = sqrtf(timeT);
                gameState->playerJumpMag = Lerp(PLAYER_JUMP_MAG_MIN, PLAYER_JUMP_MAG_MAX, timeT);
            }
        }
	}
    
	FixedArray<HashKey, 4> nextAnimations;
	nextAnimations.size = 0;
	if (gameState->playerState == PlayerState::JUMPING) {
		if (!KeyCompare(gameState->kid.activeAnimationKey, ANIM_JUMP)) {
			nextAnimations.Append(ANIM_JUMP);
		}
		else {
			nextAnimations.Append(ANIM_FALL);
		}
	}
	else if (gameState->playerState == PlayerState::FALLING) {
		nextAnimations.Append(ANIM_FALL);
	}
	else if (gameState->playerState == PlayerState::GROUNDED) {
		if (gameState->playerVel.x != 0) {
			nextAnimations.Append(ANIM_WALK);
			nextAnimations.Append(ANIM_LAND);
		}
		else {
			nextAnimations.Append(ANIM_IDLE);
			nextAnimations.Append(ANIM_LAND);
		}
	}
    
	float32 animDeltaTime = deltaTime;
	if (gameState->playerState == PlayerState::GROUNDED) {
		animDeltaTime *= speedMultiplier;
	}
	if (gameState->playerState == PlayerState::JUMPING) {
		animDeltaTime /= Lerp(gameState->playerJumpMag, 1.0f, 0.5f);
	}
	Vec2 rootMotion = UpdateAnimatedSprite(&gameState->kid, gameState->assets, animDeltaTime, nextAnimations.ToArray());
	if (gameState->playerState == PlayerState::JUMPING) {
		rootMotion *= gameState->playerJumpMag;
	}
    
	if (gameState->playerState == PlayerState::JUMPING
        && KeyCompare(gameState->kid.activeAnimationKey, ANIM_FALL)) {
		gameState->playerState = PlayerState::FALLING;
	}
    
	const FloorCollider& floor = levelData->floor;
    
	Vec2 deltaCoords = gameState->playerVel * deltaTime + rootMotion;
    
	Vec2 playerPos = floor.GetWorldPosFromCoords(gameState->playerCoords);
	Vec2 playerPosNew = floor.GetWorldPosFromCoords(gameState->playerCoords + deltaCoords);
	Vec2 deltaPos = playerPosNew - playerPos;
    
	FixedArray<LineColliderIntersect, LINE_COLLIDERS_MAX> intersects;
	GetLineColliderIntersections(levelData->lineColliders.ToArray(), playerPos, deltaPos,
                                 LINE_COLLIDER_MARGIN, &intersects);
	for (uint64 i = 0; i < intersects.size; i++) {
		if (gameState->currentPlatform == intersects[i].collider) {
			continue;
		}
        
		float32 newDeltaCoordX = deltaCoords.x;
		if (deltaPos.x != 0.0f) {
			float32 tX = (intersects[i].pos.x - playerPos.x) / deltaPos.x;
			newDeltaCoordX = deltaCoords.x * tX;
		}
		Vec2 newFloorPos, newFloorNormal;
		floor.GetInfoFromCoordX(gameState->playerCoords.x + newDeltaCoordX, &newFloorPos, &newFloorNormal);
        
		const float32 COS_WALK_ANGLE = cosf(PI_F / 4.0f);
		float32 dotCollisionFloorNormal = Dot(newFloorNormal, intersects[i].normal);
		if (AbsFloat32(dotCollisionFloorNormal) >= COS_WALK_ANGLE) {
			// Walkable floor
			if (gameState->playerState == PlayerState::FALLING) {
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
		bool playerOverPlatform = GetLineColliderCoordYFromFloorCoordX(*gameState->currentPlatform, floor,
                                                                       gameState->playerCoords.x + deltaCoords.x,
                                                                       &platformHeight);
		if (playerOverPlatform) {
			floorHeightCoord = platformHeight;
		}
		else {
			gameState->currentPlatform = nullptr;
			gameState->playerState = PlayerState::FALLING;
		}
	}
    
	Vec2 playerCoordsNew = gameState->playerCoords + deltaCoords;
	if (playerCoordsNew.y < floorHeightCoord || gameState->currentPlatform != nullptr) {
		playerCoordsNew.y = floorHeightCoord;
		gameState->prevFloorCoordY = floorHeightCoord;
		gameState->playerState = PlayerState::GROUNDED;
	}
    
    const TextureGL* textureRock = GetTexture(gameState->assets, TextureId::ROCK);
	{ // rock
		Vec2 size = ToVec2(textureRock->size) / gameState->refPixelsPerUnit;
		float32 radius = size.y / 2.0f * 0.8f;
		gameState->rock.angle = -gameState->rock.coords.x / radius;
		gameState->rock.coords.y = radius;
	}
    
	const float32 GRAB_RANGE = 0.2f;
    
	if (isInteractKeyPressed && gameState->grabbedObject.coordsPtr == nullptr) {
		FixedArray<GrabbedObjectInfo, 10> candidates;
		candidates.size = 0;
		Vec2 rockSize = ToVec2(textureRock->size) / gameState->refPixelsPerUnit;
		float32 rockRadius = rockSize.y / 2.0f * 0.8f;
		candidates.Append({
                              &gameState->rock.coords,
                              Vec2 { rockRadius * 1.2f, rockRadius * 1.7f },
                              Vec2 { 0.0f, rockRadius * 2.0f }
                          });
		for (uint64 i = 0; i < candidates.size; i++) {
			if (IsGrabbableObjectInRange(gameState->playerCoords, candidates[i], floor.length)) {
				gameState->grabbedObject = candidates[i];
				break;
			}
		}
	}
    
	if (wasInteractKeyPressed) {
		if (gameState->liftedObject.spritePtr == nullptr) {
			const FixedArray<TextureWithPosition, LEVEL_SPRITES_MAX>& sprites = levelData->sprites;
			TextureWithPosition* newLiftedObject = nullptr;
			float32 minDist = 2.0f;
			for (uint64 i = 0; i < sprites.size; i++) {
				TextureWithPosition* sprite = const_cast<TextureWithPosition*>(&sprites[i]);
				if (sprite->type != SpriteType::OBJECT) {
					continue;
				}
                
				Vec2 toCoords = sprite->coords - gameState->playerCoords;
				float32 coordDist = Mag(toCoords);
				if (coordDist < minDist) {
					newLiftedObject = sprite;
					minDist = coordDist;
				}
			}
            
			if (newLiftedObject != nullptr) {
				gameState->liftedObject.offset = Vec2 { -0.25f, 1.9f };
				gameState->liftedObject.placementOffsetX = 1.2f;
				gameState->liftedObject.coordYPrev = newLiftedObject->coords.y;
				gameState->liftedObject.spritePtr = newLiftedObject;
				//gameState->liftedObject.spritePtr->restAngle -= PI_F / 2.0f;
			}
		}
		else {
			TextureWithPosition* spritePtr = gameState->liftedObject.spritePtr;
			float32 placementOffsetX = gameState->liftedObject.placementOffsetX;
			if (spritePtr->flipped) {
				placementOffsetX = -placementOffsetX;
			}
			spritePtr->coords.x += placementOffsetX;
			spritePtr->coords.y = gameState->liftedObject.coordYPrev;
			//spritePtr->restAngle += PI_F / 2.0f;
			gameState->liftedObject.spritePtr = nullptr;
		}
	}
    
	TextureWithPosition* liftedSprite = gameState->liftedObject.spritePtr;
	if (liftedSprite != nullptr) {
		liftedSprite->coords = gameState->playerCoords;
		liftedSprite->flipped = !gameState->facingRight;
		Vec2 offset = gameState->liftedObject.offset;
		if (liftedSprite->flipped) {
			offset.x = -offset.x;
		}
		liftedSprite->coords += offset;
	}
    
	if (gameState->grabbedObject.coordsPtr != nullptr) {
		if (IsGrabbableObjectInRange(gameState->playerCoords, gameState->grabbedObject, floor.length)
            && isInteractKeyPressed) {
			float32 deltaX = playerCoordsNew.x - gameState->playerCoords.x;
			Vec2* coordsPtr = gameState->grabbedObject.coordsPtr;
			coordsPtr->x += deltaX;
			if (coordsPtr->x < 0.0f) {
				coordsPtr->x += floor.length;
			}
			else if (coordsPtr->x > floor.length) {
				coordsPtr->x -= floor.length;
			}
		}
		else {
			gameState->grabbedObject.coordsPtr = nullptr;
		}
	}
    
	if (levelData->bounded) {
		if (gameState->playerCoords.x >= levelData->bounds.x && playerCoordsNew.x < levelData->bounds.x) {
			playerCoordsNew.x = levelData->bounds.x;
		}
		if (gameState->playerCoords.x <= levelData->bounds.y && playerCoordsNew.x > levelData->bounds.y) {
			playerCoordsNew.x = levelData->bounds.y;
		}
	}
	if (playerCoordsNew.x < 0.0f) {
		playerCoordsNew.x += floor.length;
	}
	else if (playerCoordsNew.x > floor.length) {
		playerCoordsNew.x -= floor.length;
	}
	gameState->playerCoords = playerCoordsNew;
    
	Array<HashKey> paperNextAnims;
	paperNextAnims.size = 0;
	UpdateAnimatedSprite(&gameState->paper, gameState->assets, deltaTime, paperNextAnims);
    
	if (gameState->kmKey) {
		return;
	}
    
	if (!levelData->lockedCamera) {
		const float32 CAMERA_FOLLOW_ACCEL_DIST_MIN = 3.0f;
		const float32 CAMERA_FOLLOW_ACCEL_DIST_MAX = 10.0f;
		float32 cameraFollowLerpMag = 0.08f;
		Vec2 cameraCoordsTarget = gameState->playerCoords;
		if (cameraCoordsTarget.y > gameState->prevFloorCoordY) {
			cameraCoordsTarget.y = gameState->prevFloorCoordY;
		}
        
		// Wrap camera if necessary
		float32 dist = Mag(cameraCoordsTarget - gameState->cameraCoords);
		Vec2 cameraCoordsWrap = gameState->cameraCoords;
		cameraCoordsWrap.x += floor.length;
		float32 altDist = Mag(cameraCoordsTarget - cameraCoordsWrap);
		if (altDist < dist) {
			gameState->cameraCoords = cameraCoordsWrap;
			dist = altDist;
		}
		else {
			cameraCoordsWrap.x -= floor.length * 2.0f;
			altDist = Mag(cameraCoordsTarget - cameraCoordsWrap);
			if (altDist < dist) {
				gameState->cameraCoords = cameraCoordsWrap;
				dist = altDist;
			}
		}
        
		if (dist > CAMERA_FOLLOW_ACCEL_DIST_MIN) {
			float32 lerpMagAccelT = (dist - CAMERA_FOLLOW_ACCEL_DIST_MIN)
				/ (CAMERA_FOLLOW_ACCEL_DIST_MAX - CAMERA_FOLLOW_ACCEL_DIST_MIN);
			lerpMagAccelT = ClampFloat32(lerpMagAccelT, 0.0f, 1.0f);
			cameraFollowLerpMag += (1.0f - cameraFollowLerpMag) * lerpMagAccelT;
		}
		gameState->cameraCoords = Lerp(gameState->cameraCoords, cameraCoordsTarget,
                                       cameraFollowLerpMag);
	}
    
	Vec2 camFloorPos, camFloorNormal;
	floor.GetInfoFromCoordX(gameState->cameraCoords.x, &camFloorPos, &camFloorNormal);
	float32 angle = acosf(Dot(Vec2::unitY, camFloorNormal));
	if (camFloorNormal.x > 0.0f) {
		angle = -angle;
	}
	gameState->cameraPos = camFloorPos + camFloorNormal * gameState->cameraCoords.y;
	gameState->cameraRot = QuatFromAngleUnitAxis(angle, Vec3::unitZ);
}

internal void DrawWorld(const GameState* gameState, SpriteDataGL* spriteDataGL,
                        Mat4 projection, ScreenInfo screenInfo)
{
	const LevelData* levelData = GetLevelData(gameState->assets, gameState->activeLevelId);
	const FloorCollider& floor = levelData->floor;
    
	spriteDataGL->numSprites = 0;
    
	Vec2 playerFloorPos, playerFloorNormal;
	floor.GetInfoFromCoordX(gameState->playerCoords.x,
                            &playerFloorPos, &playerFloorNormal);
	Vec2 playerPos = playerFloorPos + playerFloorNormal * gameState->playerCoords.y;
	float32 playerAngle = acosf(Dot(Vec2::unitY, playerFloorNormal));
	if (playerFloorNormal.x > 0.0f) {
		playerAngle = -playerAngle;
	}
	Quat playerRot = QuatFromAngleUnitAxis(playerAngle, Vec3::unitZ);
    const AnimatedSprite* kidSprite = GetAnimatedSprite(gameState->assets, AnimatedSpriteId::KID);
	Vec2 playerSize = ToVec2(kidSprite->textureSize) / gameState->refPixelsPerUnit;
	Vec2 anchorUnused = Vec2::zero;
	DrawAnimatedSprite(gameState->kid, gameState->assets, spriteDataGL,
                       playerPos, playerSize, anchorUnused, playerRot, 1.0f, !gameState->facingRight);
    
	{ // level sprites
		for (uint64 i = 0; i < levelData->sprites.size; i++) {
			const TextureWithPosition* sprite = &levelData->sprites[i];
			Vec2 pos;
			Quat baseRot;
			Quat rot;
			if (sprite->type == SpriteType::OBJECT) {
				baseRot = QuatFromAngleUnitAxis(-sprite->restAngle, Vec3::unitZ);
                
				Vec2 floorPos, floorNormal;
				floor.GetInfoFromCoordX(sprite->coords.x, &floorPos, &floorNormal);
				pos = floorPos + floorNormal * sprite->coords.y;
				if (sprite == gameState->liftedObject.spritePtr) {
					rot = playerRot;
				}
				else {
					float32 angle = acosf(Dot(Vec2::unitY, floorNormal));
					if (floorNormal.x > 0.0f) {
						angle = -angle;
					}
					rot = QuatFromAngleUnitAxis(angle, Vec3::unitZ);
				}
			}
			else {
				pos = sprite->pos;
				baseRot = Quat::one;
				rot = Quat::one;
			}
			if (sprite->type == SpriteType::LABEL) {
				Vec2 offset = WrappedWorldOffset(playerPos, pos, floor.length);
				if (AbsFloat32(offset.x) > 1.0f || offset.y < 0.0f || offset.y > 2.5f) {
					continue;
				}
			}
			Vec2 size = ToVec2(sprite->texture.size) / gameState->refPixelsPerUnit;
			Mat4 transform = CalculateTransform(pos, size, sprite->anchor,
                                                baseRot, rot, sprite->flipped);
			PushSprite(spriteDataGL, transform, 1.0f, sprite->texture.textureID);
		}
	}
    
    const TextureGL* textureRock = GetTexture(gameState->assets, TextureId::ROCK);
	{ // rock
		Vec2 pos = floor.GetWorldPosFromCoords(gameState->rock.coords);
		Vec2 size = ToVec2(textureRock->size) / gameState->refPixelsPerUnit;
		Quat rot = QuatFromAngleUnitAxis(gameState->rock.angle, Vec3::unitZ);
		Mat4 transform = CalculateTransform(pos, size, Vec2::one / 2.0f, rot, false);
		PushSprite(spriteDataGL, transform, 1.0f, textureRock->textureID);
	}
    
	Mat4 view = CalculateViewMatrix(gameState->cameraPos, gameState->cameraRot,
                                    gameState->refPixelScreenHeight, gameState->refPixelsPerUnit, gameState->cameraOffsetFracY);
	DrawSprites(gameState->renderState, *spriteDataGL, projection * view);
    
	spriteDataGL->numSprites = 0;
    
	if (gameState->kmKey) {
		DrawSprites(gameState->renderState, *spriteDataGL, projection);
		return;
	}
    
	const float32 aspectRatio = (float32)screenInfo.size.x / screenInfo.size.y;
	const float32 screenHeightUnits = (float32)gameState->refPixelScreenHeight / gameState->refPixelsPerUnit;
	const Vec2 screenSizeWorld = { screenHeightUnits * aspectRatio, screenHeightUnits };
	DrawAnimatedSprite(gameState->paper, gameState->assets, spriteDataGL,
                       Vec2::zero, screenSizeWorld, Vec2::one / 2.0f, Quat::one, 0.5f,
                       false);
    
	DrawSprites(gameState->renderState, *spriteDataGL, projection);
}

void GameUpdateAndRender(const PlatformFunctions& platformFuncs, const GameInput& input,
                         const ScreenInfo& screenInfo, float32 deltaTime,
                         GameMemory* memory, GameAudio* audio)
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
    
	// NOTE make sure deltaTime values are reasonable
	const float32 MAX_DELTA_TIME = 1.0f / 10.0f;
	if (deltaTime < 0.0f) {
		LOG_ERROR("Negative deltaTime %f !!\n", deltaTime);
		deltaTime = 1.0f / 60.0f; // eh... idk
	}
	if (deltaTime > MAX_DELTA_TIME) {
		LOG_WARN("Large deltaTime %f, capped to %f\n", deltaTime, MAX_DELTA_TIME);
		deltaTime = MAX_DELTA_TIME;
	}
    
	if (memory->shouldInitGlobalVariables) {
		// Initialize global function names
#define FUNC(returntype, name, ...) name = \
platformFuncs.glFunctions.name;
        GL_FUNCTIONS_BASE
			GL_FUNCTIONS_ALL
#undef FUNC
        
		memory->shouldInitGlobalVariables = false;
		LOG_INFO("Initialized global variables\n");
	}
    
	if (!memory->isInitialized) {
        LinearAllocator allocator(memory->transient.size, memory->transient.memory);
        
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
        
		glDisable(GL_CULL_FACE);
		//glFrontFace(GL_CCW);
		//glCullFace(GL_BACK);
        
		glLineWidth(1.0f);
        
		// Execute constructors for everything in GameState
		gameState = new (memory->permanent.memory) GameState();
        
		if (!InitAudioState(&allocator, &gameState->audioState, audio)) {
			DEBUG_PANIC("Failed to init audio state\n");
		}
        
		gameState->aspectRatio = 4.0f / 3.0f;
		gameState->refPixelScreenHeight = 1440;
		gameState->refPixelsPerUnit = 120.0f;
		gameState->minBorderFrac = 0.05f;
		gameState->borderRadius = 20;
		gameState->cameraOffsetFracY = -1.0f / 2.8f;
        
		// Game data
		gameState->playerVel = Vec2::zero;
		gameState->facingRight = true;
		gameState->currentPlatform = nullptr;
        
		gameState->grabbedObject.coordsPtr = nullptr;
		gameState->liftedObject.spritePtr = nullptr;
        
        // TODO eh, idk... sure
		for (int i = 0; i < (int)LevelId::COUNT; i++) {
			gameState->assets.levels[i].loaded = false;
		}
		const LevelId FIRST_LEVEL = LevelId::OVERWORLD;
        Vec2 startPos = { 152.0f, 1.0f };
		if (!SetActiveLevel(gameState, FIRST_LEVEL, startPos, memory->transient)) {
			DEBUG_PANIC("Failed to load level %d\n", FIRST_LEVEL);
		}
        const Array<char> FIRST_LEVEL_NAME = GetLevelName(FIRST_LEVEL);
		char levelPsdPath[PATH_MAX_LENGTH];
		stbsp_snprintf(levelPsdPath, PATH_MAX_LENGTH, "data/psd/%.*s.psd",
                       (int)FIRST_LEVEL_NAME.size, FIRST_LEVEL_NAME.data);
		FileChangedSinceLastCall(ToString(levelPsdPath)); // TODO hacky. move this to SetActiveLevel?
        
		gameState->grainTime = 0.0f;
        
		gameState->debugView = false;
		gameState->kmKey = false;
		gameState->editorScaleExponent = 0.5f;
        
		gameState->floorVertexSelected = -1;
        
		// Rendering stuff
		InitRenderState(&allocator, gameState->renderState);
        
		gameState->rectGL = InitRectGL(&allocator);
		gameState->texturedRectGL = InitTexturedRectGL(&allocator);
		gameState->lineGL = InitLineGL(&allocator);
		gameState->textGL = InitTextGL(&allocator);
        
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
        
		gameState->assets.screenShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/screen.frag");
		gameState->assets.bloomExtractShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/bloomExtract.frag");
		gameState->assets.bloomBlendShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/bloomBlend.frag");
		gameState->assets.blurShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/blur.frag");
		gameState->assets.grainShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/grain.frag");
		// gameState->lutShader = LoadShaders(&allocator, "shaders/screen.vert", "shaders/lut.frag");
        
        // Fonts
        if (!LoadAlphabet(memory->transient, &gameState->assets.alphabet)) {
            DEBUG_PANIC("Failed to load alphabet\n");
        }
        
		FT_Error error = FT_Init_FreeType(&gameState->ftLibrary);
		if (error) {
			LOG_ERROR("FreeType init error: %d\n", error);
		}
		gameState->assets.fontFaceSmall = LoadFontFace(&allocator, gameState->ftLibrary,
                                                       "data/fonts/ocr-a/regular.ttf", 18);
		gameState->assets.fontFaceMedium = LoadFontFace(&allocator, gameState->ftLibrary,
                                                        "data/fonts/ocr-a/regular.ttf", 24);
        
        // Game objects
        const LevelData* levelData = GetLevelData(gameState->assets, gameState->activeLevelId);
		gameState->rock.coords = { levelData->floor.length - 10.0f, 0.0f };
		gameState->rock.angle = 0.0f;
		if (!LoadTextureFromPng(&allocator, "data/sprites/rock.png",
                                GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                GetTexture(&gameState->assets, TextureId::ROCK))) {
			DEBUG_PANIC("Failed to load rock\n");
		}
        
        AnimatedSprite* spriteKid = GetAnimatedSprite(&gameState->assets, AnimatedSpriteId::KID);
		if (!LoadAnimatedSprite(spriteKid, ToString("kid"), gameState->refPixelsPerUnit, memory->transient)) {
			DEBUG_PANIC("Failed to load kid animation sprite\n");
		}
		gameState->kid.animatedSpriteId = AnimatedSpriteId::KID;
		gameState->kid.activeAnimationKey = spriteKid->startAnimationKey;
		gameState->kid.activeFrame = 0;
		gameState->kid.activeFrameRepeat = 0;
		gameState->kid.activeFrameTime = 0.0f;
		// TODO priming file changed... hmm
		FileChangedSinceLastCall(ToString("data/kmkv/animations/kid.kmkv"));
		FileChangedSinceLastCall(ToString("data/psd/kid.psd"));
        
        AnimatedSprite* spritePaper = GetAnimatedSprite(&gameState->assets, AnimatedSpriteId::PAPER);
		if (!LoadAnimatedSprite(spritePaper, ToString("paper"), gameState->refPixelsPerUnit, memory->transient)) {
            DEBUG_PANIC("Failed to load paper animation sprite\n");
        }
        gameState->paper.animatedSpriteId = AnimatedSpriteId::PAPER;
        gameState->paper.activeAnimationKey = spritePaper->startAnimationKey;
        gameState->paper.activeFrame = 0;
        gameState->paper.activeFrameRepeat = 0;
        gameState->paper.activeFrameTime = 0.0f;
        
        // TODO priming file changed... hmm
        FileChangedSinceLastCall(ToString("data/kmkv/animations/paper.kmkv"));
        FileChangedSinceLastCall(ToString("data/psd/paper.psd"));
        
        // Game static sprites/textures
        if (!LoadTextureFromPng(&allocator, "data/sprites/frame-corner.png",
                                GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                GetTexture(&gameState->assets, TextureId::FRAME_CORNER))) {
            DEBUG_PANIC("Failed to load frame corner texture\n");
        }
        if (!LoadTextureFromPng(&allocator, "data/sprites/pixel.png",
                                GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                GetTexture(&gameState->assets, TextureId::PIXEL))) {
            DEBUG_PANIC("Failed to load pixel texture\n");
        }
        
#if 0
        if (!LoadTextureFromPng(&allocator, "data/luts/lutbase.png",
                                GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &gameState->lutBase)) {
            DEBUG_PANIC("Failed to load base LUT\n");
        }
        
        if (!LoadTextureFromPng(&allocator, "data/luts/kodak5205.png",
                                GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &gameState->lut1)) {
            DEBUG_PANIC("Failed to load base LUT\n");
        }
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
        
		LOG_INFO("Updated screen-size-dependent info\n");
	}
    
	const Array<char> activeLevelName = GetLevelName(gameState->activeLevelId);
	FixedArray<char, PATH_MAX_LENGTH> levelPsdPath;
	levelPsdPath.Clear();
    // TODO this part of the code shouldn't know about this path
	levelPsdPath.Append(ToString("data/psd/"));
	levelPsdPath.Append(activeLevelName);
	levelPsdPath.Append(ToString(".psd"));
	if (FileChangedSinceLastCall(levelPsdPath.ToArray())) {
		LOG_INFO("reloading level %.*s\n", (int)activeLevelName.size, activeLevelName.data);
        LevelData* activeLevelData = GetLevelData(&gameState->assets, gameState->activeLevelId);
        if (activeLevelData->loaded) {
			UnloadLevelData(activeLevelData);
		}
        
		if (!SetActiveLevel(gameState, gameState->activeLevelId, gameState->playerCoords, memory->transient)) {
			DEBUG_PANIC("Failed to reload level %.*s\n",
                        (int)activeLevelName.size, activeLevelName.data);
		}
	}
	if (FileChangedSinceLastCall(ToString("data/kmkv/animations/kid.kmkv"))
		|| FileChangedSinceLastCall(ToString("data/psd/kid.psd"))) {
		LOG_INFO("reloading kid animation sprite\n");
        
        AnimatedSprite* spriteKid = GetAnimatedSprite(&gameState->assets, AnimatedSpriteId::KID);
		UnloadAnimatedSprite(spriteKid);
		if (!LoadAnimatedSprite(spriteKid, ToString("kid"), gameState->refPixelsPerUnit, memory->transient)) {
			DEBUG_PANIC("Failed to reload kid animation sprite\n");
		}
	}
    
	// gameState->grainTime = fmod(gameState->grainTime + deltaTime, 5.0f);
    
#if 0
	// Toggle global mute
	if (WasKeyPressed(input, KM_KEY_M)) {
		gameState->audioState.globalMute = !gameState->audioState.globalMute;
	}
#endif
    
	UpdateWorld(gameState, deltaTime, input, memory->transient);
    
	// ---------------------------- Begin Rendering ---------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColorDepth[0].framebuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
	Mat4 projection = CalculateProjectionMatrix(screenInfo, gameState->refPixelScreenHeight,
                                                gameState->refPixelsPerUnit);
	if (gameState->kmKey) {
		projection = projection * Scale(ScaleExponentToWorldScale(gameState->editorScaleExponent));
	}
    
	DEBUG_ASSERT(memory->transient.size >= sizeof(SpriteDataGL));
	SpriteDataGL* spriteDataGL = (SpriteDataGL*)memory->transient.memory;
    
	DrawWorld(gameState, spriteDataGL, projection, screenInfo);
    
    if (!gameState->kmKey) {
        // Draw border
        const Vec4 borderColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        const Vec2Int borderSize = GetBorderSize(screenInfo, gameState->aspectRatio, gameState->minBorderFrac);
        DrawRect(gameState->rectGL, screenInfo, Vec2Int::zero, Vec2::zero,
                 Vec2Int { borderSize.x, screenInfo.size.y }, borderColor);
        DrawRect(gameState->rectGL, screenInfo, Vec2Int { screenInfo.size.x, 0 }, Vec2 { 1.0f, 0.0f },
                 Vec2Int { borderSize.x, screenInfo.size.y }, borderColor);
        DrawRect(gameState->rectGL, screenInfo, Vec2Int::zero, Vec2::zero,
                 Vec2Int { screenInfo.size.x, borderSize.y }, borderColor);
        DrawRect(gameState->rectGL, screenInfo, Vec2Int { 0, screenInfo.size.y }, Vec2 { 0.0f, 1.0f },
                 Vec2Int { screenInfo.size.x, borderSize.y }, borderColor);
        
        const TextureGL* textureFrameCorner = GetTexture(gameState->assets, TextureId::FRAME_CORNER);
        const int cornerRadius = gameState->borderRadius;
        DrawTexturedRect(gameState->texturedRectGL, screenInfo,
                         borderSize, Vec2 { 0.0f, 0.0f },
                         Vec2Int { cornerRadius, cornerRadius }, false, false, textureFrameCorner->textureID);
        DrawTexturedRect(gameState->texturedRectGL, screenInfo,
                         Vec2Int { screenInfo.size.x - borderSize.x, borderSize.y }, Vec2 { 1.0f, 0.0f },
                         Vec2Int { cornerRadius, cornerRadius }, true, false, textureFrameCorner->textureID);
        DrawTexturedRect(gameState->texturedRectGL, screenInfo,
                         Vec2Int { borderSize.x, screenInfo.size.y - borderSize.y }, Vec2 { 0.0f, 1.0f },
                         Vec2Int { cornerRadius, cornerRadius }, false, true, textureFrameCorner->textureID);
        DrawTexturedRect(gameState->texturedRectGL, screenInfo,
                         screenInfo.size - borderSize, Vec2 { 1.0f, 1.0f },
                         Vec2Int { cornerRadius, cornerRadius }, true, true, textureFrameCorner->textureID);
    }
    
	// ------------------------ Post processing passes ------------------------
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
    
	// Apply filters
	// PostProcessGrain(gameState->framebuffersColorDepth[0], gameState->framebuffersColor[0],
	//     gameState->screenQuadVertexArray,
	//     gameState->grainShader, gameState->grainTime);
    
	// PostProcessLUT(gameState->framebuffersColorDepth[0],
	// 	gameState->framebuffersColor[0],
	// 	gameState->screenQuadVertexArray,
	// 	gameState->lutShader, gameState->lutBase);
    
	// Render to screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
	glBindVertexArray(gameState->screenQuadVertexArray);
	glUseProgram(gameState->assets.screenShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gameState->framebuffersColorDepth[0].color);
	GLint loc = glGetUniformLocation(gameState->assets.screenShader, "framebufferTexture");
	glUniform1i(loc, 0);
    
	glDrawArrays(GL_TRIANGLES, 0, 6);
    
	// ---------------------------- End Rendering -----------------------------
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // ------------------------------ Audio -----------------------------------
	OutputAudio(audio, gameState, input, memory->transient);
    
    // ------------------------------ Debug -----------------------------------
	Mat4 view = CalculateViewMatrix(gameState->cameraPos, gameState->cameraRot,
                                    gameState->refPixelScreenHeight, gameState->refPixelsPerUnit, gameState->cameraOffsetFracY);
    
	bool wasDebugKeyPressed = WasKeyPressed(input, KM_KEY_G)
		|| (input.controllers[0].isConnected
            && input.controllers[0].x.isDown && input.controllers[0].x.transitions == 1);
	if (wasDebugKeyPressed) {
		gameState->debugView = !gameState->debugView;
	}
	if (WasKeyPressed(input, KM_KEY_K)) {
		gameState->kmKey = !gameState->kmKey;
		gameState->editorScaleExponent = 0.5f;
	}
    
    const Vec2Int DEBUG_MARGIN_SCREEN = { 30, 45 };
    const Vec2Int DEBUG_BORDER_PANEL = { 10, 10 };
    const Vec4 DEBUG_BACKGROUND_COLOR = { 0.0f, 0.0f, 0.0f, 0.5f };
	const Vec4 DEBUG_FONT_COLOR = { 0.95f, 0.95f, 0.95f, 1.0f };
    
    DrawDebugAudioInfo(audio, gameState, input, screenInfo, memory->transient, DEBUG_FONT_COLOR);
    
	if (gameState->debugView) {
		LinearAllocator tempAllocator(memory->transient.size, memory->transient.memory);
        
		const Mat4 viewProjection = projection * view;
		const LevelData* levelData = GetLevelData(gameState->assets, gameState->activeLevelId);
		const FloorCollider& floor = levelData->floor;
        
		const FontFace& fontMedium = gameState->assets.fontFaceMedium;
		const FontFace& fontSmall = gameState->assets.fontFaceSmall;
        
		Panel panelHotkeys;
		panelHotkeys.Begin(input, &fontSmall, 0,
                           Vec2Int { DEBUG_MARGIN_SCREEN.x, screenInfo.size.y - DEBUG_MARGIN_SCREEN.y },
                           Vec2 { 0.0f, 1.0f });
        
		panelHotkeys.Text(ToString("[F11] toggle fullscreen"));
		panelHotkeys.Text(ToString("[G]   toggle debug view"));
		panelHotkeys.Text(ToString("[K]   toggle km key"));
        
		panelHotkeys.Draw(screenInfo, gameState->rectGL, gameState->textGL, DEBUG_BORDER_PANEL,
                          DEBUG_FONT_COLOR, DEBUG_BACKGROUND_COLOR, &tempAllocator);
        
		Panel panelDebug;
        static bool panelDebugMinimized = true;
		panelDebug.Begin(input, &fontSmall, 0, screenInfo.size - DEBUG_MARGIN_SCREEN, Vec2 { 1.0f, 1.0f });
        panelDebug.TitleBar(ToString("Stats"), &panelDebugMinimized, Vec4::zero, &fontMedium);
        
		panelDebug.Text(AllocPrintf(&tempAllocator, "%.2f --- FPS", 1.0f / deltaTime));
		panelDebug.Text(Array<char>::empty);
        
		panelDebug.Text(AllocPrintf(&tempAllocator, "%.2f|%.2f --- CRD",
                                    gameState->playerCoords.x, gameState->playerCoords.y));
		Vec2 playerPosWorld = floor.GetWorldPosFromCoords(gameState->playerCoords);
		panelDebug.Text(AllocPrintf(&tempAllocator, "%.2f|%.2f --- POS",
                                    playerPosWorld.x, playerPosWorld.y));
		panelDebug.Text(Array<char>::empty);
        
		panelDebug.Text(AllocPrintf(&tempAllocator, "%.2f|%.2f - CMCRD",
                                    gameState->cameraCoords.x, gameState->cameraCoords.y));
		panelDebug.Text(AllocPrintf(&tempAllocator, "%.2f|%.2f - CMPOS",
                                    gameState->cameraPos.x, gameState->cameraPos.y));
		panelDebug.Text(Array<char>::empty);
        
		Vec2 mouseWorld = ScreenToWorld(input.mousePos, screenInfo,
                                        gameState->cameraPos, gameState->cameraRot,
                                        ScaleExponentToWorldScale(gameState->editorScaleExponent),
                                        gameState->refPixelScreenHeight, gameState->refPixelsPerUnit,
                                        gameState->cameraOffsetFracY);
		panelDebug.Text(AllocPrintf(&tempAllocator, "%.2f|%.2f - MSPOS", mouseWorld.x, mouseWorld.y));
		Vec2 mouseCoords = floor.GetCoordsFromWorldPos(mouseWorld);
		panelDebug.Text(AllocPrintf(&tempAllocator, "%.2f|%.2f - MSCRD", mouseCoords.x, mouseCoords.y));
        
		panelDebug.Text(Array<char>::empty);
        
		panelDebug.Text(AllocPrintf(&tempAllocator, "%d - STATE", gameState->playerState));
		const HashKey& kidActiveAnimKey = gameState->kid.activeAnimationKey;
		panelDebug.Text(AllocPrintf(&tempAllocator, "%.*s -- ANIM",
                                    (int)kidActiveAnimKey.string.size, kidActiveAnimKey.string.data));
        
		panelDebug.Draw(screenInfo, gameState->rectGL, gameState->textGL, DEBUG_BORDER_PANEL,
                        DEBUG_FONT_COLOR, DEBUG_BACKGROUND_COLOR, &tempAllocator);
        
        // TODO add player scaling slider
        // TODO save these settings somewhere
		Panel panelGeometry;
        static bool panelGeometryMinimized = true;
		panelGeometry.Begin(input, &fontSmall, PanelFlag::GROW_UPWARDS, DEBUG_MARGIN_SCREEN, Vec2 { 0.0f, 0.0f });
        panelGeometry.TitleBar(ToString("View & Camera"), &panelGeometryMinimized, Vec4::zero, &fontMedium);
        
		static bool showDebugGeometry = false;
		panelGeometry.Checkbox(&showDebugGeometry, ToString("Enable debug geometry"));
		panelGeometry.Text(Array<char>::empty);
        
		if (panelGeometry.SliderFloat(&gameState->aspectRatio, 1.0f / 3.0f, 3.0f)) {
		}
		panelGeometry.Text(ToString("Aspect Ratio"));
        
		if (panelGeometry.SliderFloat(&gameState->minBorderFrac, 0.0f, 0.5f)) {
		}
		panelGeometry.Text(ToString("Min Border"));
        
		float32 borderRadius = (float32)gameState->borderRadius;
		if (panelGeometry.SliderFloat(&borderRadius, 0.0f, 300.0f)) {
			gameState->borderRadius = (int)borderRadius;
		}
		panelGeometry.Text(ToString("Border Radius"));
        
		float32 screenHeightFloat = (float32)gameState->refPixelScreenHeight;
		if (panelGeometry.SliderFloat(&screenHeightFloat, 720.0f, 3000.0f)) {
			gameState->refPixelScreenHeight = (int)screenHeightFloat;
		}
		panelGeometry.Text(ToString("Y Resolution"));
        
		if (panelGeometry.SliderFloat(&gameState->cameraOffsetFracY, -0.5f, 0.0f)) {
		}
		panelGeometry.Text(ToString("Camera Offset"));
        
		panelGeometry.Draw(screenInfo, gameState->rectGL, gameState->textGL, DEBUG_BORDER_PANEL,
                           DEBUG_FONT_COLOR, DEBUG_BACKGROUND_COLOR, &tempAllocator);
        
		if (showDebugGeometry) {
			DEBUG_ASSERT(memory->transient.size >= sizeof(LineGLData));
			LineGLData* lineData = (LineGLData*)memory->transient.memory;
            
			{ // mouse
				Vec2 floorPos, floorNormal;
				floor.GetInfoFromCoordX(mouseCoords.x, &floorPos, &floorNormal);
				lineData->count = 2;
				lineData->pos[0] = ToVec3(floorPos, 0.0f);
				lineData->pos[1] = ToVec3(floorPos + floorNormal * mouseCoords.y, 0.0f);
				DrawLine(gameState->lineGL, viewProjection, lineData,
                         Vec4 { 0.5f, 0.4f, 0.0f, 0.25f });
			}
            
			// sprites
			for (uint64 i = 0; i < levelData->sprites.size; i++) {
				if (levelData->sprites[i].type != SpriteType::OBJECT) {
					continue;
				}
				const float32 POINT_CROSS_OFFSET = 0.05f;
				Vec4 centerColor = Vec4 { 0.0f, 1.0f, 1.0f, 1.0f };
				Vec4 boundsColor = Vec4 { 1.0f, 0.0f, 1.0f, 1.0f };
				const TextureWithPosition& sprite = levelData->sprites[i];
				lineData->count = 2;
				Vec2 worldPos = floor.GetWorldPosFromCoords(sprite.pos);
				Vec3 pos = ToVec3(worldPos, 0.0f);
				lineData->pos[0] = pos - Vec3::unitX * POINT_CROSS_OFFSET;
				lineData->pos[1] = pos + Vec3::unitX * POINT_CROSS_OFFSET;
				DrawLine(gameState->lineGL, viewProjection, lineData, centerColor);
				lineData->pos[0] = pos - Vec3::unitY * POINT_CROSS_OFFSET;
				lineData->pos[1] = pos + Vec3::unitY * POINT_CROSS_OFFSET;
				DrawLine(gameState->lineGL, viewProjection, lineData, centerColor);
				Vec2 worldSize = ToVec2(sprite.texture.size) / gameState->refPixelsPerUnit;
				Vec2 anchorOffset = Vec2 {
					sprite.anchor.x * worldSize.x,
					sprite.anchor.y * worldSize.y
				};
				Vec3 origin = ToVec3(worldPos - anchorOffset, 0.0f);
				// TODO rotate sprite box
				lineData->count = 5;
				lineData->pos[0] = origin;
				lineData->pos[1] = origin;
				lineData->pos[1].x += worldSize.x;
				lineData->pos[2] = origin + ToVec3(worldSize, 0.0f);
				lineData->pos[3] = origin;
				lineData->pos[3].y += worldSize.y;
				lineData->pos[4] = lineData->pos[0];
				DrawLine(gameState->lineGL, viewProjection, lineData, boundsColor);
			}
            
			{ // floor
				Vec4 floorSmoothColorMax = { 0.4f, 0.4f, 0.5f, 1.0f };
				Vec4 floorSmoothColorMin = { 0.4f, 0.4f, 0.5f, 0.2f };
				const float32 FLOOR_SMOOTH_STEPS = 0.2f;
				const int FLOOR_HEIGHT_NUM_STEPS = 10;
				const float32 FLOOR_HEIGHT_STEP = 0.5f;
				const float32 FLOOR_NORMAL_LENGTH = FLOOR_HEIGHT_STEP;
				const float32 FLOOR_LENGTH = floor.length;
				for (int i = 0; i < FLOOR_HEIGHT_NUM_STEPS; i++) {
					float32 height = i * FLOOR_HEIGHT_STEP;
					lineData->count = 0;
					for (float32 floorX = 0.0f; floorX < FLOOR_LENGTH; floorX += FLOOR_SMOOTH_STEPS) {
						Vec2 fPos, fNormal;
						floor.GetInfoFromCoordX(floorX, &fPos, &fNormal);
						Vec2 pos = fPos + fNormal * height;
						lineData->pos[lineData->count++] = ToVec3(pos, 0.0f);
						lineData->pos[lineData->count++] = ToVec3(
                                                                  pos + fNormal * FLOOR_NORMAL_LENGTH, 0.0f);
						lineData->pos[lineData->count++] = ToVec3(pos, 0.0f);
					}
					DrawLine(gameState->lineGL, viewProjection, lineData,
                             Lerp(floorSmoothColorMax, floorSmoothColorMin,
                                  (float32)i / (FLOOR_HEIGHT_NUM_STEPS - 1)));
				}
			}
            
			{ // line colliders
				Vec4 lineColliderColor = { 0.0f, 0.6f, 0.6f, 1.0f };
				const FixedArray<LineCollider, LINE_COLLIDERS_MAX>& lineColliders = levelData->lineColliders;
				for (uint64 i = 0; i < lineColliders.size; i++) {
					const LineCollider& lineCollider = lineColliders[i];
					lineData->count = (int)lineCollider.line.size;
					for (uint64 v = 0; v < lineCollider.line.size; v++) {
						lineData->pos[v] = ToVec3(lineCollider.line[v], 0.0f);
					}
					DrawLine(gameState->lineGL, viewProjection, lineData, lineColliderColor);
				}
			}
            
			{ // level transitions
				Vec4 levelTransitionColor = { 0.1f, 0.3f, 1.0f, 1.0f };
				const FixedArray<LevelTransition, LEVEL_TRANSITIONS_MAX>& transitions = levelData->levelTransitions;
				lineData->count = 5;
				for (uint64 i = 0; i < transitions.size; i++) {
					Vec2 coords = transitions[i].coords;
					Vec2 range = transitions[i].range;
					lineData->pos[0] = ToVec3(coords - range, 0.0f);
					lineData->pos[1] = lineData->pos[0];
					lineData->pos[1].x += range.x * 2.0f;
					lineData->pos[2] = ToVec3(coords + range, 0.0f);
					lineData->pos[3] = lineData->pos[0];
					lineData->pos[3].y += range.y * 2.0f;
					lineData->pos[4] = lineData->pos[0];
					for (int p = 0; p < 5; p++) {
						Vec2 worldPos = floor.GetWorldPosFromCoords(ToVec2(lineData->pos[p]));
						lineData->pos[p] = ToVec3(worldPos, 0.0f);
					}
					DrawLine(gameState->lineGL, viewProjection, lineData, levelTransitionColor);
				}
			}
            
			{ // bounds
				const float32 screenHeightUnits = (float32)gameState->refPixelScreenHeight / gameState->refPixelsPerUnit;
				Vec4 boundsColor = { 1.0f, 0.2f, 0.3f, 1.0f };
				if (levelData->bounded) {
					lineData->count = 2;
                    
					Vec2 boundLeftPos, boundLeftNormal;
					floor.GetInfoFromCoordX(levelData->bounds.x, &boundLeftPos, &boundLeftNormal);
					Vec2 boundLeft = floor.GetWorldPosFromCoords(Vec2 { levelData->bounds.x, 0.0f });
					lineData->pos[0] = ToVec3(boundLeftPos, 0.0f);
					lineData->pos[1] = ToVec3(boundLeftPos + boundLeftNormal * screenHeightUnits, 0.0f);
					DrawLine(gameState->lineGL, viewProjection, lineData, boundsColor);
                    
					Vec2 boundRightPos, boundRightNormal;
					floor.GetInfoFromCoordX(levelData->bounds.y, &boundRightPos, &boundRightNormal);
					lineData->pos[0] = ToVec3(boundRightPos, 0.0f);
					lineData->pos[1] = ToVec3(boundRightPos + boundRightNormal * screenHeightUnits, 0.0f);
					DrawLine(gameState->lineGL, viewProjection, lineData, boundsColor);
				}
			}
            
			{ // player
				const float32 CROSS_RADIUS = 0.2f;
				Vec4 playerColor = { 1.0f, 0.2f, 0.2f, 1.0f };
				Vec3 playerPosWorld3 = ToVec3(playerPosWorld, 0.0f);
				lineData->count = 2;
				lineData->pos[0] = playerPosWorld3 - Vec3::unitX * CROSS_RADIUS;
				lineData->pos[1] = playerPosWorld3 + Vec3::unitX * CROSS_RADIUS;
				DrawLine(gameState->lineGL, viewProjection, lineData, playerColor);
				lineData->pos[0] = playerPosWorld3 - Vec3::unitY * CROSS_RADIUS;
				lineData->pos[1] = playerPosWorld3 + Vec3::unitY * CROSS_RADIUS;
				DrawLine(gameState->lineGL, viewProjection, lineData, playerColor);
			}
		}
	}
    
    static bool alphabetAtlas = false;
    if (alphabetAtlas && WasKeyPressed(input, KM_KEY_ESCAPE)) {
        alphabetAtlas = false;
    }
    if (alphabetAtlas) {
        AlphabetAtlasUpdateAndRender(&gameState->assets.alphabet, input, memory->transient,
                                     gameState->rectGL, gameState->texturedRectGL, gameState->textGL,
                                     screenInfo, gameState->assets.fontFaceSmall, gameState->assets.fontFaceMedium);
    }
    else if (gameState->kmKey) {
        LinearAllocator tempAllocator(memory->transient.size, memory->transient.memory);
        
        FloorCollider* floor = &(GetLevelData(&gameState->assets, gameState->activeLevelId)->floor);
        
        const FontFace& fontMedium = gameState->assets.fontFaceMedium;
        const FontFace& fontSmall = gameState->assets.fontFaceSmall;
        
        const Vec4 kmKeyFontColor = { 0.0f, 0.2f, 1.0f, 1.0f };
        
        Panel panelKmKey;
        static bool panelKmKeyMinimized = false;
        panelKmKey.Begin(input, &fontSmall, PanelFlag::GROW_UPWARDS, DEBUG_MARGIN_SCREEN, Vec2::zero);
        panelKmKey.TitleBar(ToString("KM KEY"), &panelKmKeyMinimized, Vec4::zero, &fontMedium);
        
        const Array<char> activeLevelNameD = GetLevelName(gameState->activeLevelId);
        panelKmKey.Text(AllocPrintf(&tempAllocator, "Loaded level: %.*s",
                                    (int)activeLevelNameD.size, activeLevelNameD.data));
        
        static bool editCollision = false;
        panelKmKey.Checkbox(&editCollision, ToString("Ground Editor"));
        
        if (panelKmKey.Button(ToString("Alphabet Atlas"))) {
            alphabetAtlas = !alphabetAtlas;
        }
        
        panelKmKey.Draw(screenInfo, gameState->rectGL, gameState->textGL, DEBUG_BORDER_PANEL,
                        kmKeyFontColor, Vec4::zero, &tempAllocator);
        
        
        Vec2 mouseWorldPosStart = ScreenToWorld(input.mousePos, screenInfo,
                                                gameState->cameraPos, gameState->cameraRot,
                                                ScaleExponentToWorldScale(gameState->editorScaleExponent),
                                                gameState->refPixelScreenHeight, gameState->refPixelsPerUnit,
                                                gameState->cameraOffsetFracY);
        Vec2 mouseWorldPosEnd = ScreenToWorld(input.mousePos + input.mouseDelta, screenInfo,
                                              gameState->cameraPos, gameState->cameraRot,
                                              ScaleExponentToWorldScale(gameState->editorScaleExponent),
                                              gameState->refPixelScreenHeight, gameState->refPixelsPerUnit,
                                              gameState->cameraOffsetFracY);
        Vec2 mouseWorldDelta = mouseWorldPosEnd - mouseWorldPosStart;
        
        if (editCollision) {
            bool newVertexPressed = false;
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
                Vec2Int mousePosPlusAnchor = input.mousePos + ANCHOR_OFFSET;
                for (uint64 i = 0; i < floor->line.size; i++) {
                    Vec2Int boxPos = WorldToScreen(floor->line[i], screenInfo,
                                                   gameState->cameraPos, gameState->cameraRot,
                                                   ScaleExponentToWorldScale(gameState->editorScaleExponent),
                                                   gameState->refPixelScreenHeight, gameState->refPixelsPerUnit,
                                                   gameState->cameraOffsetFracY);
                    
                    Vec4 boxColor = BOX_COLOR_BASE;
                    boxColor.a = IDLE_ALPHA;
                    if ((mousePosPlusAnchor.x >= boxPos.x
                         && mousePosPlusAnchor.x <= boxPos.x + BOX_SIZE.x) &&
                        (mousePosPlusAnchor.y >= boxPos.y
                         && mousePosPlusAnchor.y <= boxPos.y + BOX_SIZE.y)) {
                        if (input.mouseButtons[0].isDown
                            && input.mouseButtons[0].transitions == 1) {
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
            
            if (input.mouseButtons[0].isDown && input.mouseButtons[0].transitions == 1
                && !newVertexPressed) {
                gameState->floorVertexSelected = -1;
            }
            
            if (input.mouseButtons[0].isDown) {
                if (gameState->floorVertexSelected == -1) {
                    gameState->cameraPos -= mouseWorldDelta;
                }
                else {
                    floor->line[gameState->floorVertexSelected] += mouseWorldDelta;
                    floor->PrecomputeSampleVerticesFromLine();
                }
            }
            
            if (gameState->floorVertexSelected != -1) {
                if (WasKeyPressed(input, KM_KEY_R)) {
                    floor->line.Remove(gameState->floorVertexSelected);
                    floor->PrecomputeSampleVerticesFromLine();
                    gameState->floorVertexSelected = -1;
                }
            }
            
            if (input.mouseButtons[1].isDown && input.mouseButtons[1].transitions == 1) {
                if (gameState->floorVertexSelected == -1) {
                    floor->line.Append(mouseWorldPosEnd);
                    gameState->floorVertexSelected = (int)(floor->line.size - 1);
                }
                else {
                    floor->line.AppendAfter(mouseWorldPosEnd, gameState->floorVertexSelected);
                    gameState->floorVertexSelected += 1;
                }
                floor->PrecomputeSampleVerticesFromLine();
            }
        }
        else {
            if (input.mouseButtons[0].isDown) {
                gameState->cameraPos -= mouseWorldDelta;
            }
        }
        
        if (input.mouseButtons[2].isDown) {
            Vec2Int centerToMousePrev = (input.mousePos - input.mouseDelta) - screenInfo.size / 2;
            Vec2Int centerToMouse = input.mousePos - screenInfo.size / 2;
            float32 angle = AngleBetween(ToVec2(centerToMousePrev), ToVec2(centerToMouse));
            gameState->cameraRot = QuatFromAngleUnitAxis(-angle, Vec3::unitZ) * gameState->cameraRot;
        }
        
        float32 editorScaleExponentDelta = input.mouseWheelDelta * 0.0002f;
        gameState->editorScaleExponent = ClampFloat32(
                                                      gameState->editorScaleExponent + editorScaleExponentDelta, 0.0f, 1.0f);
    }
    
#if GAME_SLOW
    // Catch-all site for OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        LOG_ERROR("OpenGL error: 0x%x\n", err);
    }
#endif
}

#include "alphabet.cpp"
#include "asset.cpp"
#include "asset_animation.cpp"
#include "asset_audio.cpp"
#include "asset_level.cpp"
#include "asset_texture.cpp"
#include "audio.cpp"
#include "collision.cpp"
#include "framebuffer.cpp"
#include "imgui.cpp"
#include "load_psd.cpp"
#include "opengl_base.cpp"
#include "particles.cpp"
#include "post.cpp"
#include "render.cpp"
#include "text.cpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>
#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>
#undef STB_SPRINTF_IMPLEMENTATION

#include <km_common/km_input.cpp>
#include <km_common/km_kmkv.cpp>
#include <km_common/km_lib.cpp>
#include <km_common/km_log.cpp>
#include <km_common/km_memory.cpp>
#include <km_common/km_os.cpp>
#include <km_common/km_string.cpp>

#if GAME_WIN32
#include <km_platform/win32_main.cpp>
#include <km_platform/win32_audio.cpp>
// TODO else other platforms...
#endif
