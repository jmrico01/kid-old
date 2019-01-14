#include "main.h"

#undef internal
#include <random>
#define internal static

#include "main_platform.h"
#include "km_debug.h"
#include "km_defines.h"
#include "km_input.h"
#include "km_math.h"
#include "km_string.h"
#include "opengl.h"
#include "opengl_funcs.h"
#include "opengl_base.h"
#include "post.h"
#include "render.h"

#define REF_PIXEL_SCREEN_HEIGHT 1080
#define REF_PIXELS_PER_UNIT     120

#define FLOOR_LEVEL 0.0f

#define LINE_COLLIDER_WIDTH 0.1f

inline float32 RandFloat32()
{
	return (float32)rand() / RAND_MAX;
}
inline float32 RandFloat32(float32 min, float32 max)
{
	DEBUG_ASSERT(max > min);
	return RandFloat32() * (max - min) + min;
}

Mat4 CalculateWorldMatrix(ScreenInfo screenInfo)
{
	const float32 ASPECT_RATIO = (float32)screenInfo.size.x / screenInfo.size.y;
	const float32 CAMERA_HEIGHT = (float32)REF_PIXEL_SCREEN_HEIGHT / REF_PIXELS_PER_UNIT;
	const Vec3 CAMERA_OFFSET = { 0.0f, -CAMERA_HEIGHT / 3.0f, 0.0f };
	const Vec3 SCREEN_CENTER = {
		CAMERA_HEIGHT * ASPECT_RATIO / 2.0f,
		CAMERA_HEIGHT / 2.0f,
		0.0f
	};

	Vec3 scale = {
		2.0f * REF_PIXELS_PER_UNIT / REF_PIXEL_SCREEN_HEIGHT / ASPECT_RATIO,
		2.0f * REF_PIXELS_PER_UNIT / REF_PIXEL_SCREEN_HEIGHT,
		1.0f
	};
	return Translate(Vec3 { -1.0f, -1.0f, 0.0f })
		* Scale(scale) * Translate(SCREEN_CENTER + CAMERA_OFFSET);
}

void DrawObjectStatic(const ObjectStatic& objectStatic, SpriteDataGL* spriteDataGL)
{
	Vec2 size = ToVec2(objectStatic.texture.size) / REF_PIXELS_PER_UNIT;
	PushSpriteWorldSpace(spriteDataGL, objectStatic.pos, size,
		objectStatic.anchor, false,
		objectStatic.texture.textureID);
}

void DrawObjectAnimated(const ObjectAnimated& objectAnimated, SpriteDataGL* spriteDataGL)
{
	Vec2 size = ToVec2(objectAnimated.sprite.textureSize) / REF_PIXELS_PER_UNIT;
	objectAnimated.sprite.Draw(spriteDataGL,
		objectAnimated.pos, size, objectAnimated.anchor, false);
}

void PlayerMovementInput(GameState* gameState, float32 deltaTime, const GameInput* input)
{
	const float32 PLAYER_WALK_SPEED = 1.54f;
	const float32 PLAYER_JUMP_SPEED = 4.2f;

	if (IsKeyPressed(input, KM_KEY_A)) {
		gameState->playerVel.x -= PLAYER_WALK_SPEED;
		gameState->facingRight = false;
	}
	if (IsKeyPressed(input, KM_KEY_D)) {
		gameState->playerVel.x += PLAYER_WALK_SPEED;
		gameState->facingRight = true;
	}

	if (!gameState->falling) {
		if (WasKeyPressed(input, KM_KEY_SPACE)) {
			gameState->playerVel.y = PLAYER_JUMP_SPEED;
			gameState->falling = true;
			gameState->audioState.soundSnare.playing = true;
			gameState->audioState.soundSnare.sampleIndex = 0;
		}
	}
}

