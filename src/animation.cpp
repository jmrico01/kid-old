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

bool32 KeywordCompare(FixedArray<char, KEYWORD_MAX_LENGTH> keyword, const char* refKeyword)
{
    return StringCompare(keyword.array.data, refKeyword,
        MaxInt((int)keyword.array.size, StringLength(refKeyword)));
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

    Array<char> fileString;
    fileString.size = animFile.size;
    fileString.data = (char*)animFile.data;
	const char* fileData = (const char*)animFile.data;
	HashKey currentAnimKey = {};
	Animation* currentAnim = nullptr;
	while (true) {
        FixedArray<char, KEYWORD_MAX_LENGTH> keyword;
        FixedArray<char, VALUE_MAX_LENGTH> value;
        int read = ReadNextKeywordValue(fileString, &keyword, &value);
        if (read < 0) {
            DEBUG_PRINT("Animation file keyword/value error (%s)\n", filePath);
            return false;
        }
        else if (read == 0) {
            break;
        }
        fileString.size -= read;
        fileString.data += read;
        //const char* value = valueArray.data;
        //int valueI = (int)valueArray.size;

		// TODO catch errors in order of keywords (e.g. dir should be first after anim)
		if (KeywordCompare(keyword, KEYWORD_ANIM)) {
			currentAnimKey.WriteString(value.array);
			outAnimatedSprite.animations.Add(currentAnimKey, {});
			currentAnim = outAnimatedSprite.animations.GetValue(currentAnimKey);

			currentAnim->numFrames = 0;
			currentAnim->loop = false;
            currentAnim->rootMotion = false;
			currentAnim->rootFollow = false;
			currentAnim->rootFollowEndLoop = false;
		}
		else if (KeywordCompare(keyword, KEYWORD_DIR)) {
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
				int written = sprintf(&spritePath[lastSlash + 1], "%.*s/%d.png",
					(int)value.array.size, value.array.data, frame);
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
		else if (KeywordCompare(keyword, KEYWORD_FPS)) {
			int fps;
			if (!StringToIntBase10(value.array, &fps)) {
				DEBUG_PRINT("Animation file fps parse failed (%s)\n", filePath);
				return false;
			}
			if (fps < 0 && fps != -1) {
				DEBUG_PRINT("Animation file invalid fps %d (%s)\n", fps, filePath);
				return false;
			}
			currentAnim->fps = fps;
		}
		else if (KeywordCompare(keyword, KEYWORD_LOOP)) {
			currentAnim->loop = true;
		}
		else if (KeywordCompare(keyword, KEYWORD_EXIT)) {
			Array<char> elementString = value.array;
			//const char* element = value.array.data;
			//int length = (int)value.array.size;
			int elementLength;
			const char* next;

			if (!ReadElementInSplitString(elementString.data, (int)elementString.size,
			' ', &elementLength, &next)) {
				DEBUG_PRINT("Animation file missing exit-from frame (%s)\n", filePath);
				return false;
			}
			int exitFromFrame;
			if (*elementString.data == '*') {
				// wildcard
				exitFromFrame = -1;
			}
			else {
				Array<char> element = elementString;
				element.size = elementLength;
				if (!StringToIntBase10(element, &exitFromFrame)) {
					DEBUG_PRINT("Animation file invalid exit-from frame (%s)\n", filePath);
					return false;
				}
			}

			elementString.size -= next - elementString.data;
			elementString.data = (char*)next;
			if (!ReadElementInSplitString(elementString.data, (int)elementString.size,
			' ', &elementLength, &next)) {
				DEBUG_PRINT("Animation file missing exit-to animation (%s)\n", filePath);
				return false;
			}
			HashKey exitToAnim;
			exitToAnim.WriteString(elementString.data, elementLength);

			elementString.size -= next - elementString.data;
			elementString.data = (char*)next;
			if (!ReadElementInSplitString(elementString.data, (int)elementString.size,
			'\n', &elementLength, &next)) {
				DEBUG_PRINT("Animation file missing exit-to frame (%s)\n", filePath);
				return false;
			}
			int exitToFrame;
			{
				Array<char> element = elementString;
				element.size = elementLength;
				if (!StringToIntBase10(element, &exitToFrame)) {
					DEBUG_PRINT("Animation file invalid exit-to frame (%s)\n", filePath);
					return false;
				}
			}

			if (exitFromFrame == -1) {
				for (int i = 0; i < currentAnim->numFrames; i++) {
					currentAnim->frameExitTo[i].Add(exitToAnim, exitToFrame);
				}
			}
			else {
				currentAnim->frameExitTo[exitFromFrame].Add(exitToAnim, exitToFrame);
			}
		}
		else if (KeywordCompare(keyword, KEYWORD_TIMING)) {
            int parsedElements;
            if (!StringToElementArray(value.array, ' ', false,
                StringToIntBase10,
                ANIMATION_MAX_FRAMES, currentAnim->frameTiming, &parsedElements)) {
                DEBUG_PRINT("Failed to parse timing information (%s)\n", filePath);
                return false;
            }
            if (parsedElements != currentAnim->numFrames) {
                DEBUG_PRINT("Not enough timing information (%s)\n", filePath);
            }
		}
		else if (KeywordCompare(keyword, KEYWORD_ROOTFOLLOW)) {
			currentAnim->rootFollow = true;
		}
		else if (KeywordCompare(keyword, KEYWORD_ROOTFOLLOWENDLOOP)) {
			currentAnim->rootFollowEndLoop = true;
		}
		else if (KeywordCompare(keyword, KEYWORD_ROOTMOTION)) {
            currentAnim->rootMotion = true;

			Vec2Int textureSize = outAnimatedSprite.textureSize;
			Vec2 rootPosWorld0 = Vec2::zero;

			const char* element = value.array.data;
			int length = (int)value.array.size;
			for (int i = 0; i < currentAnim->numFrames; i++) {
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
                Array<char> trimmedString;
                trimmedString.data = (char*)trimmed;
                trimmedString.size = trimmedLength;
                if (!StringToElementArray(trimmedString, ' ', false,
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
				currentAnim->frameRootAnchor[i] = {
					(float32)rootPos.x / textureSize.x,
					(float32)rootPos.y / textureSize.y
				};

				if (i == 0) {
					rootPosWorld0 = rootPosWorld;
				}
				currentAnim->frameRootMotion[i] = rootPosWorld - rootPosWorld0;

				length -= (int)(next - element);
				element = next;
			}
		}
		else if (KeywordCompare(keyword, KEYWORD_START)) {
			HashKey startAnim;
			startAnim.WriteString(value.array);
			outAnimatedSprite.startAnimation = startAnim;
		}
		else if (KeywordCompare(keyword, KEYWORD_COMMENT)) {
			// Comment, ignore
		}
		else {
			DEBUG_PRINT("Animation file with unknown keyword: %.*s (%s)\n",
                keyword.array.size, keyword, filePath);
			return false;
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
