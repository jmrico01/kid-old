#pragma once

#include <km_common/km_lib.h>

const uint64 INPUT_BUFFER_MAX = 256;
using InputString = FixedArray<char, INPUT_BUFFER_MAX>;

enum class PanelRenderCommandType
{
	RECT,
	TEXT
};

using PanelRenderCommandFlags = uint8;
namespace PanelRenderCommandFlag
{
    constexpr PanelRenderCommandFlags OVERRIDE_COLOR = 1 << 0;
    constexpr PanelRenderCommandFlags OVERRIDE_ALPHA = 1 << 1;
}

struct PanelRenderCommandRect
{
	Vec2Int size;
};

struct PanelRenderCommandText
{
	Array<char> text; // NOTE we don't own this data
	const FontFace* font;
};

struct PanelRenderCommand
{
	PanelRenderCommandType type;
    PanelRenderCommandFlags flags;
    Vec2Int position;
    Vec2 anchor;
    Vec4 color;
	union
	{
		PanelRenderCommandRect commandRect;
		PanelRenderCommandText commandText;
	};
};

using PanelFlags = uint32;
namespace PanelFlag
{
    constexpr PanelFlags GROW_UPWARDS = 1 << 0;
    constexpr PanelFlags MINIMIZED    = 1 << 1;
}

struct Panel
{
    PanelFlags flags;
	Vec2Int position;
	Vec2Int positionCurrent;
	Vec2 anchor;
	Vec2Int size;
	DynamicArray<PanelRenderCommand> renderCommands;
	const GameInput* input;
	const FontFace* fontDefault;
    
	void Begin(const GameInput& input, const FontFace* fontDefault, PanelFlags flags,
               Vec2Int position, Vec2 anchor);
    
    void TitleBar(Array<char> text, bool* minimized = nullptr, Vec4 color = Vec4::zero, const FontFace* font = nullptr);
    
	void Text(Array<char> text, Vec4 color = Vec4::zero, const FontFace* font = nullptr);
    
	bool Button(Array<char> text, Vec4 color = Vec4::zero, const FontFace* font = nullptr);
	bool Checkbox(bool* value, Array<char> text, Vec4 color = Vec4::zero, const FontFace* font = nullptr);
    
    bool InputText(InputString* inputString, bool* focused, Vec4 color = Vec4::zero, const FontFace* font = nullptr);
    
	bool SliderFloat(float32* value, float32 min, float32 max, Vec4 color = Vec4::zero, const FontFace* font = nullptr);
    
	template <typename Allocator>
        void Draw(ScreenInfo screenInfo, RectGL rectGL, TextGL textGL, Vec2Int borderSize,
                  Vec4 defaultColor, Vec4 backgroundColor, Allocator* allocator);
    
    // Unimplemented ...
	int SliderInt();
	float32 InputFloat();
	int InputInt();
	Vec2 InputVec2();
	Vec3 InputVec3();
	Vec4 InputVec4();
	Vec4 InputColor();
};
