#pragma once

namespace km_imgui {

const uint64 INPUT_BUFFER_MAX = 256;

typedef FixedArray<char, INPUT_BUFFER_MAX> InputString;

struct Panel
{
	void Begin();

	void Text(const char* text);

	bool ButtonToggle(/*...*/);
	bool ButtonTrigger(/*...*/);

	float32 InputFloat32(/*...*/);
	float32 InputSliderFloat32(/*...*/);
	int InputInt(/*...*/);
	int InputSliderInt(/*...*/);
	Vec2 InputVec2(/*...*/);
	Vec3 InputVec3(/*...*/);
	Vec4 InputVec4(/*...*/);
	Vec4 InputColor(/*...*/);
	void InputText(InputString* outText);

	void End(); // draw here?
};

}