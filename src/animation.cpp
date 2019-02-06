#include "animation.h"

#include "km_debug.h"
#include "km_string.h"
#include "main.h"

#define KEYWORD_MAX_LENGTH 16
#define VALUE_MAX_LENGTH 256
// TODO plz standardize
#define PATH_MAX_LENGTH 128

Vec2 AnimatedSprite::Update(float32 deltaTime,
	int numNextAnimations, const HashKey* nextAnimations)
{
	const Animation* activeAnim = animations.GetValue(activeAnimation);
	Vec2 rootMotion = Vec2::zero;

	for (int i = 0; i < numNextAnimations; i++) {
		if (KeyCompare(activeAnimation, nextAnimations[i])) {
			break;
		}
		
		int* exitToFrame = activeAnim->frameExitTo[activeFrame].GetValue(nextAnimations[i]);
		if (exitToFrame != nullptr) {
			activeAnimation = nextAnimations[i];
			activeFrame = *exitToFrame;
			activeFrameRepeat = 0;
		}
	}

	activeFrameTime += deltaTime;
	if (activeFrameTime > 1.0f / activeAnim->fps) {
		activeFrameTime = 0.0f;
		activeFrameRepeat++;
		if (activeFrameRepeat >= activeAnim->frameTiming[activeFrame]) {
			int activeFramePrev = activeFrame;
			activeFrame = (activeFrame + 1) % activeAnim->numFrames;
			activeFrameRepeat = 0;

			rootMotion = activeAnim->frameRootMotion[activeFrame]
				- activeAnim->frameRootMotion[activeFramePrev];
		}
	}

	return rootMotion;
}

void AnimatedSprite::Draw(SpriteDataGL* spriteDataGL,
		Vec2 pos, Vec2 size, Vec2 anchor, bool32 flipHorizontal) const
{
	Animation* activeAnim = animations.GetValue(activeAnimation);
	Vec2 anchorRootMotion = activeAnim->frameRootAnchor[activeFrame];
	PushSpriteWorldSpace(spriteDataGL, pos, size,
		anchorRootMotion, flipHorizontal,
		activeAnim->frameTextures[activeFrame].textureID);
}

