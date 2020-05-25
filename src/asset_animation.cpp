#include "asset_animation.h"

#include <km_common/km_debug.h>
#include <km_common/km_kmkv.h>
#include <km_common/km_memory.h>
#include <km_common/km_os.h>

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

Vec2 UpdateAnimatedSprite(AnimatedSpriteInstance* sprite, const GameAssets& assets, float32 deltaTime,
                          const Array<HashKey>& nextAnimations)
{
    const AnimatedSprite* animatedSprite = GetAnimatedSprite(assets, sprite->animatedSpriteId);
    const Animation* activeAnimation = animatedSprite->animations.GetValue(sprite->activeAnimationKey);
    Vec2 rootMotion = Vec2::zero;

    sprite->activeFrameTime += deltaTime;
    if (sprite->activeFrameTime > activeAnimation->frameTime[sprite->activeFrame]) {
        sprite->activeFrameTime = 0.0f;

        bool animTransition = false;
        for (uint64 i = 0; i < nextAnimations.size; i++) {
            if (KeyCompare(sprite->activeAnimationKey, nextAnimations[i])) {
                break;
            }

            const int* exitToFrame = activeAnimation->frameExitTo[sprite->activeFrame].GetValue(nextAnimations[i]);
            if (exitToFrame != nullptr) {
                //Vec2 rootMotionPrev = activeAnimation->frameRootMotion[activeFrame];
                animTransition = true;
                sprite->activeAnimationKey = nextAnimations[i];
                sprite->activeFrame = *exitToFrame;

                activeAnimation = animatedSprite->animations.GetValue(sprite->activeAnimationKey);
                // TODO transitions between rootfollow-enabled animations don't work for now
                //rootMotion += (activeAnimation->frameRootMotion[activeFrame] - rootMotionPrev);
            }
        }

        if (!animTransition) {
            int activeFrameNext = sprite->activeFrame + 1;
            if (activeFrameNext >= activeAnimation->numFrames) {
                activeFrameNext = activeAnimation->loop ? 0 : sprite->activeFrame;
            }

            if (activeAnimation->rootFollow) {
                rootMotion += activeAnimation->frameRootMotion[activeFrameNext]
                    - activeAnimation->frameRootMotion[sprite->activeFrame];

                if (!activeAnimation->loop && activeAnimation->rootFollowEndLoop
                    && sprite->activeFrame == activeAnimation->numFrames - 1) {
                    rootMotion += activeAnimation->frameRootMotion[sprite->activeFrame]
                        - activeAnimation->frameRootMotion[sprite->activeFrame - 1];
                }
            }

            sprite->activeFrame = activeFrameNext;
        }
    }

    return rootMotion;
}

void DrawAnimatedSprite(const AnimatedSpriteInstance& sprite, const GameAssets& assets, SpriteDataGL* spriteDataGL,
                        Vec2 pos, Vec2 size, Vec2 anchor, Quat rot, float32 alpha, bool flipHorizontal)
{
    const AnimatedSprite* animatedSprite = GetAnimatedSprite(assets, sprite.animatedSpriteId);
    const Animation* activeAnimation = animatedSprite->animations.GetValue(sprite.activeAnimationKey);
    Vec2 animAnchor = anchor;
    if (activeAnimation->rootMotion) {
        animAnchor = activeAnimation->frameRootAnchor[sprite.activeFrame];
    }
    Mat4 transform = CalculateTransform(pos, size, animAnchor, rot, flipHorizontal);
    PushSprite(spriteDataGL, transform, alpha, activeAnimation->frameTextures[sprite.activeFrame].textureID);
}

