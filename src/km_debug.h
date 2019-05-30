#pragma once

#include <stdlib.h>

#include "km_defines.h"
#include "km_log.h"
#include "km_math.h"
#include "main.h"
#include "main_platform.h"
#include "opengl_base.h"
#include "text.h"

#if GAME_SLOW
#define DEBUG_ASSERTF(expression, format, ...) if (!(expression)) { \
    LOG_ERROR("Assert failed:\n"); \
    LOG_ERROR(format, ##__VA_ARGS__); \
    flushLogs_(logState_); \
    abort(); }
#define DEBUG_ASSERT(expression) DEBUG_ASSERTF(expression, "")
#define DEBUG_PANIC(format, ...) \
	LOG_ERROR("PANIC!\n"); \
	LOG_ERROR(format, ##__VA_ARGS__); \
	flushLogs_(logState_); \
    abort();
#elif GAME_INTERNAL
#define DEBUG_ASSERTF(expression, format, ...) if (!(expression)) { \
    LOG_ERROR("Assert failed\n"); \
    LOG_ERROR(format, ##__VA_ARGS__); \
    flushLogs_(logState_); }
#define DEBUG_ASSERT(expression) DEBUG_ASSERTF(expression, "")
#define DEBUG_PANIC(format, ...) \
    LOG_ERROR("PANIC!\n"); \
    LOG_ERROR(format, ##__VA_ARGS__); \
    flushLogs_(logState_);
#else
// TODO rethink these macros maybe, at least the panic
#define DEBUG_ASSERTF(expression, format, ...)
#define DEBUG_ASSERT(expression)
#define DEBUG_PANIC(format, ...)
#endif

#define INPUT_BUFFER_SIZE 2048

struct Button;
struct InputField;
typedef void (*ButtonCallback)(Button*, void*);
typedef void (*InputFieldCallback)(InputField*, void*);

struct ClickableBox
{
    Vec2Int origin;
    Vec2Int size;
    Vec2 anchor;

    bool32 hovered;
    bool32 pressed;

    Vec4 color;
    Vec4 hoverColor;
    Vec4 pressColor;
};

struct Button
{
    ClickableBox box;
    char text[INPUT_BUFFER_SIZE];
    ButtonCallback callback;
    Vec4 textColor;
};

struct InputField
{
    ClickableBox box;
    char text[INPUT_BUFFER_SIZE];
    uint32 textLen;
    InputFieldCallback callback;
    Vec4 textColor;
};

void InitClickableBox(ClickableBox* box, Vec4 color, Vec4 hoverColor, Vec4 pressColor);

ClickableBox CreateClickableBox(Vec2Int origin, Vec2Int size, Vec2 anchor,
    Vec4 color, Vec4 hoverColor, Vec4 pressColor);
Button CreateButton(Vec2Int origin, Vec2Int size, Vec2 anchor,
    const char* text, ButtonCallback callback,
    Vec4 color, Vec4 hoverColor, Vec4 pressColor, Vec4 textColor);
InputField CreateInputField(Vec2Int origin, Vec2Int size, Vec2 anchor,
    const char* text, InputFieldCallback callback,
    Vec4 color, Vec4 hoverColor, Vec4 pressColor, Vec4 textColor);

void UpdateClickableBoxes(ClickableBox boxes[], uint32 n,
    const GameInput* input);
void DrawClickableBoxes(ClickableBox boxes[], uint32 n,
    RectGL rectGL, ScreenInfo screenInfo);

void UpdateButtons(Button buttons[], uint32 n,
    const GameInput* input, void* data);
void DrawButtons(Button buttons[], uint32 n,
    RectGL rectGL, TextGL textGL, const FontFace& face,
    ScreenInfo screenInfo, MemoryBlock transient);

void UpdateInputFields(InputField fields[], uint32 n,
    const GameInput* input, void* data);
void DrawInputFields(InputField fields[], uint32 n,
    RectGL rectGL, TextGL textGL, const FontFace& face,
    ScreenInfo screenInfo, MemoryBlock transient);
