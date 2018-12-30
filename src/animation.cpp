#include "animation.h"

#include <cstdio>

#include "km_debug.h"
#include "load_png.h"

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

void Animation::Update(float32 deltaTime, bool32 moving)
{
	if (!moving && IsIdleFrame(currentFrame)) {
		return;
	}
	
	currentFrameTime += deltaTime;
	if (currentFrameTime > 1.0f / ANIMATION_FPS) {
		currentFrameTime = 0.0f;
		currentFrame++;
		if (currentFrame >= frames) {
			currentFrame = 0;
		}
	}
}

void Animation::Draw(TexturedRectGL texturedRectGL, ScreenInfo screenInfo,
    Vec2Int pos, Vec2 anchor, Vec2Int size, bool32 flipHorizontal)
{
	DrawTexturedRect(texturedRectGL, screenInfo,
		pos, anchor, size, flipHorizontal, frameTextures[currentFrame]);
}

Animation LoadAnimation(const ThreadContext* thread,
	int frames, const char* path,
	int numIdles, int idles[],
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	DEBUG_ASSERT(frames <= ANIMATION_MAX_FRAMES);

	Animation animation;
	animation.frames = frames;
	animation.currentFrame = 0;
	animation.currentFrameTime = 0.0f;
	animation.numIdles = numIdles;
	for (int i = 0; i < numIdles; i++) {
		animation.idles[i] = idles[i];
	}

	char framePath[PATH_MAX_LENGTH];
	for (int i = 0; i < frames; i++) {
		sprintf(framePath, "%s/%d.png", path, i);
		animation.frameTextures[i] = LoadPNGOpenGL(thread, framePath,
			DEBUGPlatformReadFile, DEBUGPlatformFreeFileMemory);
	}

	return animation;
}
