#include "animation.h"

#include <km_common/km_debug.h>
#include <km_common/km_kmkv.h>
#include <km_common/km_memory.h>

#include "main.h"

global_var const uint64 KEYWORD_MAX_LENGTH = 32;
global_var const uint64 VALUE_MAX_LENGTH = 4096;

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

Vec2 AnimatedSpriteInstance::Update(float32 deltaTime, const Array<HashKey>& nextAnimations)
{
	const Animation* activeAnim = animatedSprite->animations.GetValue(activeAnimation);
	Vec2 rootMotion = Vec2::zero;

	activeFrameTime += deltaTime;
	if (activeFrameTime > activeAnim->frameTime[activeFrame]) {
		activeFrameTime = 0.0f;

		bool32 animTransition = false;
		for (uint64 i = 0; i < nextAnimations.size; i++) {
			if (KeyCompare(activeAnimation, nextAnimations[i])) {
				break;
			}
			
			const int* exitToFrame = activeAnim->frameExitTo[activeFrame].GetValue(nextAnimations[i]);
			if (exitToFrame != nullptr) {
				//Vec2 rootMotionPrev = activeAnim->frameRootMotion[activeFrame];
				animTransition = true;
				activeAnimation = nextAnimations[i];
				activeFrame = *exitToFrame;

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

	return rootMotion;
}

void AnimatedSpriteInstance::Draw(SpriteDataGL* spriteDataGL,
	Vec2 pos, Vec2 size, Vec2 anchor, Quat rot, float32 alpha, bool32 flipHorizontal) const
{
	const Animation* activeAnim = animatedSprite->animations.GetValue(activeAnimation);
	Vec2 animAnchor = anchor;
	if (activeAnim->rootMotion) {
		animAnchor = activeAnim->frameRootAnchor[activeFrame];
	}
	Mat4 transform = CalculateTransform(pos, size, animAnchor, rot, flipHorizontal);
	PushSprite(spriteDataGL, transform, alpha, activeAnim->frameTextures[activeFrame].textureID);
}

bool AnimatedSprite::Load(const ThreadContext* thread, const char* name, const MemoryBlock& transient)
{
	LinearAllocator allocator(transient.size, transient.memory);

	PsdFile psdFile;
	char filePath[PATH_MAX_LENGTH];
	stbsp_snprintf(filePath, PATH_MAX_LENGTH, "data/animations/%s/%s.psd", name, name);
	if (!OpenPSD(thread, &allocator, filePath, &psdFile)) {
		LOG_ERROR("Failed to open and parse level PSD file %s\n", filePath);
		return false;
	}

	textureSize = psdFile.size;

	stbsp_snprintf(filePath, PATH_MAX_LENGTH, "data/animations/%s/%s.kmkv", name, name);
	PlatformReadFileResult animFile = PlatformReadFile(thread, &allocator, filePath);
	if (!animFile.data) {
		LOG_ERROR("Failed to open animation file at: %s\n", filePath);
		return false;
	}

	Array<char> fileString;
	fileString.size = animFile.size;
	fileString.data = (char*)animFile.data;
	FixedArray<char, KEYWORD_MAX_LENGTH> keyword;
	DynamicArray<char> value;
	HashKey currentAnimKey;
	Animation* currentAnim = nullptr;
	while (true) {
		int read = ReadNextKeywordValue(fileString, &keyword, &value);
		if (read < 0) {
			LOG_ERROR("Animation file keyword/value error for %s\n", name);
			return false;
		}
		else if (read == 0) {
			break;
		}
		fileString.size -= read;
		fileString.data += read;

		// TODO catch error in keyword order (e.g. anim should always be first)
		if (StringEquals(keyword.ToArray(), ToString("anim"))) {
			currentAnimKey.WriteString(value.ToArray());
			animations.Add(currentAnimKey, {});
			currentAnim = animations.GetValue(currentAnimKey);

			currentAnim->numFrames = 0;
			currentAnim->loop = false;
			currentAnim->rootMotion = false;
			currentAnim->rootFollow = false;
			currentAnim->rootFollowEndLoop = false;

			uint64 frame = 0;
			float64 lastFrameStart = -1.0f;
			while (true) {
				uint64 nextLayerIndex = psdFile.layers.size;
				float64 earliestFrameStart = 1e6;
				for (uint64 i = 0; i < psdFile.layers.size; i++) {
					if (!psdFile.layers[i].inTimeline) {
						continue;
					}
					if (psdFile.layers[i].parentIndex == psdFile.layers.size) {
						continue;
					}
					uint64 parentIndex = psdFile.layers[i].parentIndex;
					if (!StringEquals(psdFile.layers[parentIndex].name.ToArray(), value.ToArray())) {
						continue;
					}
					float64 start = psdFile.layers[i].timelineStart;
					if (start < earliestFrameStart && start > lastFrameStart) {
						earliestFrameStart = start;
						nextLayerIndex = i;
					}
				}
				if (nextLayerIndex == psdFile.layers.size) {
					break;
				}

				const PsdLayerInfo& frameLayer = psdFile.layers[nextLayerIndex];
				TextureGL frameTextureGL;
				if (!psdFile.LoadLayerAtPsdSizeTextureGL(nextLayerIndex, LayerChannelID::ALL,
				GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &allocator,
				&frameTextureGL)) {
					LOG_ERROR("Failed to load animation texture GL for %s\n", name);
					return false;
				}
				currentAnim->frameTextures[frame] = frameTextureGL;
				currentAnim->frameTime[frame] = frameLayer.timelineDuration;
				currentAnim->frameRootAnchor[frame] = Vec2::zero;
				currentAnim->frameRootMotion[frame] = Vec2::zero;
				currentAnim->numFrames++;

				frame++;
				lastFrameStart = earliestFrameStart;
			}

			if (currentAnim->numFrames == 0) {
				LOG_ERROR("Animation with no frames (%s)\n", filePath);
				return false;
			}
			else if (currentAnim->numFrames == 1) {
				currentAnim->frameTime[0] = 0.0f;
			}
		}
		else if (StringEquals(keyword.ToArray(), ToString("fps"))) { // TODO won't need this anymore
			int fps;
			if (!StringToIntBase10(value.ToArray(), &fps)) {
				LOG_ERROR("Animation file fps parse failed (%s)\n", filePath);
				return false;
			}
			if (fps < 0 && fps != -1) {
				LOG_ERROR("Animation file invalid fps %d (%s)\n", fps, filePath);
				return false;
			}
			currentAnim->fps = fps;
		}
		else if (StringEquals(keyword.ToArray(), ToString("loop"))) {
			currentAnim->loop = true;
		}
		else if (StringEquals(keyword.ToArray(), ToString("exit"))) {
			if (value.size == 0) {
				LOG_ERROR("Animation file missing exit information (%s)\n", filePath);
				return false;
			}

			Array<char> element = value.ToArray();
			Array<char> next;
			ReadElementInSplitString(&element, &next, ' ');
			int exitFromFrame;
			if (*(element.data) == '*') {
				// wildcard
				exitFromFrame = -1;
			}
			else {
				if (!StringToIntBase10(element, &exitFromFrame)) {
					LOG_ERROR("Animation file invalid exit-from frame (%s)\n", filePath);
					return false;
				}
			}

			if (next.size == 0) {
				LOG_ERROR("Animation file missing exit-to animation (%s)\n", filePath);
				return false;
			}
			element = next;
			ReadElementInSplitString(&element, &next, ' ');
			HashKey exitToAnim;
			exitToAnim.WriteString(element);

			if (next.size == 0) {
				LOG_ERROR("Animation file missing exit-to frame (%s)\n", filePath);
				return false;
			}
			element = next;
			ReadElementInSplitString(&element, &next, '\n');
			int exitToFrame;
			{
				if (!StringToIntBase10(element, &exitToFrame)) {
					LOG_ERROR("Animation file invalid exit-to frame (%s)\n", filePath);
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
		else if (StringEquals(keyword.ToArray(), ToString("rootfollow"))) {
			currentAnim->rootFollow = true;
		}
		else if (StringEquals(keyword.ToArray(), ToString("rootfollowendloop"))) {
			currentAnim->rootFollowEndLoop = true;
		}
		else if (StringEquals(keyword.ToArray(), ToString("rootmotion"))) {
			currentAnim->rootMotion = true;

			Vec2 rootPosWorld0 = Vec2::zero;
			Array<char> element = value.ToArray();
			for (int i = 0; i < currentAnim->numFrames; i++) {
				// Read root motion coordinate pair
				Array<char> next;
				ReadElementInSplitString(&element, &next, '\n');

				Array<char> trimmed;
				TrimWhitespace(element, &trimmed);

				// Parse root motion coordinate pair
				Vec2Int rootPos;
				int parsedElements;
				if (!StringToElementArray(trimmed, ' ', false,
					StringToIntBase10, 2, rootPos.e, &parsedElements)) {
					LOG_ERROR("Failed to parse root motion coordinates %.*s (%s)\n",
						trimmed.size, trimmed.data, filePath);
					return false;
				}
				if (parsedElements != 2) {
					LOG_ERROR("Not enough coordinates in root motion %.*s (%s)\n",
						trimmed.size, trimmed.data, filePath);
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

				if (next.size == 0) {
					break;
				}
				element = next;
			}
		}
		else if (StringEquals(keyword.ToArray(), ToString("start"))) {
			HashKey startAnim;
			startAnim.WriteString(value.ToArray());
			startAnimation = startAnim;
		}
		else if (StringEquals(keyword.ToArray(), ToString("//"))) {
			// Comment, ignore
		}
		else {
			LOG_ERROR("Animation file with unknown keyword: %.*s (%s)\n",
				keyword.size, keyword.data, filePath);
			return false;
		}
	}

	// TODO maybe provide a friendlier way of iterating through HashTable
	for (uint32 k = 0; k < animations.capacity; k++) {
		const HashKey* animKey = &animations.pairs[k].key;
		if (animKey->string.size == 0) {
			continue;
		}

		const Animation* anim = &animations.pairs[k].value;
		for (int f = 0; f < anim->numFrames; f++) {
			const HashTable<int>* frameExitToTable = &anim->frameExitTo[f];
			for (uint32 j = 0; j < frameExitToTable->capacity; j++) {
				const HashKey* toAnimKey = &frameExitToTable->pairs[j].key;
				if (toAnimKey->string.size == 0) {
					continue;
				}

				const int* exitToFrame = frameExitToTable->GetValue(*toAnimKey);
				if (exitToFrame != nullptr && *exitToFrame >= 0) {
					const Animation* toAnim = animations.GetValue(*toAnimKey);
					if (toAnim == nullptr) {
						LOG_ERROR("Animation file non-existent exit-to animation %.*s (%s)\n",
							toAnimKey->string.size, toAnimKey->string.data, filePath);
						return false;
					}
					if (*exitToFrame >= toAnim->numFrames) {
						LOG_ERROR("Animation file exit-to frame out of bounds %d (%s)\n",
							*exitToFrame, filePath);
						return false;
					}
				}
			}
		}
	}

	return true;
}

void AnimatedSprite::Unload()
{
	for (uint32 k = 0; k < animations.capacity; k++) {
		if (animations.pairs[k].key.string.size == 0) {
			continue;
		}

		const Animation& animation = animations.pairs[k].value;
		for (int i = 0; i < animation.numFrames; i++) {
			UnloadTextureGL(animation.frameTextures[i]);
		}
	}
}
