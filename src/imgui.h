#pragma once

#include <km_common/km_lib.h>

const uint64 INPUT_BUFFER_MAX = 256;

typedef FixedArray<char, INPUT_BUFFER_MAX> InputString;

enum class PanelRenderCommandType
{
	DRAW_TEXT
};

struct PanelRenderCommandDataText
{
	FontFace* face;
};

struct PanelRenderCommand
{
	PanelRenderCommandType type;
	union
	{
		PanelRenderCommandDataText dataText;
	};
};

struct PanelRenderInfo
{
	DynamicArray<PanelRenderCommand> renderCommands;
};

struct Panel
{
	Vec2Int size;

	void Begin();
	void End(); // draw here?

	void Text(const char* text);

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
