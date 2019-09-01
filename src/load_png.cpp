#include "load_png.h"

#include <km_common/km_debug.h>
#include <km_common/km_log.h>
#undef STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

#include "opengl_funcs.h"

void UnloadTextureGL(const TextureGL& textureGL)
{
	glDeleteTextures(1, &textureGL.textureID);
}

bool LoadPNGOpenGL(const ThreadContext* thread, const char* filePath,
	GLint magFilter, GLint minFilter, GLint wrapS, GLint wrapT,
	TextureGL& outTextureGL, MemoryBlock transient,
	DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
	DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
	outTextureGL.size = Vec2Int { 0, 0 };

	DEBUGReadFileResult pngFile = DEBUGPlatformReadFile(thread, filePath);
	if (!pngFile.data) {
		LOG_ERROR("Failed to open PNG file %s\n", filePath);
		return false;
	}

	int width, height, channels;
	stbi_set_flip_vertically_on_load(true);
	uint8* data = stbi_load_from_memory((const uint8*)pngFile.data, (int)pngFile.size,
		&width, &height, &channels, 0);
	if (data == NULL) {
		LOG_ERROR("Failed to STB load PNG file %s\n", filePath);
		return false;
	}

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

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height,
		0, format, GL_UNSIGNED_BYTE, (const GLvoid*)data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);

	stbi_image_free(data);
	DEBUGPlatformFreeFileMemory(thread, &pngFile);

	outTextureGL.textureID = textureID;
	outTextureGL.size = Vec2Int { width, height };
	return true;
}
