#include "imgui.h"

#include "opengl_base.h"
#include "text.h"

global_var const float32 MARGIN_FRACTION = 1.1f;

void Panel::Begin(const GameInput& input, const FontFace* fontDefault, PanelFlags flags,
                  Vec2Int position, Vec2 anchor)
{
	DEBUG_ASSERT(fontDefault != nullptr);
    
    this->flags = flags;
	this->position = position;
	this->positionCurrent = position;
	this->anchor = anchor;
	this->size = Vec2Int::zero;
	this->input = &input;
	this->fontDefault = fontDefault;
}

template <typename Allocator>
void Panel::Draw(ScreenInfo screenInfo, RectGL rectGL, TextGL textGL, Vec2Int borderSize,
                 Vec4 defaultColor, Vec4 backgroundColor,
                 Allocator* allocator)
{
	// Draw background
    Vec2Int backgroundOffset = {
        Lerp(-borderSize.x, borderSize.x, anchor.x),
        Lerp(-borderSize.y, borderSize.y, anchor.y)
    };
    Vec2Int backgroundSize = size + borderSize * 2;
	DrawRect(rectGL, screenInfo, position + backgroundOffset, anchor, backgroundSize, backgroundColor);
    
	for (uint64 i = 0; i < renderCommands.size; i++) {
        const auto& command = renderCommands[i];
        Vec4 color = command.flags & PanelRenderCommandFlag::OVERRIDE_COLOR ? command.color : defaultColor;
        if (command.flags & PanelRenderCommandFlag::OVERRIDE_ALPHA) {
            color.a = command.color.a;
        }
		switch (command.type) {
			case PanelRenderCommandType::RECT: {
				DrawRect(rectGL, screenInfo, command.position, command.anchor, command.commandRect.size, color);
			} break;
			case PanelRenderCommandType::TEXT: {
				DrawText(textGL, *command.commandText.font, screenInfo, command.commandText.text,
                         command.position, command.anchor, color, allocator);
			} break;
		}
	}
}

void Panel::TitleBar(Array<char> text, bool* minimized, Vec4 color, const FontFace* font)
{
    DEBUG_ASSERT(renderCommands.size == 0);
    
    if (minimized == nullptr) {
        Text(text, color, font);
    }
    else {
        bool notMinimized = !(*minimized);
        Checkbox(&notMinimized, text, color, font);
        *minimized = !notMinimized;
        if (*minimized) {
            flags |= PanelFlag::MINIMIZED;
        }
    }
}

void Panel::Text(Array<char> text, Vec4 color, const FontFace* font)
{
    if (flags & PanelFlag::MINIMIZED) {
        return;
    }
    
	const FontFace* fontToUse = font == nullptr ? fontDefault : font;
    
	PanelRenderCommand* newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::TEXT;
    newCommand->flags = color == Vec4::zero ? 0 : PanelRenderCommandFlag::OVERRIDE_COLOR;
	newCommand->position = positionCurrent;
	newCommand->color = color;
	newCommand->anchor = anchor;
	newCommand->commandText.text = text;
	newCommand->commandText.font = fontToUse;
    
	int sizeX = (int)(GetTextWidth(*fontToUse, text) * MARGIN_FRACTION);
	int sizeY = (int)(fontToUse->height * MARGIN_FRACTION);
	if (flags & PanelFlag::GROW_UPWARDS) {
		positionCurrent.y += sizeY;
	}
	else {
		positionCurrent.y -= sizeY;
	}
	size.x = MaxInt(size.x, sizeX);
	size.y += sizeY;
}

