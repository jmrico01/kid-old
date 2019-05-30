#include "km_debug.h"

#include <map>
#include <cstring>

#include "km_math.h"
#include "text.h"

void InitClickableBox(ClickableBox* box, Vec4 color, Vec4 hoverColor, Vec4 pressColor)
{
	box->hovered = false;
	box->pressed = false;

	box->color = color;
	box->hoverColor = hoverColor;
	box->pressColor = pressColor;
}

ClickableBox CreateClickableBox(Vec2Int origin, Vec2Int size, Vec2 anchor,
	Vec4 color, Vec4 hoverColor, Vec4 pressColor)
{
	ClickableBox box = {};
	box.origin = origin;
	box.size = size;
	box.anchor = anchor;

	box.hovered = false;
	box.pressed = false;

	box.color = color;
	box.hoverColor = hoverColor;
	box.pressColor = pressColor;

	return box;
}

Button CreateButton(Vec2Int origin, Vec2Int size, Vec2 anchor,
	const char* text, ButtonCallback callback,
	Vec4 color, Vec4 hoverColor, Vec4 pressColor, Vec4 textColor)
{
	Button button = {};
	button.box = CreateClickableBox(origin, size, anchor,
		color, hoverColor, pressColor);

	uint32 textLen = (uint32)strnlen(text, INPUT_BUFFER_SIZE - 1);
	strncpy(button.text, text, textLen);
	button.text[textLen] = '\0';
	button.callback = callback;

	button.textColor = textColor;

	return button;
}

InputField CreateInputField(Vec2Int origin, Vec2Int size, Vec2 anchor,
	const char* text, InputFieldCallback callback,
	Vec4 color, Vec4 hoverColor, Vec4 pressColor, Vec4 textColor)
{
	InputField inputField = {};
	inputField.box = CreateClickableBox(origin, size, anchor,
		color, hoverColor, pressColor);

	inputField.textLen = (uint32)strnlen(text, INPUT_BUFFER_SIZE - 1);
	strncpy(inputField.text, text, inputField.textLen);
	inputField.text[inputField.textLen] = '\0';
	inputField.callback = callback;

	inputField.textColor = textColor;

	return inputField;
}

void UpdateClickableBoxes(ClickableBox boxes[], uint32 n,
	const GameInput* input)
{
	Vec2Int mousePos = input->mousePos;

	for (uint32 i = 0; i < n; i++) {
		Vec2Int boxOrigin = boxes[i].origin;
		Vec2Int boxSize = boxes[i].size;
		Vec2Int mousePosAnchored = {
			(int)(mousePos.x + boxes[i].size.x * boxes[i].anchor.x),
			(int)(mousePos.y + boxes[i].size.y * boxes[i].anchor.y)
		};

		if ((mousePosAnchored.x >= boxOrigin.x
		&& mousePosAnchored.x <= boxOrigin.x + boxSize.x) &&
		(mousePosAnchored.y >= boxOrigin.y
		&& mousePosAnchored.y <= boxOrigin.y + boxSize.y)) {
			boxes[i].hovered = true;
			boxes[i].pressed = input->mouseButtons[0].isDown;
		}
		else {
			boxes[i].hovered = false;
			boxes[i].pressed = false;
		}
	}
}

void DrawClickableBoxes(ClickableBox boxes[], uint32 n,
	RectGL rectGL, ScreenInfo screenInfo)
{
	for (uint32 i = 0; i < n; i++) {
		Vec4 color = boxes[i].color;
		if (boxes[i].hovered) {
			color = boxes[i].hoverColor;
		}
		if (boxes[i].pressed) {
			color = boxes[i].pressColor;
		}

		DrawRect(rectGL, screenInfo,
			boxes[i].origin, boxes[i].anchor, boxes[i].size, color);
	}
}

void UpdateButtons(Button buttons[], uint32 n,
	const GameInput* input, void* data)
{
	for (uint32 i = 0; i < n; i++) {
		bool32 wasPressed = buttons[i].box.pressed;
		// TODO unfortunate... SOA would be cool
		UpdateClickableBoxes(&buttons[i].box, 1, input);

		if (buttons[i].box.hovered && wasPressed && !buttons[i].box.pressed) {
			buttons[i].callback(&buttons[i], data);
		}
	}
}

