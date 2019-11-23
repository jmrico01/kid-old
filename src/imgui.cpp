#include "imgui.h"

#include "opengl_base.h"
#include "text.h"

void Panel::Begin()
{
	size = Vec2Int::zero;
}

void Panel::Draw(ScreenInfo screenInfo, RectGL rectGL, TextGL textGL, Vec2Int position, Vec2 anchor)
{
	DrawRect(rectGL, screenInfo, position, anchor, size, Vec4 { 1.0f, 0.0f, 0.0f, 1.0f });
}

void Panel::Text(const FontFace& face, const char* text)
{
	const Array<char> textArray = {
		.size = StringLength(text),
		.data = (char*)text
	};
	size.x += GetTextWidth(face, textArray);
	size.y += face.height;

	PanelRenderCommand* command = renderInfo.renderCommands.Append();
	command->type = PanelRenderCommandType::TEXT;
	PanelRenderCommandText& commandText = command->commandText;
	// fill out commandText
}