bool32 LoadAnimatedSprite(const ThreadContext* thread, const char* filePath,
	AnimatedSprite& outAnimatedSprite,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	const int TIMING_HARDCODED[16] = {
		2, 1, 2, 1, 1, 1, 2, 2, 2, 2, 1, 2, 1, 2, 2, 2
	};
	const Vec2Int ROOTMOTION_HARDCODED[16] = {
		{ 101, 480 },
		{ 101, 480 },
		{ 101, 480 },
		{ 101, 403 },
		{ 101, 270 },
		{ 101, 217 },
		{ 101, 209 },
		{ 101, 206 },
		{ 101, 209 },
		{ 101, 217 },
		{ 101, 246 },
		{ 101, 340 },
		{ 101, 480 },
		{ 101, 480 },
		{ 101, 480 },
		{ 101, 480 },
	};

	DEBUGReadFileResult animFile = DEBUGPlatformReadFile(thread, filePath);
	if (!animFile.data) {
		DEBUG_PRINT("Failed to open animation file at: %s\n", filePath);
		return false;
	}

	outAnimatedSprite.animations.Init();

	const char* fileData = (const char*)animFile.data;
	bool32 done = false;
	int i = 0;
	HashKey currentAnimKey = {};
	Animation* currentAnim = nullptr;
	while (!done) {
		char keyword[KEYWORD_MAX_LENGTH];
		int keywordI = 0;
		while (i < animFile.size && fileData[i] != ' ') {
			if (keywordI >= KEYWORD_MAX_LENGTH) {
				DEBUG_PRINT("Animation file keyword too long (%s)\n", filePath);
				return false;
			}
			keyword[keywordI++] = fileData[i++];
		}
		if (i >= animFile.size) {
			DEBUG_PRINT("Animation file keyword without value (%s)\n", filePath);
			return false;
		}
		i++; // Skip space
		char value[VALUE_MAX_LENGTH];
		int valueI = 0;
		while (i < animFile.size
		&& fileData[i] != '\n' && fileData[i] != '\r' && fileData[i] != '\0') {
			if (valueI >= VALUE_MAX_LENGTH) {
				DEBUG_PRINT("Animation file value too long (%s)\n", filePath);
				return false;
			}
			value[valueI++] = fileData[i++];
		}

		// TODO catch errors in order of keywords (e.g. dir should be first after anim)
		if (StringCompare(keyword, "anim", 4)) {
			currentAnimKey.WriteString(value, valueI);
			outAnimatedSprite.animations.Add(currentAnimKey, {});
			currentAnim = outAnimatedSprite.animations.GetValue(currentAnimKey);
		}
		else if (StringCompare(keyword, "dir", 3)) {
			currentAnim->numFrames = 0;
			int frame = 0;
			int lastSlash = GetLastOccurrence(filePath, StringLength(filePath), '/');
			if (lastSlash == -1) {
				DEBUG_PRINT("Couldn't find slash in animation file path %s\n", filePath);
				return false;
			}
			if (lastSlash >= PATH_MAX_LENGTH) {
				DEBUG_PRINT("Animation file path too long %s\n", filePath);
				return false;
			}
			char spritePath[PATH_MAX_LENGTH];
			for (int j = 0; j < lastSlash; j++) {
				spritePath[j] = filePath[j];
			}
			spritePath[lastSlash] = '/';
			while (true) {
				value[valueI] = '\0'; // TODO dangerous but ugh
				int written = sprintf(&spritePath[lastSlash + 1], "%s/%d.png", value, frame);
				if (written <= 0) {
					DEBUG_PRINT("Failed to build animation sprite path for %s\n", filePath);
					return false;
				}
				if (lastSlash + written + 1 >= PATH_MAX_LENGTH) {
					DEBUG_PRINT("Sprite path too long in %s\n", filePath);
					return false;
				}
				spritePath[lastSlash + 1 + written] = '\0';

				TextureGL frameTextureGL;
				// TODO probably make a DoesFileExist function?
				bool32 frameResult = LoadPNGOpenGL(thread, spritePath, frameTextureGL,
					DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);
				if (!frameResult) {
					break;
				}
				currentAnim->frameTextures[frame] = frameTextureGL;
				currentAnim->frameTiming[frame] = 1;
				currentAnim->frameExitTo[frame].Init();
				/*for (int j = 0; j < SPRITE_MAX_ANIMATIONS; j++) {
					currentAnim->frameExitTo[frame][j] = -1;
				}*/
				currentAnim->numFrames++;

				if (outAnimatedSprite.animations.size == 1 && frame == 0) {
					outAnimatedSprite.textureSize = frameTextureGL.size;
				}
				else if (outAnimatedSprite.textureSize != frameTextureGL.size) {
					DEBUG_PRINT("Animation sprite size mismatch for frame %s\n",
						spritePath);
					return false;
				}

				// TODO hardcoded
				currentAnim->frameRootAnchor[frame] = {
					(float32)ROOTMOTION_HARDCODED[0].x / outAnimatedSprite.textureSize.x,
					(float32)(outAnimatedSprite.textureSize.y - ROOTMOTION_HARDCODED[0].y) / outAnimatedSprite.textureSize.y
				};

				frame++;
			}

			if (currentAnim->numFrames == 0) {
				DEBUG_PRINT("Animation with no frames (%s)\n", filePath);
				return false;
			}
		}
		else if (StringCompare(keyword, "fps", 3)) {
			int fps;
			bool32 result = StringToIntBase10(value, valueI, fps);
			if (!result) {
				DEBUG_PRINT("Animation file fps parse failed (%s)\n", filePath);
				return false;
			}
			if (fps < 0 && fps != -1) {
				DEBUG_PRINT("Animation file invalid fps %d (%s)\n", fps, filePath);
				return false;
			}
			currentAnim->fps = fps;
		}
		else if (StringCompare(keyword, "exit", 4)) {
			int spaceInd1 = -1;
			int spaceInd2 = -1;
			for (int j = 0; j < valueI; j++) {
				if (value[j] == ' ') {
					if (spaceInd1 == -1) {
						spaceInd1 = j;
					}
					else if (spaceInd2 == -1) {
						spaceInd2 = j;
						break;
					}
				}
			}
			if (spaceInd1 == -1 || spaceInd1 == spaceInd2 - 1
			|| spaceInd2 == -1 || spaceInd2 == valueI - 1) {
				DEBUG_PRINT("Animation file invalid exit value (%s)\n", filePath);
				return false;
			}
			int exitFromFrame, /*exitToAnim,*/ exitToFrame;
			bool32 result;
			if (spaceInd1 == 1 && value[0] == '*') {
				// wildcard
				exitFromFrame = -1;
			}
			else {
				result = StringToIntBase10(value, spaceInd1, exitFromFrame);
				if (!result) {
					DEBUG_PRINT("Animation file invalid exit-from frame (%s)\n", filePath);
					return false;
				}
			}
			/*result = StringToIntBase10(&value[spaceInd1 + 1],
				spaceInd2 - spaceInd1 - 1, exitToAnim);
			if (!result) {
				DEBUG_PRINT("Animation file invalid exit-to animation (%s)\n", filePath);
				return false;
			}*/
			result = StringToIntBase10(&value[spaceInd2 + 1],
				valueI - spaceInd2 - 1, exitToFrame);
			if (!result) {
				DEBUG_PRINT("Animation file invalid exit-to frame (%s)\n", filePath);
				return false;
			}
			HashKey exitToAnim;
			exitToAnim.WriteString(&value[spaceInd1 + 1], spaceInd2 - spaceInd1 - 1);
			DEBUG_PRINT("tried to write %.*s\nwrote %.*s\n",
				spaceInd2 - spaceInd1 - 1, &value[spaceInd1 + 1],
				exitToAnim.length, exitToAnim.string);
			if (exitFromFrame == -1) {
				for (int j = 0; j < currentAnim->numFrames; j++) {
					currentAnim->frameExitTo[j].Add(exitToAnim, exitToFrame);
					//currentAnim->frameExitTo[j][exitToAnim] = exitToFrame;
				}
			}
			else {
				//currentAnim->frameExitTo[exitFromFrame][exitToAnim] = exitToFrame;
				currentAnim->frameExitTo[exitFromFrame].Add(exitToAnim, exitToFrame);
			}
		}
		else if (StringCompare(keyword, "timing", 6)) {
			// TODO hardcoded
			DEBUG_ASSERT(currentAnim->numFrames == 16);
			for (int j = 0; j < currentAnim->numFrames; j++) {
				currentAnim->frameTiming[j] = TIMING_HARDCODED[j];
			}
		}
		else if (StringCompare(keyword, "rootmotion", 10)) {
			// TODO hardcoded
			Vec2Int textureSize = outAnimatedSprite.textureSize;
			Vec2Int rootMotion0 = ROOTMOTION_HARDCODED[0];
			rootMotion0.y = textureSize.y - rootMotion0.y;
			Vec2 rootPosWorld0 = {
				(float32)rootMotion0.x / REF_PIXELS_PER_UNIT,
				(float32)rootMotion0.y / REF_PIXELS_PER_UNIT
			};
			DEBUG_ASSERT(currentAnim->numFrames == 16);
			for (int j = 0; j < currentAnim->numFrames; j++) {
				Vec2Int rootPos = ROOTMOTION_HARDCODED[j];
				rootPos.y = textureSize.y - rootPos.y;
				Vec2 rootPosWorld = {
					(float32)rootPos.x / REF_PIXELS_PER_UNIT,
					(float32)rootPos.y / REF_PIXELS_PER_UNIT
				};
				currentAnim->frameRootMotion[j] = rootPosWorld - rootPosWorld0;
				currentAnim->frameRootAnchor[j] = {
					(float32)rootPos.x / textureSize.x,
					(float32)rootPos.y / textureSize.y
				};
			}
		}
		else if (StringCompare(keyword, "//", 2)) {
			// Comment, ignore
		}
		else {
			DEBUG_PRINT("Animation file with unknown keyword (%s)\n", filePath);
			return false;
		}
		// Gobble newline characters
		while (i < animFile.size && (fileData[i] == '\n' || fileData[i] == '\r')) {
			i++;
		}

		if (i >= animFile.size || *fileData == '\0') {
			done = true;
		}
	}

	DEBUGPlatformFreeFileMemory(thread, &animFile);

	outAnimatedSprite.activeAnimation = currentAnimKey;
	outAnimatedSprite.activeFrame = 0;
	outAnimatedSprite.activeFrameRepeat = 0;
	outAnimatedSprite.activeFrameTime = 0.0f;

	// TODO fix. add ability to iterate over all hashmap?
#if 0
	for (int a = 0; a < SPRITE_MAX_ANIMATIONS; a++) {
		Animation* animation = outAnimatedSprite.animations[a];
		for (int f = 0; f < animation.numFrames; f++) {
			for (int j = 0; j < SPRITE_MAX_ANIMATIONS; j++) {
				int exitToFrame = animation.frameExitTo[f][j];
				if (exitToFrame >= 0) {
					if (j >= outAnimatedSprite.numAnimations) {
						DEBUG_PRINT("Animation file exit-to animation out of bounds %d (%s)\n",
							j, filePath);
					}
					if (exitToFrame >= outAnimatedSprite.animations[j].numFrames) {
						DEBUG_PRINT("Animation file exit-to frame out of bounds %d (%s)\n",
							exitToFrame, filePath);
					}
				}
			}
		}
	}
#endif

	return true;
}