bool Panel::Button(Array<char> text, Vec4 color, const FontFace* font)
{
    if (flags & PanelFlag::MINIMIZED) {
        return false;
    }
    
	const float32 PRESSED_ALPHA = 1.0f;
	const float32 HOVERED_ALPHA = 0.65f;
	const float32 IDLE_ALPHA = 0.3f;
    
	const FontFace* fontToUse = font == nullptr ? fontDefault : font;
    Vec2Int textSize = { GetTextWidth(*fontToUse, text), (int)fontToUse->height };
    Vec2Int boxSize = { (int)(textSize.x * MARGIN_FRACTION), (int)(textSize.y * MARGIN_FRACTION) };
    const Vec2Int boxOffset = Vec2Int {
        Lerp(boxSize.x / 2, -boxSize.x / 2, anchor.x),
        Lerp(boxSize.y / 2, -boxSize.y / 2, anchor.y)
    };
    const Vec2Int boxCenter = positionCurrent + boxOffset;
    const RectInt boxRect = {
        .min = boxCenter - boxSize / 2,
        .max = boxCenter + boxSize / 2
    };
    const bool hovered = IsInside(input->mousePos, boxRect);
    const bool pressed = hovered && input->mouseButtons[0].isDown;
    const bool changed = pressed && input->mouseButtons[0].transitions == 1;
    
	PanelRenderCommand* newCommand;
    
	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::RECT;
    newCommand->flags = PanelRenderCommandFlag::OVERRIDE_ALPHA;
    if (color != Vec4::zero) {
        newCommand->flags |= PanelRenderCommandFlag::OVERRIDE_COLOR;
    }
	newCommand->position = boxCenter;
	newCommand->anchor = Vec2 { 0.5f, 0.5f };
	newCommand->color = color;
    newCommand->color.a = pressed ? PRESSED_ALPHA : hovered ? HOVERED_ALPHA : IDLE_ALPHA;
	newCommand->commandRect.size = boxSize;
    
	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::TEXT;
    newCommand->flags = color == Vec4::zero ? 0 : PanelRenderCommandFlag::OVERRIDE_COLOR;
	newCommand->position = positionCurrent;
	newCommand->anchor = anchor;
	newCommand->color = color;
	newCommand->commandText.text = text;
	newCommand->commandText.font = fontToUse;
    
    const int sizeX = (int)(boxSize.x * MARGIN_FRACTION);
	const int sizeY = (int)(boxSize.y * MARGIN_FRACTION);
	if (flags & PanelFlag::GROW_UPWARDS) {
		positionCurrent.y += sizeY;
	}
	else {
		positionCurrent.y -= sizeY;
	}
	size.x = MaxInt(size.x, sizeX);
	size.y += sizeY;
    
    return changed;
}

bool Panel::Checkbox(bool* value, Array<char> text, Vec4 color, const FontFace* font)
{
	DEBUG_ASSERT(value != nullptr);
    if (flags & PanelFlag::MINIMIZED) {
        return false;
    }
    
	const float32 CHECKED_ALPHA   = 1.0f;
	const float32 HOVERED_ALPHA   = 0.65f;
	const float32 UNCHECKED_ALPHA = 0.3f;
    bool valueChanged = false;
    
	const FontFace* fontToUse = font == nullptr ? fontDefault : font;
    const int fontHeight = fontToUse->height;
    const Vec2Int boxOffset = Vec2Int {
        Lerp(fontHeight / 2, -fontHeight / 2, anchor.x),
        Lerp(fontHeight / 2, -fontHeight / 2, anchor.y)
    };
	const Vec2Int boxCenter = positionCurrent + boxOffset;
	const int boxSize = (int)(fontHeight / MARGIN_FRACTION);
	const Vec2Int boxSize2 = Vec2Int { boxSize, boxSize };
	const RectInt boxRect = {
        .min = boxCenter - boxSize2 / 2,
        .max = boxCenter + boxSize2 / 2
    };
	const bool hover = IsInside(input->mousePos, boxRect);
	if (hover && !input->mouseButtons[0].isDown && input->mouseButtons[0].transitions == 1) {
		*value = !(*value);
        valueChanged = true;
	}
    const float32 boxAlpha = *value ? CHECKED_ALPHA : (hover ? HOVERED_ALPHA : UNCHECKED_ALPHA);
    
	PanelRenderCommand* newCommand;
    
	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::RECT;
    newCommand->flags = PanelRenderCommandFlag::OVERRIDE_ALPHA;
    if (color != Vec4::zero) {
        newCommand->flags |= PanelRenderCommandFlag::OVERRIDE_COLOR;
    }
	newCommand->position = boxCenter;
	newCommand->anchor = Vec2 { 0.5f, 0.5f };
	newCommand->color = color;
    newCommand->color.a = boxAlpha;
	newCommand->commandRect.size = boxSize2;
    
	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::TEXT;
    newCommand->flags = color == Vec4::zero ? 0 : PanelRenderCommandFlag::OVERRIDE_COLOR;
	newCommand->position = positionCurrent + Vec2Int { Lerp(fontHeight, -fontHeight, anchor.x), 0 };
	newCommand->anchor = anchor;
	newCommand->color = color;
	newCommand->commandText.text = text;
	newCommand->commandText.font = fontToUse;
    
    const int sizeX = (int)(GetTextWidth(*fontToUse, text) * MARGIN_FRACTION) + fontHeight;
	const int sizeY = (int)(fontHeight * MARGIN_FRACTION);
	if (flags & PanelFlag::GROW_UPWARDS) {
		positionCurrent.y += sizeY;
	}
	else {
		positionCurrent.y -= sizeY;
	}
	size.x = MaxInt(size.x, sizeX);
	size.y += sizeY;
    
	return valueChanged;
}