void UpdateTown(GameState* gameState, float32 deltaTime, const GameInput* input)
{
	gameState->playerVel.x = 0.0f;
#if GAME_INTERNAL
	if (gameState->editor) {
		if (input->mouseButtons[0].isDown) {
			// TODO fix (screen to world)
			//gameState->cameraPos -= input->mouseDelta;
		}
	}
	else {
		PlayerMovementInput(gameState, deltaTime, input);
	}
#else
	PlayerMovementInput(gameState, deltaTime, input);
#endif

	const float32 GRAVITY_ACCEL = 8.3f;

	if (gameState->falling) {
		gameState->playerVel.y -= GRAVITY_ACCEL * deltaTime;
	}
	else {
		gameState->playerVel.y = 0.0f;
	}
	gameState->playerPos += gameState->playerVel * deltaTime;

	Vec2 playerPos = gameState->playerPos;
	float32 floorHeight = -1e9f;
	for (int i = 0; i < gameState->numLineColliders; i++) {
		const ColliderLine& lineCollider = gameState->lineColliders[i];
		float32 matchHeight = -1e9f;
		for (int j = 1; j < lineCollider.numVertices; j++) {
			Vec2 vertPrev = lineCollider.vertices[j - 1];
			Vec2 vert = lineCollider.vertices[j];
			if (vertPrev.x <= playerPos.x && vert.x >= playerPos.x) {
				float32 t = (playerPos.x - vertPrev.x) / (vert.x - vertPrev.x);
				float32 height = vertPrev.y + t * (vert.y - vertPrev.y);
				if (height - LINE_COLLIDER_WIDTH <= playerPos.y) {
					matchHeight = height;
					break;
				}
			}
		}

		if (matchHeight > floorHeight) {
			floorHeight = matchHeight;
		}
	}

	if (gameState->falling) {
		if (gameState->playerPos.y < floorHeight) {
			gameState->playerPos.y = floorHeight;
			gameState->playerVel.y = 0.0f;
			gameState->falling = false;
		}
	}
	else {
		if (gameState->playerPos.y > floorHeight - LINE_COLLIDER_WIDTH
		&& gameState->playerPos.y < floorHeight + LINE_COLLIDER_WIDTH) {
			gameState->playerPos.y = floorHeight;
		}
		else if (gameState->playerPos.y > floorHeight + LINE_COLLIDER_WIDTH) {
			gameState->falling = true;
			gameState->playerVel.y = 0.0f;
		}
	}

	// TODO ideally animation IDs would be strings
	const int KID_IDLE_ANIMS[2] = { 0, 1 };
	const int KID_WALK_ANIMS[1] = { 2 };
	const int KID_JUMP_ANIMS[2] = { 3, 4 };
	const int* nextAnims = KID_IDLE_ANIMS;
	int numNextAnims = 2;
	if (gameState->falling) {
		nextAnims = KID_JUMP_ANIMS;
		numNextAnims = 2;
	}
	else if (gameState->playerVel.x != 0) {
		nextAnims = KID_WALK_ANIMS;
		numNextAnims = 1;
	}
	gameState->spriteKid.Update(deltaTime, numNextAnims, nextAnims);
	gameState->spriteMe.Update(deltaTime, numNextAnims, nextAnims);

#if GAME_INTERNAL
	gameState->floorHeight = floorHeight;
#endif
}

void DrawTown(GameState* gameState, SpriteDataGL* spriteDataGL, Mat4 worldMatrix)
{
#if GAME_INTERNAL
	if (!gameState->editor) {
		gameState->cameraPos = gameState->playerPos;
	}
#else
	gameState->cameraPos = gameState->playerPos;
#endif

	DrawObjectStatic(gameState->background, spriteDataGL);

	{ // kid & me text
		Vec2 anchor = { 0.5f, 0.08f };
		Vec2 size = ToVec2(gameState->spriteKid.textureSize) / REF_PIXELS_PER_UNIT;
		// Vec2 meTextOffset = { -0.1f, 0.1f };

		gameState->spriteKid.Draw(spriteDataGL, gameState->playerPos, size,
			anchor, !gameState->facingRight);
	}

	Vec3 cameraPos = { gameState->cameraPos.x, gameState->cameraPos.y, 0.0f };
	Mat4 view = Translate(-cameraPos);
	DrawSprites(gameState->renderState, *spriteDataGL, view, worldMatrix);

#if 0
#if GAME_INTERNAL
	if (gameState->editor) {
		gameState->guys.box.origin = ToVec2Int(gameState->guys.pos * REF_SCALE_FACTOR)
			+ objectOffset;
		gameState->guys.box.size = gameState->guys.sprite.textureSize * REF_SCALE_FACTOR;
		DrawClickableBoxes(&gameState->guys.box, 1, gameState->rectGL, screenInfo);
		gameState->bush.box.origin = ToVec2Int(gameState->bush.pos * REF_SCALE_FACTOR)
			+ objectOffset;
		gameState->bush.box.size = gameState->bush.sprite.textureSize * REF_SCALE_FACTOR;
		DrawClickableBoxes(&gameState->bush.box, 1, gameState->rectGL, screenInfo);
	}
#endif
#endif
}

