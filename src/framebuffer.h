#pragma once

#include "opengl.h"

enum FramebufferState
{
    FBSTATE_NONE,        // No setup has been done yet
    FBSTATE_INITIALIZED, // Framebuffer object has been generated
    FBSTATE_COLOR,       // Color attachment created, status is complete
    FBSTATE_COLOR_DEPTH  // Color and depth attachments created, status is complete
};

struct Framebuffer
{
    GLuint framebuffer;
    GLuint color;
    GLuint depth;
    FramebufferState state = FBSTATE_NONE;
};

#include "framebuffer.h"

void InitializeFramebuffers(int n, Framebuffer framebuffers[]);
void UpdateFramebufferColorAttachments(int n, Framebuffer framebuffers[],
    GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type);
void UpdateFramebufferDepthAttachments(int n, Framebuffer framebuffers[],
    GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type);