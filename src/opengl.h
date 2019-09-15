#pragma once

#include <km_common/km_defines.h>

// TODO review these GAME_PLATFORM_CODE things and wonky stuff, especially non-windows
#if defined(GAME_MACOS) && defined(GAME_PLATFORM_CODE)
#include <OpenGL/gl.h>
#endif
#if defined(GAME_LINUX) && defined(GAME_PLATFORM_CODE)
#include <GL/gl.h>
#endif

#if !defined(GAME_PLATFORM_CODE) || defined(GAME_WIN32)

#define GL_FALSE                    0
#define GL_TRUE                     1

#define GL_BYTE                     0x1400
#define GL_UNSIGNED_BYTE            0x1401
#define GL_SHORT                    0x1402
#define GL_UNSIGNED_SHORT           0x1403
#define GL_INT                      0x1404
#define GL_UNSIGNED_INT             0x1405
#define GL_FLOAT                    0x1406
#define GL_DOUBLE                   0x140A

#define GL_POINTS                   0x0000
#define GL_LINES                    0x0001
#define GL_LINE_LOOP                0x0002
#define GL_LINE_STRIP               0x0003
#define GL_TRIANGLES                0x0004
#define GL_TRIANGLE_STRIP           0x0005
#define GL_TRIANGLE_FAN             0x0006

#define GL_DEPTH_BUFFER_BIT         0x00000100
#define GL_STENCIL_BUFFER_BIT       0x00000400
#define GL_COLOR_BUFFER_BIT         0x00004000

#define GL_VENDOR                   0x1F00
#define GL_RENDERER                 0x1F01
#define GL_VERSION                  0x1F02
#define GL_EXTENSIONS               0x1F03
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C

#define GL_FRAGMENT_SHADER          0x8B30
#define GL_VERTEX_SHADER            0x8B31
#define GL_COMPILE_STATUS           0x8B81
#define GL_LINK_STATUS              0x8B82
#define GL_INFO_LOG_LENGTH          0x8B84

#define GL_ARRAY_BUFFER             0x8892
#define GL_ELEMENT_ARRAY_BUFFER     0x8893
#define GL_STREAM_DRAW              0x88E0
#define GL_STREAM_READ              0x88E1
#define GL_STREAM_COPY              0x88E2
#define GL_STATIC_DRAW              0x88E4
#define GL_STATIC_READ              0x88E5
#define GL_STATIC_COPY              0x88E6
#define GL_DYNAMIC_DRAW             0x88E8
#define GL_DYNAMIC_READ             0x88E9
#define GL_DYNAMIC_COPY             0x88EA

#define GL_BLEND                    0x0BE2
#define GL_CULL_FACE                0x0B44
#define GL_CULL_FACE_MODE           0x0B45
#define GL_FRONT_FACE               0x0B46
#define GL_DEPTH_TEST               0x0B71

#define GL_NEVER                    0x0200
#define GL_LESS                     0x0201
#define GL_EQUAL                    0x0202
#define GL_LEQUAL                   0x0203
#define GL_GREATER                  0x0204
#define GL_NOTEQUAL                 0x0205
#define GL_GEQUAL                   0x0206
#define GL_ALWAYS                   0x0207
#define GL_ZERO                     0
#define GL_ONE                      1
#define GL_SRC_COLOR                0x0300
#define GL_ONE_MINUS_SRC_COLOR      0x0301
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303
#define GL_DST_ALPHA                0x0304
#define GL_ONE_MINUS_DST_ALPHA      0x0305
#define GL_DST_COLOR                0x0306
#define GL_ONE_MINUS_DST_COLOR      0x0307

#define GL_TEXTURE_2D               0x0DE1

#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_NEAREST                  0x2600
#define GL_LINEAR                   0x2601
#define GL_NEAREST_MIPMAP_NEAREST   0x2700
#define GL_LINEAR_MIPMAP_NEAREST    0x2701
#define GL_NEAREST_MIPMAP_LINEAR    0x2702
#define GL_LINEAR_MIPMAP_LINEAR     0x2703

#define GL_TEXTURE_WRAP_S           0x2802
#define GL_TEXTURE_WRAP_T           0x2803
#define GL_REPEAT                   0x2901
#define GL_CLAMP_TO_EDGE            0x812F