void UpdateFishing(GameState* gameState, float32 deltaTime,
	const GameInput* input, ScreenInfo screenInfo)
{
	const int PLAYER_MOVE_SPEED = 600;
	const Vec2Int PLAYER_SIZE = { 170, 250 };
	const int REF_X = (int)(screenInfo.size.x * 1080.0f / screenInfo.size.y);
	const int MARGIN_X = (REF_X - 1440) / 2;

	gameState->obstacleTimer += deltaTime;
	if (gameState->obstacleTimer >= 0.25f) {
		if (gameState->numObstacles < FISHING_OBSTACLES_MAX) {
			FishingObstacle& newObstacle = gameState->obstacles[gameState->numObstacles];
			newObstacle.pos = Vec2Int { rand() % REF_X, 0 };
			newObstacle.vel = Vec2Int { 0, PLAYER_MOVE_SPEED - 100 + (rand() % 400) };
			newObstacle.size = Vec2Int { 100, 100 };
			gameState->numObstacles++;
			gameState->obstacleTimer = 0.0f;
		}
	}
	for (int i = 0; i < gameState->numObstacles; i++) {
		gameState->obstacles[i].pos += gameState->obstacles[i].vel * deltaTime;
	}

	if (IsKeyPressed(input, KM_KEY_A)) {
		gameState->playerPosX -= (int)(PLAYER_MOVE_SPEED * deltaTime);
	}
	if (IsKeyPressed(input, KM_KEY_D)) {
		gameState->playerPosX += (int)(PLAYER_MOVE_SPEED * deltaTime);
	}

	int minX = MARGIN_X + PLAYER_SIZE.x / 2;
	int maxX = REF_X - MARGIN_X - PLAYER_SIZE.x / 2;
	if (gameState->playerPosX < minX) {
		gameState->playerPosX = minX;
	}
	if (gameState->playerPosX > maxX) {
		gameState->playerPosX = maxX;
	}
}

void DrawFishing(GameState* gameState, SpriteDataGL* spriteDataGL, ScreenInfo screenInfo)
{
	const float32 REF_SCALE_FACTOR = screenInfo.size.y / 1080.0f;
	const Vec2Int PLAYER_SIZE = { 170, 250 };

	Vec2Int playerPos = { gameState->playerPosX, 700 };
	playerPos *= REF_SCALE_FACTOR;
	Vec2Int playerSize = PLAYER_SIZE * REF_SCALE_FACTOR;
	DrawRect(gameState->rectGL, screenInfo,
		playerPos, Vec2 { 0.5f, 0.0f }, playerSize,
		Vec4 { 1.0f, 0.0f, 1.0f, 1.0f });

	for (int i = 0; i < gameState->numObstacles; i++) {
		Vec2Int obstaclePos = gameState->obstacles[i].pos;
		obstaclePos *= REF_SCALE_FACTOR;
		Vec2Int obstacleSize = gameState->obstacles[i].size;
		obstacleSize *= REF_SCALE_FACTOR;
		DrawRect(gameState->rectGL, screenInfo,
			obstaclePos, Vec2 { 0.5f, 1.0f }, obstacleSize,
			Vec4 { 0.0f, 0.0f, 0.0f, 1.0f });
	}
}