bool Panel::SliderFloat(float32* value, float32 min, float32 max, Vec4 color, const FontFace* font)
{
	DEBUG_ASSERT(value != nullptr);
    if (flags & PanelFlag::MINIMIZED) {
        return false;
    }
    
	const Vec2Int sliderBarSize = Vec2Int { 200, 5 };
	const Vec2Int sliderSize = Vec2Int { 10, 30 };
	const int totalHeight = 40;
    const float32 SLIDER_HOVER_ALPHA = 0.9f;
    const float32 SLIDER_ALPHA = 0.6f;
    const float32 BACKGROUND_BAR_ALPHA = 0.4f;
    
	// const FontFace* fontToUse = font == nullptr ? fontDefault : font;
	float32 sliderT = (*value - min) / (max - min);
    
	bool valueChanged = false;
	RectInt sliderMouseRect;
	sliderMouseRect.min = Vec2Int {
		positionCurrent.x,
		positionCurrent.y - totalHeight / 2
	};
	sliderMouseRect.max = Vec2Int {
		positionCurrent.x + sliderBarSize.x,
		positionCurrent.y + totalHeight / 2
	};
	bool hover = IsInside(input->mousePos, sliderMouseRect);
	if (hover && input->mouseButtons[0].isDown) {
		int newSliderX = ClampInt(input->mousePos.x - positionCurrent.x, 0, sliderBarSize.x);
		float32 newSliderT = (float32)newSliderX / sliderBarSize.x;
		*value = newSliderT * (max - min) + min;
		valueChanged = true;
	}
    
	int sliderOffset = totalHeight / 2;
	Vec2Int pos = positionCurrent;
	if (flags & PanelFlag::GROW_UPWARDS) {
		pos.y += sliderOffset;
	}
	else {
		pos.y -= sliderOffset;
	}
    
	PanelRenderCommand* newCommand;
    
	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::RECT;
    newCommand->flags = PanelRenderCommandFlag::OVERRIDE_ALPHA;
	newCommand->position = pos;
	newCommand->anchor = Vec2 { 0.0f, 0.5f };
	newCommand->color = color;
    newCommand->color.a = BACKGROUND_BAR_ALPHA;
	newCommand->commandRect.size = sliderBarSize;
    
	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::RECT;
	newCommand->flags = PanelRenderCommandFlag::OVERRIDE_ALPHA;
    newCommand->position = Vec2Int {
		pos.x + (int)(sliderBarSize.x * sliderT),
		pos.y
	};
	newCommand->anchor = Vec2 { 0.0f, 0.5f };
    newCommand->color = color;
	newCommand->color.a = hover ? SLIDER_HOVER_ALPHA : SLIDER_ALPHA;
	newCommand->commandRect.size = sliderSize;
    
	if (flags & PanelFlag::GROW_UPWARDS) {
		positionCurrent.y += totalHeight;
	}
	else {
		positionCurrent.y -= totalHeight;
	}
	size.x = MaxInt(size.x, sliderBarSize.x);
    size.y += totalHeight;
    
	return valueChanged;
}