#define GL_RED                      0x1903
#define GL_GREEN                    0x1904
#define GL_BLUE                     0x1905
#define GL_ALPHA                    0x1906
#define GL_RGB                      0x1907
#define GL_RGBA                     0x1908
#define GL_BGR                      0x80E0
#define GL_BGRA                     0x80E1
#define GL_RGBA32F                  0x8814
#define GL_RGB32F                   0x8815
#define GL_RGBA16F                  0x881A
#define GL_RGB16F                   0x881B

#define GL_TEXTURE0                 0x84C0
#define GL_TEXTURE1                 0x84C1
#define GL_TEXTURE2                 0x84C2
#define GL_TEXTURE3                 0x84C3
#define GL_TEXTURE4                 0x84C4
#define GL_TEXTURE5                 0x84C5
#define GL_TEXTURE6                 0x84C6
#define GL_TEXTURE7                 0x84C7
#define GL_TEXTURE8                 0x84C8
#define GL_TEXTURE9                 0x84C9
#define GL_TEXTURE10                0x84CA
#define GL_TEXTURE11                0x84CB
#define GL_TEXTURE12                0x84CC
#define GL_TEXTURE13                0x84CD
#define GL_TEXTURE14                0x84CE
#define GL_TEXTURE15                0x84CF
#define GL_TEXTURE16                0x84D0
#define GL_TEXTURE17                0x84D1
#define GL_TEXTURE18                0x84D2
#define GL_TEXTURE19                0x84D3
#define GL_TEXTURE20                0x84D4
#define GL_TEXTURE21                0x84D5
#define GL_TEXTURE22                0x84D6
#define GL_TEXTURE23                0x84D7
#define GL_TEXTURE24                0x84D8
#define GL_TEXTURE25                0x84D9
#define GL_TEXTURE26                0x84DA
#define GL_TEXTURE27                0x84DB
#define GL_TEXTURE28                0x84DC
#define GL_TEXTURE29                0x84DD
#define GL_TEXTURE30                0x84DE
#define GL_TEXTURE31                0x84DF

#define GL_UNPACK_ALIGNMENT         0x0CF5
#define GL_PACK_ALIGNMENT           0x0D05

#define GL_FRAMEBUFFER              0x8D40
#define GL_COLOR_ATTACHMENT0        0x8CE0
#define GL_COLOR_ATTACHMENT1        0x8CE1
#define GL_COLOR_ATTACHMENT2        0x8CE2
#define GL_COLOR_ATTACHMENT3        0x8CE3
#define GL_COLOR_ATTACHMENT4        0x8CE4
#define GL_COLOR_ATTACHMENT5        0x8CE5
#define GL_COLOR_ATTACHMENT6        0x8CE6
#define GL_COLOR_ATTACHMENT7        0x8CE7
#define GL_COLOR_ATTACHMENT8        0x8CE8
#define GL_COLOR_ATTACHMENT9        0x8CE9
#define GL_COLOR_ATTACHMENT10       0x8CEA
#define GL_COLOR_ATTACHMENT11       0x8CEB
#define GL_COLOR_ATTACHMENT12       0x8CEC
#define GL_COLOR_ATTACHMENT13       0x8CED
#define GL_COLOR_ATTACHMENT14       0x8CEE
#define GL_COLOR_ATTACHMENT15       0x8CEF
#define GL_COLOR_ATTACHMENT16       0x8CF0
#define GL_COLOR_ATTACHMENT17       0x8CF1
#define GL_COLOR_ATTACHMENT18       0x8CF2
#define GL_COLOR_ATTACHMENT19       0x8CF3
#define GL_COLOR_ATTACHMENT20       0x8CF4
#define GL_COLOR_ATTACHMENT21       0x8CF5
#define GL_COLOR_ATTACHMENT22       0x8CF6
#define GL_COLOR_ATTACHMENT23       0x8CF7
#define GL_COLOR_ATTACHMENT24       0x8CF8
#define GL_COLOR_ATTACHMENT25       0x8CF9
#define GL_COLOR_ATTACHMENT26       0x8CFA
#define GL_COLOR_ATTACHMENT27       0x8CFB
#define GL_COLOR_ATTACHMENT28       0x8CFC
#define GL_COLOR_ATTACHMENT29       0x8CFD
#define GL_COLOR_ATTACHMENT30       0x8CFE
#define GL_COLOR_ATTACHMENT31       0x8CFF
#define GL_DEPTH_ATTACHMENT         0x8D00
#define GL_STENCIL_ATTACHMENT       0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_MAX_RENDERBUFFER_SIZE    0x84E8
#define GL_DEPTH_STENCIL            0x84F9
#define GL_UNSIGNED_INT_24_8        0x84FA
#define GL_DEPTH24_STENCIL8         0x88F0