extern "C" GAME_UPDATE_AND_RENDER_FUNC(GameUpdateAndRender)
{
	// NOTE: for clarity
	// A call to this function means the following has happened:
	//  - A frame has been displayed to the user
	//  - The latest user input has been processed by the platform layer
	//
	// This function is expected to update the state of the game
	// and draw the frame that will be displayed, ideally, some constant
	// amount of time in the future.
	DEBUG_ASSERT(sizeof(GameState) <= memory->permanent.size);

	GameState *gameState = (GameState*)memory->permanent.memory;
	if (memory->DEBUGShouldInitGlobalFuncs) {
		// Initialize global function names
#if GAME_SLOW
		debugPrint_ = platformFuncs->DEBUGPlatformPrint;
#endif
		#define FUNC(returntype, name, ...) name = \
		platformFuncs->glFunctions.name;
			GL_FUNCTIONS_BASE
			GL_FUNCTIONS_ALL
		#undef FUNC

		memory->DEBUGShouldInitGlobalFuncs = false;
	}
	if (!memory->isInitialized) {
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

		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
		glCullFace(GL_BACK);

		glLineWidth(2.0f);

		InitAudioState(thread, &gameState->audioState, audio,
			&memory->transient,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		// Game data
		gameState->activeScene = SCENE_TOWN;

		// town data init
		gameState->playerPos = Vec2 { 0.0f, 5.0f };
		gameState->playerVel = Vec2 { 0.0f, 0.0f };
		gameState->falling = true;
		gameState->facingRight = true;

		gameState->numLineColliders = 0;
		ColliderLine* lineCollider;

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->numVertices = 2;
		lineCollider->vertices[0] = { -16.0f, FLOOR_LEVEL };
		lineCollider->vertices[1] = { 16.0f, FLOOR_LEVEL };

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->numVertices = 2;
		lineCollider->vertices[0] = { 2.0f, 0.5f };
		lineCollider->vertices[1] = { 6.0f, 2.0f };

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->numVertices = 7;
		lineCollider->vertices[0] = { 5.0f, 1.5f };
		lineCollider->vertices[1] = { 7.0f, 1.8f };
		lineCollider->vertices[2] = { 9.0f, 1.5f };
		lineCollider->vertices[3] = { 11.0f, 2.0f };
		lineCollider->vertices[4] = { 13.0f, 3.0f };
		lineCollider->vertices[5] = { 15.0f, 5.0f };
		lineCollider->vertices[6] = { 25.0f, 10.0f };

		lineCollider = &gameState->lineColliders[gameState->numLineColliders++];
		lineCollider->numVertices = 2;
		lineCollider->vertices[0] = { -16.0f, 10.0f };
		lineCollider->vertices[1] = { -2.0f, 2.0f };

		// fishing data init
		gameState->playerPosX = screenInfo.size.x;
		gameState->obstacleTimer = 0.0f;
		gameState->numObstacles = 0;

		gameState->grainTime = 0.0f;

#if GAME_INTERNAL
		gameState->debugView = true;
		gameState->editor = false;
#endif

		// Rendering stuff
		InitRenderState(gameState->renderState, thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		gameState->rectGL = InitRectGL(thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->texturedRectGL = InitTexturedRectGL(thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->lineGL = InitLineGL(thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->textGL = InitTextGL(thread,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		FT_Error error = FT_Init_FreeType(&gameState->ftLibrary);
		if (error) {
			DEBUG_PRINT("FreeType init error: %d\n", error);
		}
		gameState->fontFaceSmall = LoadFontFace(thread, gameState->ftLibrary,
			"data/fonts/ocr-a/regular.ttf", 18,
			memory->transient,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->fontFaceMedium = LoadFontFace(thread, gameState->ftLibrary,
			"data/fonts/ocr-a/regular.ttf", 24,
			memory->transient,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

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

		gameState->screenShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/screen.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->bloomExtractShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/bloomExtract.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->bloomBlendShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/bloomBlend.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->blurShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/blur.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		gameState->grainShader = LoadShaders(thread,
			"shaders/screen.vert", "shaders/grain.frag",
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);

		gameState->background.pos = { 0.0f, -3.05f };
		gameState->background.anchor = { 0.5f, 0.0f };
		bool32 loadBackground = LoadPNGOpenGL(thread,
			"data/sprites/bg.png", gameState->background.texture,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		if (!loadBackground) {
			DEBUG_PANIC("Failed to load background");
		}

		bool32 loadKidAnim = LoadAnimatedSprite(thread, "data/animations/kid/kid.kma",
			gameState->spriteKid,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		if (!loadKidAnim) {
			DEBUG_PANIC("Failed to load kid animation sprite");
		}
		bool32 loadMeAnim = LoadAnimatedSprite(thread, "data/animations/me/me.kma",
			gameState->spriteMe,
			platformFuncs->DEBUGPlatformReadFile,
			platformFuncs->DEBUGPlatformFreeFileMemory);
		if (!loadMeAnim) {
			DEBUG_PANIC("Failed to load me animation sprite");
		}

#if GAME_INTERNAL
		/*gameState->guys.box = CreateClickableBox(gameState->guys.pos,
			gameState->guys.sprite.textureSize,
			gameState->guys.anchor,
			Vec4 { 0.0f, 0.0f, 0.0f, 0.1f },
			Vec4 { 0.0f, 0.0f, 0.0f, 0.4f },
			Vec4 { 0.0f, 0.0f, 0.0f, 0.7f }
		);
		gameState->bush.box = CreateClickableBox(gameState->bush.pos,
			gameState->bush.sprite.textureSize,
			gameState->bush.anchor,
			Vec4 { 0.0f, 0.0f, 0.0f, 0.1f },
			Vec4 { 0.0f, 0.0f, 0.0f, 0.4f },
			Vec4 { 0.0f, 0.0f, 0.0f, 0.7f }
		);*/
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

		DEBUG_PRINT("Updated screen-size-dependent info\n");
	}

	// gameState->grainTime = fmod(gameState->grainTime + deltaTime, 5.0f);

	if (WasKeyPressed(input, KM_KEY_1)) {
		gameState->activeScene = SCENE_TOWN;
	}
	if (WasKeyPressed(input, KM_KEY_2)) {
		gameState->activeScene = SCENE_FISHING;
	}

	// Toggle global mute
	if (WasKeyPressed(input, KM_KEY_M)) {
		gameState->audioState.globalMute = !gameState->audioState.globalMute;
	}

	switch (gameState->activeScene) {
		case SCENE_TOWN: {
			UpdateTown(gameState, deltaTime, input);
		} break;
		case SCENE_FISHING: {
			UpdateFishing(gameState, deltaTime, input, screenInfo);
		} break;
	}

	// ---------------------------- Begin Rendering ---------------------------
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glBindFramebuffer(GL_FRAMEBUFFER, gameState->framebuffersColorDepth[0].framebuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Mat4 worldMatrix = CalculateWorldMatrix(screenInfo);

	DEBUG_ASSERT(memory->transient.size >= sizeof(SpriteDataGL));
	SpriteDataGL* spriteDataGL = (SpriteDataGL*)memory->transient.memory;
	spriteDataGL->numSprites = 0;

	switch (gameState->activeScene) {
		case SCENE_TOWN: {
			DrawTown(gameState, spriteDataGL, worldMatrix);
		} break;
		case SCENE_FISHING: {
			DrawFishing(gameState, spriteDataGL, screenInfo);
		} break;
	}

	// ------------------------ Post processing passes ------------------------
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	// Apply filters
	// PostProcessGrain(gameState->framebuffersColorDepth[0], gameState->framebuffersColor[0],
	//     gameState->screenQuadVertexArray,
	//     gameState->grainShader, gameState->grainTime);

	// Render to screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindVertexArray(gameState->screenQuadVertexArray);
	glUseProgram(gameState->screenShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gameState->framebuffersColorDepth[0].color);
	GLint loc = glGetUniformLocation(gameState->screenShader, "framebufferTexture");
	glUniform1i(loc, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	// Render pillarbox
	const float32 ASPECT_RATIO = 4.0f / 3.0f;
	const Vec4 PILLARBOX_COLOR = { 0.0f, 0.0f, 0.0f, 1.0f }; 
	int targetWidth = (int)(screenInfo.size.y * ASPECT_RATIO);
	int pillarboxWidth = (screenInfo.size.x - targetWidth) / 2;
	Vec2Int pillarboxPos1 = Vec2Int::zero;
	Vec2Int pillarboxPos2 = { screenInfo.size.x - pillarboxWidth, 0 };
	Vec2 pillarboxAnchor = Vec2 { 0.0f, 0.0f };
	Vec2Int pillarboxSize = { pillarboxWidth, screenInfo.size.y };
	if (pillarboxWidth > 0) {
		DrawRect(gameState->rectGL, screenInfo,
			pillarboxPos1, pillarboxAnchor, pillarboxSize, PILLARBOX_COLOR);
		DrawRect(gameState->rectGL, screenInfo,
			pillarboxPos2, pillarboxAnchor, pillarboxSize, PILLARBOX_COLOR);
	}

	DrawRect(gameState->rectGL, screenInfo,
		input->mousePos, Vec2 { 0.5f, 0.5f }, Vec2Int { 5, 5 },
		Vec4 { 0.1f, 0.1f, 0.1f, 1.0f });

	// ---------------------------- End Rendering -----------------------------
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	OutputAudio(audio, gameState, input, memory->transient);

#if GAME_INTERNAL
	if (WasKeyPressed(input, KM_KEY_G)) {
		gameState->debugView = !gameState->debugView;
	}
	if (WasKeyPressed(input, KM_KEY_H)) {
		gameState->editor = !gameState->editor;
	}

	const Vec4 DEBUG_FONT_COLOR = { 0.05f, 0.05f, 0.05f, 1.0f };
	const Vec2Int MARGIN = { 15, 10 };

	if (gameState->debugView) {
		FontFace& textFont = gameState->fontFaceMedium;
		char textStr[128];
		sprintf(textStr, "%.2f FPS", 1.0f / deltaTime);
		Vec2Int textPos = {
			screenInfo.size.x - pillarboxWidth - MARGIN.x,
			screenInfo.size.y - MARGIN.y,
		};
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPos, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPos.y -= textFont.height;

		textPos.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f POS", gameState->playerPos.x, gameState->playerPos.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPos, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPos.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f VEL", gameState->playerVel.x, gameState->playerVel.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPos, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		textPos.y -= textFont.height;
		textPos.y -= textFont.height;
		sprintf(textStr, "%.2f|%.2f CAM", gameState->cameraPos.x, gameState->cameraPos.y);
		DrawText(gameState->textGL, textFont, screenInfo,
			textStr, textPos, Vec2 { 1.0f, 1.0f }, DEBUG_FONT_COLOR, memory->transient);

		DEBUG_ASSERT(memory->transient.size >= sizeof(LineGLData));
		LineGLData* lineData = (LineGLData*)memory->transient.memory;

		Vec3 cameraPos = { gameState->cameraPos.x, gameState->cameraPos.y, 0.0f };
		Mat4 view = Translate(-cameraPos);
		Vec4 lineColliderColor = { 0.0f, 1.0f, 1.0f, 1.0f };

		for (int i = 0; i < gameState->numLineColliders; i++) {
			ColliderLine& line = gameState->lineColliders[i];
			DEBUG_ASSERT(line.numVertices <= MAX_LINE_POINTS);

			lineData->count = line.numVertices;
			for (int v = 0; v < line.numVertices; v++) {
				lineData->pos[v] = Vec3 {
					line.vertices[v].x,
					line.vertices[v].y,
					0.0f
				};
			}
			DrawLine(gameState->lineGL, worldMatrix, view, lineData, lineColliderColor);
		}

		// Player position cross
		lineData->count = 2;
		float32 crossSize = 0.1f;
		Vec3 playerPos = { gameState->playerPos.x, gameState->playerPos.y, 0.0f };
		Vec4 playerPosColor = { 1.0f, 0.0f, 1.0f, 1.0f };

		lineData->pos[0] = playerPos;
		lineData->pos[0].x -= crossSize;
		lineData->pos[1] = playerPos;
		lineData->pos[1].x += crossSize;
		DrawLine(gameState->lineGL, worldMatrix, view, lineData, playerPosColor);

		lineData->pos[0] = playerPos;
		lineData->pos[0].y -= crossSize;
		lineData->pos[1] = playerPos;
		lineData->pos[1].y += crossSize;
		DrawLine(gameState->lineGL, worldMatrix, view, lineData, playerPosColor);

		Vec3 floorPos = { gameState->playerPos.x, gameState->floorHeight, 0.0f };
		Vec4 floorPosColor = { 1.0f, 1.0f, 0.0f, 1.0f };

		lineData->pos[0] = floorPos;
		lineData->pos[0].x -= crossSize;
		lineData->pos[1] = floorPos;
		lineData->pos[1].x += crossSize;
		DrawLine(gameState->lineGL, worldMatrix, view, lineData, floorPosColor);

		lineData->pos[0] = floorPos;
		lineData->pos[0].y -= crossSize;
		lineData->pos[1] = floorPos;
		lineData->pos[1].y += crossSize;
		DrawLine(gameState->lineGL, worldMatrix, view, lineData, floorPosColor);
	}
	if (gameState->editor) {
		Vec2Int editorStrPos = {
			pillarboxWidth + MARGIN.x,
			screenInfo.size.y - MARGIN.y,
		};
		Vec4 editorFontColor = { 1.0f, 0.1f, 1.0f, 1.0f };
		DrawText(gameState->textGL, gameState->fontFaceMedium, screenInfo,
			"EDITOR", editorStrPos, Vec2 { 0.0f, 1.0f }, editorFontColor, memory->transient);
	}

	DrawDebugAudioInfo(audio, gameState, input, screenInfo, memory->transient, DEBUG_FONT_COLOR);
#endif

#if GAME_SLOW
	// Catch-all site for OpenGL errors
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		DEBUG_PRINT("OpenGL error: 0x%x\n", err);
	}
#endif
}

#include "animation.cpp"
#include "audio.cpp"
#include "framebuffer.cpp"
#include "gui.cpp"
#include "km_input.cpp"
#include "km_lib.cpp"
#include "km_string.cpp"
#include "load_png.cpp"
#include "load_wav.cpp"
#include "opengl_base.cpp"
#include "particles.cpp"
#include "post.cpp"
#include "render.cpp"
#include "text.cpp"