bool LoadAnimatedSprite(AnimatedSprite* sprite, const_string name, float32 pixelsPerUnit, MemoryBlock transient)
{
    LinearAllocator allocator(transient.size, transient.memory);

    FixedArray<char, PATH_MAX_LENGTH> filePath;
    filePath.Clear();
    filePath.Append(ToString("data/psd/"));
    filePath.Append(name);
    filePath.Append(ToString(".psd"));
    PsdFile psdFile;
    if (!LoadPsd(&psdFile, filePath.ToConstArray(), &allocator)) {
        LOG_ERROR("Failed to open and parse level PSD file %.*s\n", filePath.size, filePath.data);
        return false;
    }

    sprite->textureSize = psdFile.size;

    filePath.Clear();
    filePath.Append(ToString("data/kmkv/animations/"));
    filePath.Append(name);
    filePath.Append(ToString(".kmkv"));
    Array<uint8> animFile = LoadEntireFile(filePath.ToArray(), &allocator);
    if (!animFile.data) {
        LOG_ERROR("Failed to open animation file at: %.*s\n", filePath.size, filePath.data);
        return false;
    }

    string fileString = {
        .size = animFile.size,
        .data = (char*)animFile.data
    };
    string keyword, value;
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
        if (StringEquals(keyword, ToString("anim"))) {
            currentAnimKey.WriteString(value);
            currentAnim = sprite->animations.Add(currentAnimKey);

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
                    if (!StringEquals(psdFile.layers[parentIndex].name.ToArray(), value)) {
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
                LOG_ERROR("Animation with no frames (%.*s)\n", filePath.size, filePath.data);
                return false;
            }
            else if (currentAnim->numFrames == 1) {
                currentAnim->frameTime[0] = 0.0f;
            }
        }
        else if (StringEquals(keyword, ToString("fps"))) { // TODO won't need this anymore
            int fps;
            if (!StringToIntBase10(value, &fps)) {
                LOG_ERROR("Animation file fps parse failed (%.*s)\n", filePath.size, filePath.data);
                return false;
            }
            if (fps < 0 && fps != -1) {
                LOG_ERROR("Animation file invalid fps %d (%.*s)\n", fps, filePath.size, filePath.data);
                return false;
            }
            currentAnim->fps = fps;
        }
        else if (StringEquals(keyword, ToString("loop"))) {
            currentAnim->loop = true;
        }
        else if (StringEquals(keyword, ToString("exit"))) {
            if (value.size == 0) {
                LOG_ERROR("Animation file missing exit information (%.*s)\n", filePath.size, filePath.data);
                return false;
            }

            string next = NextSplitElement(&value, ' ');
            int exitFromFrame;
            if (next.size > 0 && next[0] == '*') {
                // wildcard
                exitFromFrame = -1;
            }
            else {
                if (!StringToIntBase10(next, &exitFromFrame)) {
                    LOG_ERROR("Animation file invalid exit-from frame (%.*s)\n",
                              filePath.size, filePath.data);
                    return false;
                }
            }

            if (value.size == 0) {
                LOG_ERROR("Animation file missing exit-to animation (%.*s)\n",
                          filePath.size, filePath.data);
                return false;
            }

            next = NextSplitElement(&value, ' ');
            HashKey exitToAnim;
            exitToAnim.WriteString(next);

            if (value.size == 0) {
                LOG_ERROR("Animation file missing exit-to frame (%.*s)\n",
                          filePath.size, filePath.data);
                return false;
            }

            next = NextSplitElement(&value, '\n');
            int exitToFrame;
            {
                if (!StringToIntBase10(next, &exitToFrame)) {
                    LOG_ERROR("Animation file invalid exit-to frame (%.*s)\n",
                              filePath.size, filePath.data);
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
        else if (StringEquals(keyword, ToString("rootfollow"))) {
            currentAnim->rootFollow = true;
        }
        else if (StringEquals(keyword, ToString("rootfollowendloop"))) {
            currentAnim->rootFollowEndLoop = true;
        }
        else if (StringEquals(keyword, ToString("rootmotion"))) {
            currentAnim->rootMotion = true;

            Vec2 rootPosWorld0 = Vec2::zero;
            for (int i = 0; i < currentAnim->numFrames; i++) {
                // Read root motion coordinate pair
                string next = NextSplitElement(&value, '\n');
                string trimmed = TrimWhitespace(next);

                // Parse root motion coordinate pair
                Vec2Int rootPos;
                int parsedElements;
                if (!StringToElementArray(trimmed, ' ', false,
                                          StringToIntBase10, 2, rootPos.e, &parsedElements)) {
                    LOG_ERROR("Failed to parse root motion coordinates %.*s (%.*s)\n",
                              trimmed.size, trimmed.data, filePath.size, filePath.data);
                    return false;
                }
                if (parsedElements != 2) {
                    LOG_ERROR("Not enough coordinates in root motion %.*s (%.*s)\n",
                              trimmed.size, trimmed.data, filePath.size, filePath.data);
                    return false;
                }

                rootPos.y = sprite->textureSize.y - rootPos.y;
                Vec2 rootPosWorld = {
                    (float32)rootPos.x / pixelsPerUnit,
                    (float32)rootPos.y / pixelsPerUnit
                };
                currentAnim->frameRootAnchor[i] = {
                    (float32)rootPos.x / sprite->textureSize.x,
                    (float32)rootPos.y / sprite->textureSize.y
                };

                if (i == 0) {
                    rootPosWorld0 = rootPosWorld;
                }
                currentAnim->frameRootMotion[i] = rootPosWorld - rootPosWorld0;

                if (next.size == 0) {
                    break;
                }
            }
        }
        else if (StringEquals(keyword, ToString("start"))) {
            HashKey startAnim;
            startAnim.WriteString(value);
            sprite->startAnimationKey = startAnim;
        }
        else if (StringEquals(keyword, ToString("//"))) {
            // Comment, ignore
        }
        else {
            LOG_ERROR("Animation file with unknown keyword: %.*s (%.*s)\n",
                      keyword.size, keyword.data, filePath.size, filePath.data);
            return false;
        }
    }

    // TODO maybe provide a friendlier way of iterating through HashTable
    for (uint32 k = 0; k < sprite->animations.capacity; k++) {
        const HashKey* animKey = &sprite->animations.pairs[k].key;
        if (animKey->s.size == 0) {
            continue;
        }

        const Animation* anim = &sprite->animations.pairs[k].value;
        for (int f = 0; f < anim->numFrames; f++) {
            const HashTable<int>* frameExitToTable = &anim->frameExitTo[f];
            for (uint32 j = 0; j < frameExitToTable->capacity; j++) {
                const HashKey* toAnimKey = &frameExitToTable->pairs[j].key;
                if (toAnimKey->s.size == 0) {
                    continue;
                }

                const int* exitToFrame = frameExitToTable->GetValue(*toAnimKey);
                if (exitToFrame != nullptr && *exitToFrame >= 0) {
                    const Animation* toAnim = sprite->animations.GetValue(*toAnimKey);
                    if (toAnim == nullptr) {
                        LOG_ERROR("Animation file non-existent exit-to animation %.*s (%.*s)\n",
                                  toAnimKey->s.size, toAnimKey->s.data, filePath.size, filePath.data);
                        return false;
                    }
                    if (*exitToFrame >= toAnim->numFrames) {
                        LOG_ERROR("Animation file exit-to frame out of bounds %d (%.*s)\n",
                                  *exitToFrame, filePath.size, filePath.data);
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

void UnloadAnimatedSprite(AnimatedSprite* sprite)
{
    for (uint32 k = 0; k < sprite->animations.capacity; k++) {
        if (sprite->animations.pairs[k].key.s.size == 0) {
            continue;
        }

        const Animation& animation = sprite->animations.pairs[k].value;
        for (int i = 0; i < animation.numFrames; i++) {
            UnloadTexture(animation.frameTextures[i]);
        }
    }
}
