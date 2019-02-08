#include "animation.h"

#include "km_debug.h"
#include "km_string.h"
#include "main.h"

#define KEYWORD_MAX_LENGTH 16
#define VALUE_MAX_LENGTH 256
// TODO plz standardize file paths
#define PATH_MAX_LENGTH 128

enum SplitStringCode
{
	SPLIT_STRING_SUCCESS,
	SPLIT_STRING_END
};

static bool32 ReadElementInSplitString(const char* string, int stringLength, char separator,
	int* elementLength, const char** next)
{
	for (int i = 0; i < stringLength; i++) {
		if (string[i] == separator) {
			*elementLength = i;
			*next = &string[i + 1];
			return true;
		}
	}

	*elementLength = stringLength;
	return true;
}

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

		char valueBuffer[VALUE_MAX_LENGTH];
		int valueLength = 0;
		bool bracketValue = false;
		while (i < animFile.size &&
		((!bracketValue && fileData[i] != '\n' && fileData[i] != '\r')
		|| (bracketValue && fileData[i] != '}'))) {
			if (valueLength == 0 && fileData[i] == '{') {
				bracketValue = true;
				i++;
				continue;
			}
			if (valueLength >= VALUE_MAX_LENGTH) {
				DEBUG_PRINT("Animation file value too long (%s)\n", filePath);
				return false;
			}
			valueBuffer[valueLength++] = fileData[i++];
		}

		const char* value;
		int valueI;
		TrimWhitespace(valueBuffer, valueLength, &value, &valueI);
		DEBUG_PRINT("keyword: %.*s\nvalue: %.*s\n", keywordI, keyword, valueI, value);

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
				int written = sprintf(&spritePath[lastSlash + 1], "%.*s/%d.png",
					valueI, value, frame);
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
				currentAnim->frameRootAnchor[frame] = Vec2::zero;
				currentAnim->frameRootMotion[frame] = Vec2::zero;
				currentAnim->numFrames++;

				if (outAnimatedSprite.animations.size == 1 && frame == 0) {
					outAnimatedSprite.textureSize = frameTextureGL.size;
				}
				else if (outAnimatedSprite.textureSize != frameTextureGL.size) {
					DEBUG_PRINT("Animation sprite size mismatch for frame %s\n",
						spritePath);
					return false;
				}

				frame++;
			}

			if (currentAnim->numFrames == 0) {
				DEBUG_PRINT("Animation with no frames (%s)\n", filePath);
				return false;
			}
		}
		else if (StringCompare(keyword, "fps", 3)) {
			int fps;
			if (!StringToIntBase10(value, valueI, &fps)) {
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
			const char* element = value;
			int length = valueI;
			int elementLength;
			const char* next;

			if (!ReadElementInSplitString(element, length, ' ', &elementLength, &next)) {
				DEBUG_PRINT("Animation file missing exit-from frame (%s)\n", filePath);
				return false;
			}
			int exitFromFrame;
			if (*element == '*') {
				// wildcard
				exitFromFrame = -1;
			}
			else {
				if (!StringToIntBase10(element, elementLength, &exitFromFrame)) {
					DEBUG_PRINT("Animation file invalid exit-from frame (%s)\n", filePath);
					return false;
				}
			}

			length -= (int)(next - element);
			element = next;
			if (!ReadElementInSplitString(element, length, ' ', &elementLength, &next)) {
				DEBUG_PRINT("Animation file missing exit-to animation (%s)\n", filePath);
				return false;
			}
			HashKey exitToAnim;
			exitToAnim.WriteString(element, elementLength);

			length -= (int)(next - element);
			element = next;
			if (!ReadElementInSplitString(element, length, '\n', &elementLength, &next)) {
				DEBUG_PRINT("Animation file missing exit-to frame (%s)\n", filePath);
				return false;
			}
			int exitToFrame;
			if (!StringToIntBase10(element, elementLength, &exitToFrame)) {
				DEBUG_PRINT("Animation file invalid exit-to frame (%s)\n", filePath);
				return false;
			}

			if (exitFromFrame == -1) {
				for (int j = 0; j < currentAnim->numFrames; j++) {
					currentAnim->frameExitTo[j].Add(exitToAnim, exitToFrame);
				}
			}
			else {
				currentAnim->frameExitTo[exitFromFrame].Add(exitToAnim, exitToFrame);
			}
		}
		else if (StringCompare(keyword, "timing", 6)) {
			const char* element = value;
			int length = valueI;
			int frameTiming;
			for (int j = 0; j < currentAnim->numFrames; j++) {
				int elementLength;
				const char* next;
				if (!ReadElementInSplitString(element, length, ' ', &elementLength, &next)) {
					DEBUG_PRINT("Animation file missing timing information (%s)\n", filePath);
					return false;
				}
				if (!StringToIntBase10(element, elementLength, &frameTiming)) {
					DEBUG_PRINT("Animation file invalid timing value (%s)\n", filePath);
					return false;
				}
				currentAnim->frameTiming[j] = frameTiming;
				length -= (int)(next - element);
				element = next;
			}
		}
		else if (StringCompare(keyword, "rootmotion", 10)) {
			Vec2Int textureSize = outAnimatedSprite.textureSize;
			Vec2 rootPosWorld0 = Vec2::zero;

			const char* element = value;
			int length = valueI;
			for (int j = 0; j < currentAnim->numFrames; j++) {
				// Read root motion coordinate pair
				int elementLength;
				const char* next;
				if (!ReadElementInSplitString(element, length, '\n', &elementLength, &next)) {
					DEBUG_PRINT("Animation file missing root motion entry (%s)\n", filePath);
					return false;
				}
				DEBUG_PRINT("element: %.*s (%d)\n", elementLength, element, elementLength);

				// Parse root motion coordinate pair
				Vec2Int rootPos;
				const char* trimmed;
				int trimmedLength;
				TrimWhitespace(element, elementLength, &trimmed, &trimmedLength);
				DEBUG_PRINT("trimmed: %.*s (%d)\n", trimmedLength, trimmed, trimmedLength);

				int trimmedElementLength;
				const char* trimmedNext;
				if (!ReadElementInSplitString(trimmed, trimmedLength, ' ',
				&trimmedElementLength, &trimmedNext)) {
					DEBUG_PRINT("Animation file missing root motion 1st coordinate (%s)\n", filePath);
					return false;
				}
				if (!StringToIntBase10(trimmed, trimmedElementLength, &rootPos.x)) {
					DEBUG_PRINT("Animation file invalid root motion 1st coordinate (%s)\n", filePath);
					return false;
				}

				trimmedLength -= (int)(trimmedNext - trimmed);
				trimmed = trimmedNext;
				if (!ReadElementInSplitString(trimmed, trimmedLength, ' ',
				&trimmedElementLength, &trimmedNext)) {
					DEBUG_PRINT("Animation file missing root motion 2nd coordinate (%s)\n", filePath);
					return false;
				}
				if (!StringToIntBase10(trimmed, trimmedElementLength, &rootPos.y)) {
					DEBUG_PRINT("Animation file invalid root motion 2nd coordinate (%s)\n", filePath);
					return false;
				}

				rootPos.y = textureSize.y - rootPos.y;
				Vec2 rootPosWorld = {
					(float32)rootPos.x / REF_PIXELS_PER_UNIT,
					(float32)rootPos.y / REF_PIXELS_PER_UNIT
				};
				currentAnim->frameRootAnchor[j] = {
					(float32)rootPos.x / textureSize.x,
					(float32)rootPos.y / textureSize.y
				};
				if (i == 0) {
					rootPosWorld0 = rootPosWorld;
				}
				currentAnim->frameRootMotion[j] = rootPosWorld - rootPosWorld0;

				length -= (int)(next - element);
				element = next;
			}
		}
		else if (StringCompare(keyword, "//", 2)) {
			// Comment, ignore
		}
		else {
			DEBUG_PRINT("Animation file with unknown keyword (%s)\n", filePath);
			return false;
		}

		// Gobble newline and '}' characters
		while (i < animFile.size &&
		(fileData[i] == '}' || fileData[i] == '\n' || fileData[i] == '\r')) {
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
