#pragma once

#include <km_common/km_lib.h>

const uint64 TEXT_BUFFER_MAX = 256;
const uint64 INPUT_BUFFER_MAX = 256;

typedef FixedArray<char, INPUT_BUFFER_MAX> InputString;

enum class PanelRenderCommandType
{
	RECT,
	TEXT
};

struct PanelRenderCommandRect
{
	Vec2Int position;
	Vec2 anchor;
	Vec2Int size;
	Vec4 color;
};

struct PanelRenderCommandText
{
	Array<char> text; // NOTE we don't own this data
	const FontFace* font;
	Vec2Int position;
	Vec2 anchor;
	Vec4 color;
};

struct PanelRenderCommand
{
	PanelRenderCommandType type;
	union
	{
		PanelRenderCommandRect commandRect;
		PanelRenderCommandText commandText;
	};
};

struct Panel
{
	Vec2Int position;
	Vec2Int positionCurrent;
	Vec2 anchor;
	Vec2Int size;
	bool growDownwards;
	DynamicArray<PanelRenderCommand> renderCommands;
	const GameInput* input;
	const FontFace* fontDefault;

	void Begin(const GameInput* input, const FontFace* fontDefault,
		Vec2Int position, Vec2 anchor, bool growDownwards = true);

	void Text(Array<char> text, Vec4 color, const FontFace* font = nullptr);

	bool Button(Array<char> text, Vec4 color, const FontFace* font = nullptr);
	bool Checkbox(bool* value, Array<char> text, Vec4 color, const FontFace* font = nullptr);

	float32 InputFloat32();
	float32 InputSliderFloat32();
	int InputInt();
	int InputSliderInt();
	Vec2 InputVec2();
	Vec3 InputVec3();
	Vec4 InputVec4();
	Vec4 InputColor();
	void InputText(InputString* outText);

	template <typename Allocator>
	void Draw(ScreenInfo screenInfo, RectGL rectGL, TextGL textGL, Vec4 backgroundColor,
		Allocator* allocator);
};
