#include "framebuffer.h"

void InitializeFramebuffers(int n, Framebuffer framebuffers[])
{
    for (int i = 0; i < n; i++) {
        glGenFramebuffers(1, &framebuffers[i].framebuffer);
        framebuffers[i].state = FBSTATE_INITIALIZED;
    }
}

void UpdateFramebufferColorAttachments(int n, Framebuffer framebuffers[],
    GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type)
{
    for (int i = 0; i < n; i++) {
        if (framebuffers[i].state == FBSTATE_COLOR
        || framebuffers[i].state == FBSTATE_COLOR_DEPTH) {
            glDeleteTextures(1, &framebuffers[i].color);
        }
        glGenTextures(1, &framebuffers[i].color);

        glBindTexture(GL_TEXTURE_2D, framebuffers[i].color);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
            width, height,
            0,
            format, type,
            NULL
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i].framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, framebuffers[i].color, 0);

        GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("Incomplete framebuffer (%d), status %x\n",
                i, fbStatus);
        }
    }
}

void UpdateFramebufferDepthAttachments(int n, Framebuffer framebuffers[],
    GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type)
{
    for (int i = 0; i < n; i++) {
        if (framebuffers[i].state == FBSTATE_COLOR_DEPTH) {
            glDeleteTextures(1, &framebuffers[i].depth);
        }
        glGenTextures(1, &framebuffers[i].depth);

        glBindTexture(GL_TEXTURE_2D, framebuffers[i].depth);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
            width, height,
            0,
            format, type,
            NULL
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // TODO FIX
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i].framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_TEXTURE_2D, framebuffers[i].depth, 0);

        GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("Incomplete framebuffer (%d), status %x\n",
                i, fbStatus);
        }
    }
}