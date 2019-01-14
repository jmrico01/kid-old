#include "animation.h"

//#include <cstdio>

#include "km_debug.h"
#include "km_string.h"

#define KEYWORD_MAX_LENGTH 8
#define VALUE_MAX_LENGTH 256
// TODO plz standardize
#define PATH_MAX_LENGTH 128

void AnimatedSprite::Update(float32 deltaTime, int numNextAnimations, const int nextAnimations[])
{
	Animation activeAnim = animations[activeAnimation];

	int nextAnim = -1;
	int nextAnimStartFrame = 0;
	for (int i = 0; i < numNextAnimations; i++) {
		int exitToFrame = activeAnim.frameExitTo[activeFrame][nextAnimations[i]];
		if (activeAnimation == nextAnimations[i]) {
			nextAnim = -1;
			break;
		}
		else if (exitToFrame >= 0) {
			nextAnim = nextAnimations[i];
			nextAnimStartFrame = exitToFrame;
		}
	}
	if (nextAnim != -1) {
		activeAnimation = nextAnim;
		activeAnim = animations[activeAnimation];
		activeFrame = nextAnimStartFrame;
	}

	activeFrameTime += deltaTime;
	if (activeFrameTime > 1.0f / activeAnim.fps) {
		activeFrameTime = 0.0f;
		activeFrame++;
		if (activeFrame >= activeAnim.numFrames) {
			activeFrame = 0;
		}
	}
}

void AnimatedSprite::Draw(SpriteDataGL* spriteDataGL,
		Vec2 pos, Vec2 size, Vec2 anchor, bool32 flipHorizontal) const
{
	PushSpriteWorldSpace(spriteDataGL, pos, size,
		anchor, flipHorizontal,
		animations[activeAnimation].frameTextures[activeFrame].textureID);
}

bool32 LoadAnimatedSprite(const ThreadContext* thread, const char* filePath,
	AnimatedSprite& outAnimatedSprite,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	DEBUGReadFileResult animFile = DEBUGPlatformReadFile(thread, filePath);
	if (!animFile.data) {
		DEBUG_PRINT("Failed to open animation file at: %s\n", filePath);
		return false;
	}

	const char* fileData = (const char*)animFile.data;
	bool32 done = false;
	int i = 0;
	int animIndex = -1;
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
			animIndex++;
			if (animIndex >= SPRITE_MAX_ANIMATIONS) {
				DEBUG_PRINT("Too many animations in file (%s)\n", filePath);
				return false;
			}
		}
		else if (StringCompare(keyword, "dir", 3)) {
			Animation& animation = outAnimatedSprite.animations[animIndex];
			animation.numFrames = 0;
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
				animation.frameTextures[frame] = frameTextureGL;
				for (int j = 0; j < SPRITE_MAX_ANIMATIONS; j++) {
					animation.frameExitTo[frame][j] = -1;
				}
				animation.numFrames++;

				if (animIndex == 0 && frame == 0) {
					outAnimatedSprite.textureSize = frameTextureGL.size;
				}
				else if (outAnimatedSprite.textureSize != frameTextureGL.size) {
					DEBUG_PRINT("Animation sprite size mismatch for frame %s\n",
						spritePath);
					return false;
				}

				frame++;
			}

			if (animation.numFrames == 0) {
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
			outAnimatedSprite.animations[animIndex].fps = fps;
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
			int exitFromFrame, exitToAnim, exitToFrame;
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
			result = StringToIntBase10(&value[spaceInd1 + 1],
				spaceInd2 - spaceInd1 - 1, exitToAnim);
			if (!result) {
				DEBUG_PRINT("Animation file invalid exit-to animation (%s)\n", filePath);
				return false;
			}
			result = StringToIntBase10(&value[spaceInd2 + 1],
				valueI - spaceInd2 - 1, exitToFrame);
			if (!result) {
				DEBUG_PRINT("Animation file invalid exit-to frame (%s)\n", filePath);
				return false;
			}
			Animation& anim = outAnimatedSprite.animations[animIndex];
			if (exitFromFrame == -1) {
				for (int j = 0; j < anim.numFrames; j++) {
					anim.frameExitTo[j][exitToAnim] = exitToFrame;
				}
			}
			else {
				anim.frameExitTo[exitFromFrame][exitToAnim] = exitToFrame;
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

	outAnimatedSprite.activeAnimation = 0;
	outAnimatedSprite.activeFrame = 0;
	outAnimatedSprite.activeFrameTime = 0.0f;

	outAnimatedSprite.numAnimations = animIndex + 1;

	for (int a = 0; a < SPRITE_MAX_ANIMATIONS; a++) {
		Animation& animation = outAnimatedSprite.animations[a];
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

	return true;
}