#define GL_CW                       0x0900
#define GL_CCW                      0x0901

#define GL_FRONT                    0x0404
#define GL_BACK                     0x0405
#define GL_FRONT_AND_BACK           0x0408

#define GL_NO_ERROR                 0
#define GL_FRAMEBUFFER_COMPLETE     0x8CD5

typedef void    GLvoid;

typedef bool    GLboolean;

typedef char    GLchar;
typedef int8    GLbyte;
typedef int16   GLshort;
typedef int32   GLint;
typedef int64   GLint64;

typedef uint8   GLubyte;
typedef uint16  GLushort;
typedef uint32  GLuint;
typedef uint64  GLuint64;

typedef uint32  GLenum;
typedef uint32  GLbitfield;

typedef float32 GLfloat;
typedef float32 GLclampf;
typedef float64 GLdouble;
typedef float64 GLclampd;

typedef uint32  GLsizei;
typedef ptrdiff_t   GLsizeiptr;
typedef ptrdiff_t   GLintptr;

#endif

// X Macro trickery for declaring required OpenGL functions
// The general FUNC macro has the form
//      FUNC( return type, function name, arg1, arg2, arg3, arg4, ... )
// This macro will be used for:
//  - Declaring the functions
//  - Declaring pointers to the functions in struct OpenGLFunctions
//  - Loading the functions in platform layers
//  - More stuff, probably, as time goes on
#define GL_FUNCTIONS_BASE \
    FUNC(void,              glViewport,     GLint x, GLint y, \
                                            GLsizei width, GLsizei height) \
    FUNC(const GLubyte*,    glGetString,    GLenum name) \
    FUNC(void,              glClear,        GLbitfield mask) \
    FUNC(void,              glClearColor,   GLclampf r, GLclampf g, \
                                            GLclampf b, GLclampf a) \
    FUNC(void,              glClearDepth,   GLdouble depth)

#define GL_FUNCTIONS_ALL \
    FUNC(void,  glEnable, GLenum cap) \
    FUNC(void,  glDisable, GLenum cap) \
    FUNC(void,  glBlendFunc, GLenum sfactor, GLenum dfactor) \
    FUNC(void,  glBlendFuncSeparate, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) \
    FUNC(void,  glDepthFunc, GLenum func) \
    FUNC(void,  glDepthRange, GLdouble near, GLdouble far) \
\
    FUNC(GLuint, glCreateShader, GLenum type) \
    FUNC(GLuint, glCreateProgram) \
    FUNC(void,  glShaderSource, GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) \
    FUNC(void,  glCompileShader, GLuint shader) \
    FUNC(void,  glGetShaderiv, GLuint shader, GLenum pname, GLint* params) \
    FUNC(void,  glGetShaderInfoLog, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar* infoLog) \
    FUNC(void,  glGetProgramInfoLog, GLuint program, GLsizei bufSize, GLsizei *length, GLchar* infoLog) \
    FUNC(void,  glAttachShader, GLuint program, GLuint shader) \
    FUNC(void,  glLinkProgram, GLuint program) \
    FUNC(void,  glGetProgramiv, GLuint program, GLenum pname, GLint *params) \
    FUNC(void,  glDetachShader, GLuint program, GLuint shader) \
    FUNC(void,  glDeleteProgram, GLuint program) \
    FUNC(void,  glDeleteShader, GLuint shader) \
