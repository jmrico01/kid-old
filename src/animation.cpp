#include "animation.h"

#include "km_debug.h"
#include "km_string.h"
#include "main.h"

#define KEYWORD_MAX_LENGTH 32
#define VALUE_MAX_LENGTH 256
// TODO plz standardize file paths
#define PATH_MAX_LENGTH 128

const char KEYWORD_ANIM             [KEYWORD_MAX_LENGTH] = "anim";
const char KEYWORD_DIR              [KEYWORD_MAX_LENGTH] = "dir";
const char KEYWORD_FPS              [KEYWORD_MAX_LENGTH] = "fps";
const char KEYWORD_LOOP             [KEYWORD_MAX_LENGTH] = "loop";
const char KEYWORD_EXIT             [KEYWORD_MAX_LENGTH] = "exit";
const char KEYWORD_TIMING           [KEYWORD_MAX_LENGTH] = "timing";
const char KEYWORD_ROOTFOLLOW       [KEYWORD_MAX_LENGTH] = "rootfollow";
const char KEYWORD_ROOTFOLLOWENDLOOP[KEYWORD_MAX_LENGTH] = "rootfollowendloop";
const char KEYWORD_ROOTMOTION       [KEYWORD_MAX_LENGTH] = "rootmotion";

const char KEYWORD_START            [KEYWORD_MAX_LENGTH] = "start";
const char KEYWORD_COMMENT          [KEYWORD_MAX_LENGTH] = "//";

Vec2 AnimatedSpriteInstance::Update(float32 deltaTime,
	int numNextAnimations, const HashKey* nextAnimations)
{
	const Animation* activeAnim = animatedSprite->animations.GetValue(activeAnimation);
	Vec2 rootMotion = Vec2::zero;

	activeFrameTime += deltaTime;
	if (activeFrameTime > 1.0f / activeAnim->fps) {
		activeFrameTime = 0.0f;
		activeFrameRepeat++;
		if (activeFrameRepeat >= activeAnim->frameTiming[activeFrame]) {
			activeFrameRepeat = 0;

			bool32 animTransition = false;
			for (int i = 0; i < numNextAnimations; i++) {
				if (KeyCompare(activeAnimation, nextAnimations[i])) {
					break;
				}
				
				int* exitToFrame = activeAnim->frameExitTo[activeFrame].GetValue(nextAnimations[i]);
				if (exitToFrame != nullptr) {
					//Vec2 rootMotionPrev = activeAnim->frameRootMotion[activeFrame];
                    animTransition = true;
					activeAnimation = nextAnimations[i];
					activeFrame = *exitToFrame;
					activeFrameRepeat = 0;

					activeAnim = animatedSprite->animations.GetValue(activeAnimation);
					// TODO transitions between rootfollow-enabled animations don't work for now
					//rootMotion += (activeAnim->frameRootMotion[activeFrame] - rootMotionPrev);
				}
			}

			if (!animTransition) {
				int activeFrameNext = activeFrame + 1;
				if (activeFrameNext >= activeAnim->numFrames) {
					activeFrameNext = activeAnim->loop ? 0 : activeFrame;
				}

				if (activeAnim->rootFollow) {
					rootMotion += activeAnim->frameRootMotion[activeFrameNext]
						- activeAnim->frameRootMotion[activeFrame];

					if (!activeAnim->loop && activeAnim->rootFollowEndLoop
					&& activeFrame == activeAnim->numFrames - 1) {
						rootMotion += activeAnim->frameRootMotion[activeFrame]
							- activeAnim->frameRootMotion[activeFrame - 1];
					}
				}

				activeFrame = activeFrameNext;
			}
		}
	}

	return rootMotion;
}

void AnimatedSpriteInstance::Draw(SpriteDataGL* spriteDataGL,
	Vec2 pos, Vec2 size, Vec2 anchor, Quat rot, float32 alpha, bool32 flipHorizontal) const
{
	Animation* activeAnim = animatedSprite->animations.GetValue(activeAnimation);
    Vec2 animAnchor = anchor;
    if (activeAnim->rootMotion) {
        animAnchor = activeAnim->frameRootAnchor[activeFrame];
    }
    Mat4 transform = CalculateTransform(pos, size, animAnchor, rot);
	PushSprite(spriteDataGL, transform, alpha, flipHorizontal,
		activeAnim->frameTextures[activeFrame].textureID);
}

bool32 KeywordCompare(const char* keyword, int keywordLength, const char* refKeyword)
{
	return StringCompare(keyword, refKeyword, MaxInt(keywordLength, StringLength(refKeyword)));
}

