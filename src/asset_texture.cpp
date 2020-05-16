#include "asset_texture.h"

#include <km_common/km_debug.h>
#include <km_common/km_log.h>
#undef STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>

#include "opengl_funcs.h"

bool LoadTexture(const uint8* data, GLint width, GLint height, GLint format,
                 GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT, TextureGL* outTextureGL)
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height,
                 0, format, GL_UNSIGNED_BYTE, (const GLvoid*)data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);

	outTextureGL->textureID = textureID;
	outTextureGL->size = Vec2Int { width, height };
	return true;
}

void UnloadTexture(const TextureGL& textureGL)
{
	glDeleteTextures(1, &textureGL.textureID);
}

template <typename Allocator>
bool LoadTextureFromPng(Allocator* allocator, const_string& filePath,
                        GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT, TextureGL* outTextureGL)
{
	Array<uint8> pngFile = LoadEntireFile(filePath, allocator);
	if (!pngFile.data) {
		LOG_ERROR("Failed to open PNG file %s\n", filePath);
		return false;
	}
	defer (FreeFile(pngFile, allocator));

	int width, height, channels;
	stbi_set_flip_vertically_on_load(true);
	uint8* data = stbi_load_from_memory((const uint8*)pngFile.data, (int)pngFile.size,
                                        &width, &height, &channels, 0);
	if (data == NULL) {
		LOG_ERROR("Failed to STB load PNG file %s\n", filePath);
		return false;
	}
	defer (stbi_image_free(data));

	GLint format;
	switch (channels) {
		case 1: {
			format = GL_RED;
		} break;
		case 3: {
			format = GL_RGB;
		} break;
		case 4: {
			format = GL_RGBA;
		} break;
		default: {
			LOG_ERROR("Unexpected number of channels %d in PNG file %s\n", channels, filePath);
			return false;
		} break;
	}

	return LoadTexture(data, width, height, format, magFilter, minFilter, wrapS, wrapT,
                       outTextureGL);
}
