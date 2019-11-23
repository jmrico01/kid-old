#include "imgui.h"

#include "opengl_base.h"
#include "text.h"

global_var const Vec2 MARGIN_FRACTION = { 1.0f, 1.1f };

void Panel::Begin()
{
	size = Vec2Int::zero;
}

void Panel::Draw(ScreenInfo screenInfo, RectGL rectGL, TextGL textGL, Vec2Int position, Vec2 anchor,
	MemoryBlock transient)
{
	DrawRect(rectGL, screenInfo, position, anchor, size, Vec4 { 1.0f, 0.0f, 0.0f, 1.0f });

	for (uint64 i = 0; i < renderCommands.size; i++) {
		switch (renderCommands[i].type) {
			case PanelRenderCommandType::TEXT: {
				const PanelRenderCommandText& commandText = renderCommands[i].commandText;
				DrawText(textGL, *commandText.face, screenInfo, commandText.text.ToArray(),
					position, anchor, commandText.color, transient);
				position.y -= (int)(commandText.face->height * MARGIN_FRACTION.y);
			} break;
		}
	}
}

void Panel::Text(const FontFace& face, Array<char> text, Vec4 color)
{
	int sizeX = (int)(GetTextWidth(face, text) * MARGIN_FRACTION.x);
	size.x = MaxInt(size.x, sizeX);
	size.y += (int)(face.height * MARGIN_FRACTION.y);

	PanelRenderCommand* command = renderCommands.Append();
	command->type = PanelRenderCommandType::TEXT;
	command->commandText.face = &face;
	uint64 textLength = MinUInt64(text.size, TEXT_BUFFER_MAX);
	MemCopy(command->commandText.text.data, text.data, textLength);
	command->commandText.text.size = textLength;
	command->commandText.color = color;
}

void Panel::Text(const FontFace& face, const char* text, Vec4 color)
{
	const Array<char> textArray = {
		.size = StringLength(text),
		.data = (char*)text
	};
	Text(face, textArray, color);
}
