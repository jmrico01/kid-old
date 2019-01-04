#include "animation.h"

#include <cstdio>

#include "km_debug.h"
#include "km_string.h"

#define KEYWORD_MAX_LENGTH 8
#define VALUE_MAX_LENGTH 64
// TODO plz standardize
#define PATH_MAX_LENGTH 128

bool32 Animation::IsIdleFrame(int frame)
{
	bool32 isIdle = false;
	for (int j = 0; j < numIdles; j++) {
		if (frame == idles[j]) {
			isIdle = true;
			break;
		}
	}
	return isIdle;
}

void AnimatedSprite::Update(float32 deltaTime, bool32 moving)
{
	Animation activeAnim = animations[activeAnimation];
	if (!moving && activeAnim.IsIdleFrame(activeFrame)) {
		return;
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

void AnimatedSprite::Draw(TexturedRectGL texturedRectGL, ScreenInfo screenInfo,
	Vec2Int pos, Vec2 anchor, Vec2Int size, bool32 flipHorizontal) const
{
	DrawTexturedRect(texturedRectGL, screenInfo,
		pos, anchor, size, flipHorizontal,
		animations[activeAnimation].frameTextures[activeFrame].textureID
	);
}

bool32 LoadAnimatedSprite(const ThreadContext* thread, const char* filePath,
	AnimatedSprite& outAnimatedSprite,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	// TODO check exit frame for out of bounds
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

		if (StringCompare(keyword, "anim", 4)) {
			animIndex++;
			if (animIndex >= SPRITE_MAX_ANIMATIONS) {
				DEBUG_PRINT("Too many animations in file (%s)\n", filePath);
				return false;
			}
		}
		else if (StringCompare(keyword, "dir", 3)) {
			outAnimatedSprite.animations[animIndex].numFrames = 0;
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
			for (int i = 0; i < lastSlash; i++) {
				spritePath[i] = filePath[i];
			}
			spritePath[lastSlash] = '/';
			while (true) {
				value[valueI] = '\0'; // TODO dangerous but ugh
				int written = sprintf(&spritePath[lastSlash + 1], "%s/%d.png", value, frame);
				if (written <= 0) {
					DEBUG_PRINT("Failed to build animation sprite path for %s\n", filePath);
					return false;
				}
				spritePath[lastSlash + 1 + written] = '\0';

				TextureGL frameTextureGL;
				// TODO probably have a DoesFileExist function?
				bool32 frameResult = LoadPNGOpenGL(thread, spritePath, frameTextureGL,
					DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);
				if (!frameResult) {
					break;
				}
				outAnimatedSprite.animations[animIndex].frameTextures[frame] = frameTextureGL;
				outAnimatedSprite.animations[animIndex].numFrames++;

				if (animIndex == 0 && frame == 0) {
					outAnimatedSprite.textureSize = frameTextureGL.size;
				}
				else if (outAnimatedSprite.textureSize != frameTextureGL.size) {
					DEBUG_PRINT("Animation sprite size mismatch for frame %s\n",
						spritePath);
				}

				frame++;
			}
		}
		else if (StringCompare(keyword, "fps", 3)) {
			bool32 result = StringToIntBase10(value, valueI,
				outAnimatedSprite.animations[animIndex].fps);
			if (!result) {
				DEBUG_PRINT("Animation file fps parse failed (%s)\n", filePath);
				return false;
			}
		}
		else if (StringCompare(keyword, "exit", 4)) {
			// TODO support no-exit for single-animation sprites
			outAnimatedSprite.animations[animIndex].numIdles = 0;
			int numStart = 0;
			for (int i = 0; i < valueI; i++) {
				if (value[i] == ' ' || i == valueI - 1) {
					int numLength = i - numStart;
					if (i == valueI - 1) {
						numLength++;
					}
					int exitFrame;
					bool32 result = StringToIntBase10(&value[numStart], numLength, exitFrame);
					if (!result) {
						DEBUG_PRINT("Animation file exit parse failed (%s)\n", filePath);
						return false;
					}
					numStart = i + 1;

					int numIdles = outAnimatedSprite.animations[animIndex].numIdles;
					outAnimatedSprite.animations[animIndex].idles[numIdles] = exitFrame;
					outAnimatedSprite.animations[animIndex].numIdles++;
				}
			}
		}
		else {
			DEBUG_PRINT("Animation file with unknown keyword (%s)\n", filePath);
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

	return true;
}