void DrawButtons(Button buttons[], uint32 n,
	RectGL rectGL, TextGL textGL, const FontFace& face,
	ScreenInfo screenInfo, MemoryBlock transient)
{
	for (uint32 i = 0; i < n; i++) {
		DrawClickableBoxes(&buttons[i].box, 1, rectGL, screenInfo);
		Vec2Int pos = buttons[i].box.origin + buttons[i].box.size / 2;
		Vec2 anchor = { 0.5f, 0.5f };
		DrawText(textGL, face, screenInfo,
			buttons[i].text,
			pos, anchor, buttons[i].textColor, transient);
	}
}

void UpdateInputFields(InputField fields[], uint32 n,
	const GameInput* input, void* data)
{
	// TODO this is horribly hacky
	static std::map<uint64, int> focus;
	uint64 fieldsID = (uint64)fields;
	if (focus.find(fieldsID) != focus.end()) {
		focus.insert(std::pair<uint64, int>(fieldsID, -1));
	}

	bool anyPressed = false;
	for (uint32 i = 0; i < n; i++) {
		UpdateClickableBoxes(&fields[i].box, 1, input);
		
		if (fields[i].box.pressed) {
			// TODO picks the last one for now. sort based on Z?
			//LOG_INFO("new input focus: %d\n", i);
			focus[fieldsID] = i;
			anyPressed = true;
		}
	}

	if (focus[fieldsID] != -1 && input->mouseButtons[0].isDown
	&& !anyPressed) {
		//LOG_INFO("lost focus\n");
		focus[fieldsID] = -1;
	}

	// TODO mysterious bug: can't type in more than 16 characters...
	if (focus[fieldsID] != -1 && input->keyboardStringLen != 0) {
		//LOG_INFO("size: %d\n", (int)sizeof(fields[focus]));
		for (uint32 i = 0; i < input->keyboardStringLen; i++) {
			if (input->keyboardString[i] == 8) {
				//LOG_INFO(">> backspaced\n");
				if (fields[focus[fieldsID]].textLen > 0) {
					fields[focus[fieldsID]].textLen--;
				}
			}
			else if (input->keyboardString[i] == 13) {
				//LOG_INFO(">> CR (enter, at last in Windows)\n");
				fields[focus[fieldsID]].callback(&fields[focus[fieldsID]],
					data);
			}
			else if (fields[focus[fieldsID]].textLen < INPUT_BUFFER_SIZE - 1) {
				//LOG_INFO("added %c\n", input->keyboardString[i].ascii);
				//LOG_INFO("textLen before: %d\n", fields[focus].textLen);
				fields[focus[fieldsID]].text[fields[focus[fieldsID]].textLen++]
					= input->keyboardString[i];
			}
		}
		fields[focus[fieldsID]].text[fields[focus[fieldsID]].textLen] = '\0';
		//LOG_INFO("new focus (%d) length: %d\n",
		//    focus, fields[focus].textLen);
		for (uint32 i = 0; i < fields[focus[fieldsID]].textLen; i++) {
			//LOG_INFO("chardump: %c\n", fields[focus].text[i]);
		}
		//LOG_INFO("new text: %s\n", fields[focus].text);
	}
}

void DrawInputFields(InputField fields[], uint32 n,
	RectGL rectGL, TextGL textGL, const FontFace& face,
	ScreenInfo screenInfo, MemoryBlock transient)
{
	for (uint32 i = 0; i < n; i++) {
		DrawClickableBoxes(&fields[i].box, 1, rectGL, screenInfo);
		Vec2Int pos = fields[i].box.origin + fields[i].box.size / 2;
		Vec2 anchor = { 0.5f, 0.5f };
		DrawText(textGL, face, screenInfo,
			fields[i].text,
			pos, anchor, fields[i].textColor, transient);
	}
}