bool Panel::InputText(InputString* inputString, bool* focused, Vec4 color, const FontFace* font)
{
    DEBUG_ASSERT(inputString != nullptr && focused != nullptr);
    if (flags & PanelFlag::MINIMIZED) {
        return false;
    }
    
    bool inputChanged = false;
    if (*focused) {
        for (uint64 i = 0; i < input->keyboardStringLen; i++) {
            char c = input->keyboardString[i];
            if (c == 8) { // backspace
                if (inputString->size > 0) {
                    inputString->RemoveLast();
                }
                inputChanged = true;
                continue;
            }
            
            if (inputString->size >= INPUT_BUFFER_MAX) {
                continue;
            }
            inputChanged = true;
            inputString->Append(c);
        }
    }
    
	const float32 PRESSED_ALPHA = 0.5f;
	const float32 HOVERED_ALPHA = 0.2f;
	const float32 IDLE_ALPHA = 0.0f;
    const int BOX_MIN_WIDTH = 20;
    
	const FontFace* fontToUse = font == nullptr ? fontDefault : font;
    Vec2Int textSize = { GetTextWidth(*fontToUse, inputString->ToArray()), (int)fontToUse->height };
    Vec2Int boxSize = { (int)(textSize.x * MARGIN_FRACTION), (int)(textSize.y * MARGIN_FRACTION) };
    if (boxSize.x < BOX_MIN_WIDTH) {
        boxSize.x = BOX_MIN_WIDTH;
    }
    const Vec2Int boxOffset = Vec2Int {
        Lerp(boxSize.x / 2, -boxSize.x / 2, anchor.x),
        Lerp(boxSize.y / 2, -boxSize.y / 2, anchor.y)
    };
    const Vec2Int boxCenter = positionCurrent + boxOffset;
    const RectInt boxRect = {
        .min = boxCenter - boxSize / 2,
        .max = boxCenter + boxSize / 2
    };
    const bool hovered = IsInside(input->mousePos, boxRect);
    const bool pressed = hovered && input->mouseButtons[0].isDown;
    const bool changed = pressed && input->mouseButtons[0].transitions == 1;
    if (changed) {
        *focused = true;
    }
    
	PanelRenderCommand* newCommand;
    
	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::RECT;
    newCommand->flags = PanelRenderCommandFlag::OVERRIDE_ALPHA;
    if (color != Vec4::zero) {
        newCommand->flags |= PanelRenderCommandFlag::OVERRIDE_COLOR;
    }
	newCommand->position = boxCenter;
	newCommand->anchor = Vec2 { 0.5f, 0.5f };
	newCommand->color = color;
    newCommand->color.a = pressed ? PRESSED_ALPHA : hovered ? HOVERED_ALPHA : IDLE_ALPHA;
	newCommand->commandRect.size = boxSize;
    
	newCommand = renderCommands.Append();
	newCommand->type = PanelRenderCommandType::TEXT;
    newCommand->flags = color == Vec4::zero ? 0 : PanelRenderCommandFlag::OVERRIDE_COLOR;
	newCommand->position = positionCurrent;
	newCommand->anchor = anchor;
	newCommand->color = color;
	newCommand->commandText.text = inputString->ToArray();
	newCommand->commandText.font = fontToUse;
    
    const int sizeX = (int)(boxSize.x * MARGIN_FRACTION);
	const int sizeY = (int)(boxSize.y * MARGIN_FRACTION);
	if (flags & PanelFlag::GROW_UPWARDS) {
		positionCurrent.y += sizeY;
	}
	else {
		positionCurrent.y -= sizeY;
	}
	size.x = MaxInt(size.x, sizeX);
	size.y += sizeY;
    
    return inputChanged;
}
