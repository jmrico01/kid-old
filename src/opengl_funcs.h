#pragma once

// This header file grabs all OpenGL functions defined in opengl.h
// and defines them in global scope.
// DO NOT include this in any platform layer,
// this should only be used by game code.

#include "opengl.h"

// TODO maybe there's a better way of doing this...
#define FUNC(returntype, name, ...) global_var name##Func* name;
    GL_FUNCTIONS_BASE
    GL_FUNCTIONS_ALL
#undef FUNC