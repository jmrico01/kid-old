#pragma once

#include "km_math.h"
#include "opengl_base.h"
#include "main.h"
#include "text.h"

#define INPUT_BUFFER_SIZE 2048

struct Button;
struct InputField;
typedef void (*ButtonCallback)(Button*, void*);
typedef void (*InputFieldCallback)(InputField*, void*);

struct ClickableBox
{
    Vec2Int origin;
    Vec2Int size;

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

ClickableBox CreateClickableBox(Vec2Int origin, Vec2Int size,
    Vec4 color, Vec4 hoverColor, Vec4 pressColor);
Button CreateButton(Vec2Int origin, Vec2Int size,
    const char* text, ButtonCallback callback,
    Vec4 color, Vec4 hoverColor, Vec4 pressColor, Vec4 textColor);
InputField CreateInputField(Vec2Int origin, Vec2Int size,
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