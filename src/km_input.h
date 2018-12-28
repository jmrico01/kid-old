#pragma once

#include "km_defines.h"

enum KeyInputCode
{
    KM_KEY_A = 0,
    KM_KEY_B,
    KM_KEY_C,
    KM_KEY_D,
    KM_KEY_E,
    KM_KEY_F,
    KM_KEY_G,
    KM_KEY_H,
    KM_KEY_I,
    KM_KEY_J,
    KM_KEY_K,
    KM_KEY_L,
    KM_KEY_M,
    KM_KEY_N,
    KM_KEY_O,
    KM_KEY_P,
    KM_KEY_Q,
    KM_KEY_R,
    KM_KEY_S,
    KM_KEY_T,
    KM_KEY_U,
    KM_KEY_V,
    KM_KEY_W,
    KM_KEY_X,
    KM_KEY_Y,
    KM_KEY_Z,
    KM_KEY_SPACE,

    KM_KEY_0,
    KM_KEY_1,
    KM_KEY_2,
    KM_KEY_3,
    KM_KEY_4,
    KM_KEY_5,
    KM_KEY_6,
    KM_KEY_7,
    KM_KEY_8,
    KM_KEY_9,
    KM_KEY_NUMPAD_0,
    KM_KEY_NUMPAD_1,
    KM_KEY_NUMPAD_2,
    KM_KEY_NUMPAD_3,
    KM_KEY_NUMPAD_4,
    KM_KEY_NUMPAD_5,
    KM_KEY_NUMPAD_6,
    KM_KEY_NUMPAD_7,
    KM_KEY_NUMPAD_8,
    KM_KEY_NUMPAD_9,

    KM_KEY_ESCAPE,
    KM_KEY_ENTER,
    KM_KEY_BACKSPACE,
    KM_KEY_TAB,
    KM_KEY_SHIFT,
    KM_KEY_CTRL,

    KM_KEY_ARROW_UP,
    KM_KEY_ARROW_DOWN,
    KM_KEY_ARROW_LEFT,
    KM_KEY_ARROW_RIGHT,

    KM_KEY_LAST // Always keep this at the end.
};

struct GameInput;
inline bool32 IsKeyPressed(const GameInput* input, KeyInputCode keyCode);
inline bool32 WasKeyPressed(const GameInput* input, KeyInputCode keyCode);
inline bool32 WasKeyReleased(const GameInput* input, KeyInputCode keyCode);

void ClearInput(GameInput* input);
void ClearInput(GameInput* input, GameInput* inputPrev);