bool32 LoadAnimatedSprite(const ThreadContext* thread, const char* filePath,
	AnimatedSprite& outAnimatedSprite, MemoryBlock transient,
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
		while (i < animFile.size && !IsWhitespace(fileData[i])) {
			if (keywordI >= KEYWORD_MAX_LENGTH) {
				DEBUG_PRINT("Animation file keyword too long (%s)\n", filePath);
				return false;
			}
			keyword[keywordI++] = fileData[i++];
		}

		if (i < animFile.size && fileData[i] == ' ') {
			i++; // Skip space
		}

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
		/* DEBUG_PRINT("keyword: %.*s (%d)\nvalue: %.*s (%d)\n",
			keywordI, keyword, keywordI, valueI, value, valueI); */

		// TODO catch errors in order of keywords (e.g. dir should be first after anim)
		if (KeywordCompare(keyword, keywordI, KEYWORD_ANIM)) {
			currentAnimKey.WriteString(value, valueI);
			outAnimatedSprite.animations.Add(currentAnimKey, {});
			currentAnim = outAnimatedSprite.animations.GetValue(currentAnimKey);

			currentAnim->numFrames = 0;
			currentAnim->loop = false;
            currentAnim->rootMotion = false;
			currentAnim->rootFollow = false;
			currentAnim->rootFollowEndLoop = false;
		}
		else if (KeywordCompare(keyword, keywordI, KEYWORD_DIR)) {
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
				bool32 frameResult = LoadPNGOpenGL(thread, spritePath,
                    GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                    frameTextureGL, transient,
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
		else if (KeywordCompare(keyword, keywordI, KEYWORD_FPS)) {
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
		else if (KeywordCompare(keyword, keywordI, KEYWORD_LOOP)) {
			currentAnim->loop = true;
		}
		else if (KeywordCompare(keyword, keywordI, KEYWORD_EXIT)) {
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
		else if (KeywordCompare(keyword, keywordI, KEYWORD_TIMING)) {
            int parsedElements;
            if (!StringToElementArray(value, valueI, ' ', false,
                StringToIntBase10,
                ANIMATION_MAX_FRAMES, currentAnim->frameTiming, &parsedElements)) {
                DEBUG_PRINT("Failed to parse timing information (%s)\n", filePath);
                return false;
            }
            if (parsedElements != currentAnim->numFrames) {
                DEBUG_PRINT("Not enough timing information (%s)\n", filePath);
            }
		}
		else if (KeywordCompare(keyword, keywordI, KEYWORD_ROOTFOLLOW)) {
			currentAnim->rootFollow = true;
		}
		else if (KeywordCompare(keyword, keywordI, KEYWORD_ROOTFOLLOWENDLOOP)) {
			currentAnim->rootFollowEndLoop = true;
		}
		else if (KeywordCompare(keyword, keywordI, KEYWORD_ROOTMOTION)) {
            currentAnim->rootMotion = true;

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

                const char* trimmed;
                int trimmedLength;
                TrimWhitespace(element, elementLength, &trimmed, &trimmedLength);

                // Parse root motion coordinate pair
                Vec2Int rootPos;
                int parsedElements;
                if (!StringToElementArray(trimmed, trimmedLength, ' ', false,
                    StringToIntBase10, 2, rootPos.e, &parsedElements)) {
                    DEBUG_PRINT("Failed to parse root motion coordinates %.*s (%s)\n",
                        trimmedLength, trimmed, filePath);
                }
                if (parsedElements != 2) {
                    DEBUG_PRINT("Not enough coordinates in root motion %.*s (%s)\n",
                        trimmedLength, trimmed, filePath);
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
		else if (KeywordCompare(keyword, keywordI, KEYWORD_START)) {
			HashKey startAnim;
			startAnim.WriteString(value, valueI);
			outAnimatedSprite.startAnimation = startAnim;
		}
		else if (KeywordCompare(keyword, keywordI, KEYWORD_COMMENT)) {
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

	// TODO maybe provide a friendlier way of iterating through HashTable
	const HashTable<Animation>* animTable = &outAnimatedSprite.animations;
	for (uint32 k = 0; k < animTable->capacity; k++) {
		const HashKey* animKey = &animTable->pairs[k].key;
		if (animKey->length == 0) {
			continue;
		}

		const Animation* anim = &animTable->pairs[k].value;
		for (int f = 0; f < anim->numFrames; f++) {
			const HashTable<int>* frameExitToTable = &anim->frameExitTo[f];
			for (uint32 j = 0; j < frameExitToTable->capacity; j++) {
				const HashKey* toAnimKey = &frameExitToTable->pairs[j].key;
				if (toAnimKey->length == 0) {
					continue;
				}

				int* exitToFrame = frameExitToTable->GetValue(*toAnimKey);
				if (exitToFrame != nullptr && *exitToFrame >= 0) {
					/* DEBUG_PRINT("Animation transition: %.*s (%d) -> %.*s (%d)\n",
						animKey->length, animKey->string, f,
						toAnimKey->length, toAnimKey->string, *exitToFrame); */

					const Animation* toAnim = animTable->GetValue(*toAnimKey);
					if (toAnim == nullptr) {
						DEBUG_PRINT("Animation file non-existent exit-to animation %.*s (%s)\n",
							toAnimKey->length, toAnimKey->string, filePath);
						return false;
					}
					if (*exitToFrame >= toAnim->numFrames) {
						DEBUG_PRINT("Animation file exit-to frame out of bounds %d (%s)\n",
							*exitToFrame, filePath);
						return false;
					}
				}
			}
		}
	}

	return true;
}