\
    FUNC(void,  glGenBuffers, GLsizei n, GLuint* buffers) \
    FUNC(void,  glBindBuffer, GLenum target, GLuint buffer) \
    FUNC(void,  glBufferData, GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) \
    FUNC(void,  glBufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data) \
    FUNC(void,  glBindVertexArray, GLuint array) \
    FUNC(void,  glDeleteVertexArrays, GLsizei n, const GLuint* arrays) \
    FUNC(void,  glGenVertexArrays, GLsizei n, GLuint* arrays) \
    FUNC(void,  glEnableVertexAttribArray, GLuint index) \
    FUNC(void,  glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* pointer) \
    FUNC(void,  glVertexAttribDivisor, GLuint index, GLuint divisor) \
    FUNC(void,  glDeleteBuffers, GLsizei n, const GLuint* buffers) \
\
    FUNC(void,  glUseProgram, GLuint program) \
    FUNC(GLint, glGetUniformLocation, GLuint program, const GLchar* name) \
    FUNC(void,  glUniform1f, GLint location, GLfloat v0) \
    FUNC(void,  glUniform2f, GLint location, GLfloat v0, GLfloat v1) \
    FUNC(void,  glUniform3f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) \
    FUNC(void,  glUniform4f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) \
    FUNC(void,  glUniform1i, GLint location, GLint v0) \
    FUNC(void,  glUniform2i, GLint location, GLint v0, GLint v1) \
    FUNC(void,  glUniform3i, GLint location, GLint v0, GLint v1, GLint v2) \
    FUNC(void,  glUniform4i, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) \
    FUNC(void,  glUniform1fv, GLint location, GLsizei count, const GLfloat *value) \
    FUNC(void,  glUniform2fv, GLint location, GLsizei count, const GLfloat *value) \
    FUNC(void,  glUniform3fv, GLint location, GLsizei count, const GLfloat *value) \
    FUNC(void,  glUniform4fv, GLint location, GLsizei count, const GLfloat *value) \
    FUNC(void,  glUniform1iv, GLint location, GLsizei count, const GLint *value) \
    FUNC(void,  glUniform2iv, GLint location, GLsizei count, const GLint *value) \
    FUNC(void,  glUniform3iv, GLint location, GLsizei count, const GLint *value) \
    FUNC(void,  glUniform4iv, GLint location, GLsizei count, const GLint *value) \
    FUNC(void,  glUniformMatrix2fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) \
    FUNC(void,  glUniformMatrix3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) \
    FUNC(void,  glUniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) \
\
    FUNC(void,  glActiveTexture, GLenum texture) \
    FUNC(void,  glBindTexture, GLenum target, GLuint texture) \
    FUNC(void,  glGenTextures, GLsizei n, GLuint* textures) \
    FUNC(void,  glDeleteTextures, GLsizei n, const GLuint* textures) \
    FUNC(void,  glTexParameteri, GLenum target, GLenum pname, GLint param) \
    FUNC(void,  glTexImage2D, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data) \
    FUNC(void,  glPixelStorei, GLenum pname, GLint param) \
\
    FUNC(void,  glDrawArrays, GLenum mode, GLint first, GLsizei count) \
    FUNC(void,  glDrawElements, GLenum mode, GLsizei count, GLenum type, const void *indices) \
    FUNC(void,  glDrawArraysInstanced, GLenum mode, GLint first, GLsizei count, GLsizei primcount) \
\
    FUNC(void,  glGenFramebuffers, GLsizei n, GLuint* ids) \
    FUNC(void,  glBindFramebuffer, GLenum target, GLuint framebuffer) \
    FUNC(void,  glFramebufferTexture2D, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) \
    FUNC(void,  glDrawBuffers, GLsizei n, const GLenum* bufs) \
\
    FUNC(GLenum, glGetError) \
    FUNC(GLenum, glCheckFramebufferStatus, GLenum target) \
\
    FUNC(void,  glCullFace, GLenum mode) \
    FUNC(void,  glFrontFace, GLenum mode) \
\
    FUNC(void,  glLineWidth, GLfloat width)

// Generate function declarations
#define FUNC(returntype, name, ...) \
    typedef returntype name##Func ( __VA_ARGS__ );
GL_FUNCTIONS_BASE
GL_FUNCTIONS_ALL
#undef FUNC

struct OpenGLFunctions
{
    // Generate function pointers
#define FUNC(returntype, name, ...) name##Func* name;
    GL_FUNCTIONS_BASE
    GL_FUNCTIONS_ALL
#undef FUNC
};