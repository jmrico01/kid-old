#include "imgui.h"

#include "opengl_base.h"
#include "text.h"

global_var const float32 MARGIN_FRACTION = 1.1f;

void Panel::Begin(const GameInput* input, const FontFace* fontDefault, Vec2Int position, Vec2 anchor)
{
	DEBUG_ASSERT(input != nullptr);
	DEBUG_ASSERT(fontDefault != nullptr);

	this->position = position;
	this->positionCurrent = position;
	this->anchor = anchor;
	this->size = Vec2Int::zero;
	this->input = input;
	this->fontDefault = fontDefault;
}

template <typename Allocator>
void Panel::Draw(ScreenInfo screenInfo, RectGL rectGL, TextGL textGL, Vec4 backgroundColor,
	Allocator* allocator)
{
	// Draw background
	DrawRect(rectGL, screenInfo, position, anchor, size, backgroundColor);

	for (uint64 i = 0; i < renderCommands.size; i++) {
		switch (renderCommands[i].type) {
			case PanelRenderCommandType::RECT: {
				const auto& commandRect = renderCommands[i].commandRect;
				DrawRect(rectGL, screenInfo, commandRect.position, commandRect.anchor,
					commandRect.size, commandRect.color);
			} break;
			case PanelRenderCommandType::TEXT: {
				const auto& commandText = renderCommands[i].commandText;
				DrawText(textGL, *commandText.font, screenInfo, commandText.text,
					commandText.position, commandText.anchor, commandText.color, allocator);
			} break;
		}
	}
}

void Panel::Text(Array<char> text, Vec4 color, const FontFace* font)
{
	const FontFace* fontToUse = font == nullptr ? fontDefault : font;

	PanelRenderCommand* newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::TEXT;
	newCommand->commandText.text = text;
	newCommand->commandText.font = fontToUse;
	newCommand->commandText.position = positionCurrent;
	newCommand->commandText.anchor = anchor;
	newCommand->commandText.color = color;

	int sizeX = (int)(GetTextWidth(*fontToUse, text) * MARGIN_FRACTION);
	int sizeY = (int)(fontToUse->height * MARGIN_FRACTION);
	positionCurrent.y -= sizeY;
	size.x = MaxInt(size.x, sizeX);
	size.y += sizeY;
}

bool Panel::Checkbox(bool* value, Array<char> text, Vec4 color, const FontFace* font)
{
	DEBUG_ASSERT(value != nullptr);

	const Vec4 CHECKED_COLOR   = Vec4 { 0.0f, 0.0f, 0.0f, 1.0f  };
	const Vec4 HOVERED_COLOR   = Vec4 { 0.0f, 0.0f, 0.0f, 0.65f };
	const Vec4 UNCHECKED_COLOR = Vec4 { 0.0f, 0.0f, 0.0f, 0.3f  };

	const FontFace* fontToUse = font == nullptr ? fontDefault : font;
	int fontHeight = fontToUse->height;
	Vec2Int boxCenter = positionCurrent + Vec2Int { fontHeight, fontHeight } / 2;
	int boxSize = (int)(fontHeight / MARGIN_FRACTION);
	Vec2Int boxSize2 = Vec2Int { boxSize, boxSize };

	RectInt boxRect;
	boxRect.min = boxCenter - boxSize2 / 2;
	boxRect.max = boxCenter + boxSize2 / 2;
	bool hover = IsInside(input->mousePos, boxRect);
	if (hover && !input->mouseButtons[0].isDown && input->mouseButtons[0].transitions == 1) {
		*value = !(*value);
	}
	Vec4 boxColor = *value ? CHECKED_COLOR : (hover ? HOVERED_COLOR : UNCHECKED_COLOR);

	PanelRenderCommand* newCommand;

	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::RECT;
	newCommand->commandRect.position = boxCenter;
	newCommand->commandRect.anchor = Vec2 { 0.5f, 0.5f };
	newCommand->commandRect.size = Vec2Int { boxSize, boxSize };
	newCommand->commandRect.color = boxColor;

	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::TEXT;
	newCommand->commandText.text = text;
	newCommand->commandText.font = fontToUse;
	newCommand->commandText.position = positionCurrent + Vec2Int { fontHeight, 0 };
	newCommand->commandText.anchor = anchor;
	newCommand->commandText.color = color;

	int sizeX = (int)(GetTextWidth(*fontToUse, text) * MARGIN_FRACTION) + fontHeight;
	int sizeY = (int)(fontHeight * MARGIN_FRACTION);
	positionCurrent.y -= sizeY;
	size.x = MaxInt(size.x, sizeX);
	size.y += sizeY;

	return false;
}
