#pragma once

#include <km_common/km_lib.h>

const uint64 TEXT_BUFFER_MAX = 256;
const uint64 INPUT_BUFFER_MAX = 256;

typedef FixedArray<char, INPUT_BUFFER_MAX> InputString;

enum class PanelRenderCommandType
{
	TEXT
};

struct PanelRenderCommandText
{
	FixedArray<char, TEXT_BUFFER_MAX> text;
	const FontFace* face;
	Vec4 color;
};

struct PanelRenderCommand
{
	PanelRenderCommandType type;
	union
	{
		PanelRenderCommandText commandText;
	};
};

struct Panel
{
	Vec2Int size;
	DynamicArray<PanelRenderCommand> renderCommands;

	void Begin();
	void Draw(ScreenInfo screenInfo, RectGL rectGL, TextGL textGL, Vec2Int position, Vec2 anchor,
		MemoryBlock transient);

	void Text(const FontFace& face, Array<char> text, Vec4 color);
	void Text(const FontFace& face, const char* text, Vec4 color);

	bool ButtonToggle();
	bool ButtonTrigger();

	float32 InputFloat32();
	float32 InputSliderFloat32();
	int InputInt();
	int InputSliderInt();
	Vec2 InputVec2();
	Vec3 InputVec3();
	Vec4 InputVec4();
	Vec4 InputColor();
	void InputText(InputString* outText);
};
