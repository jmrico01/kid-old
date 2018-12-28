#include "load_png.h"

#include <png.h>

#include "km_debug.h"
#include "opengl_funcs.h"

struct PNGErrorData {
    const ThreadContext* thread;
    DEBUGReadFileResult* pngFile;
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory;
};
struct PNGDataReadStream {
    png_bytep data;
    int length;
    int readInd;
};

void LoadPNGError(png_structp pngPtr, png_const_charp msg)
{
    DEBUG_PRINT("Load PNG error: %s\n", msg);

    png_voidp errorPtr = png_get_error_ptr(pngPtr);
    if (errorPtr) {
        PNGErrorData* errorData = (PNGErrorData*)errorPtr;
        errorData->DEBUGPlatformFreeFileMemory(errorData->thread,
            errorData->pngFile);
    }
    else {
        DEBUG_PRINT("Load PNG double-error: NO ERROR POINTER!\n");
    }

    // DEBUG_PANIC("IDK what happens now\n");
}
void LoadPNGWarning(png_structp pngPtr, png_const_charp msg)
{
    DEBUG_PRINT("Load PNG warning: %s\n", msg);
}

void LoadPNGReadData(png_structp pngPtr,
    png_bytep outBuffer, png_size_t bytesToRead)
{
    //DEBUG_PRINT("read request: %d\n", (int)bytesToRead);
    png_voidp ioPtr = png_get_io_ptr(pngPtr);
    if (!ioPtr) {
        png_error(pngPtr, "Invalid PNG I/O pointer\n");
    }

    PNGDataReadStream* inputStream = (PNGDataReadStream*)ioPtr;
    // DEBUG_PRINT("input stream: length: %d, readInd: %d\n",
    //     inputStream->length, inputStream->readInd);
    int readInd = inputStream->readInd;
    if (readInd == inputStream->length) {
        png_error(pngPtr, "Read stream empty\n");
    }
    int readLen = (int)bytesToRead;
    if (readInd + readLen > inputStream->length) {
        png_error(pngPtr, "Not enough bytes on read stream\n");
        //readLen = inputStream->length - readInd;
    }
    png_bytep read = inputStream->data + readInd;
    png_bytep write = outBuffer;
    for (int i = 0; i < readLen; i++) {
        *write++ = *read++;
    }
    inputStream->readInd += readLen;
}

// TODO pass a custom allocator to libPNG
GLuint LoadPNGOpenGL(const ThreadContext* thread,
    const char* filePath,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
    DEBUGReadFileResult pngFile = DEBUGPlatformReadFile(thread, filePath);
    if (!pngFile.data) {
        DEBUG_PRINT("Failed to open PNG file at: %s\n", filePath);
        return 0;
    }

    const int headerSize = 8;
    if (png_sig_cmp((png_const_bytep)pngFile.data, 0, headerSize)) {
        DEBUG_PRINT("Invalid PNG file: %s\n", filePath);
        DEBUGPlatformFreeFileMemory(thread, &pngFile);
        return 0;
    }

    PNGErrorData errorData;
    errorData.thread = thread;
    errorData.pngFile = &pngFile;
    errorData.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
    png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
        &errorData, &LoadPNGError, &LoadPNGWarning);
    if (!pngPtr) {
        DEBUG_PRINT("png_create_read_struct failed\n");
        DEBUGPlatformFreeFileMemory(thread, &pngFile);
        return 0;
    }
    png_infop infoPtr = png_create_info_struct(pngPtr);
    if (!infoPtr) {
        DEBUG_PRINT("png_create_info_struct failed\n");
        png_destroy_read_struct(&pngPtr, NULL, NULL);
        DEBUGPlatformFreeFileMemory(thread, &pngFile);
        return 0;
    }

    PNGDataReadStream inputStream;
    inputStream.data = (png_bytep)pngFile.data;
    inputStream.length = (int)pngFile.size;
    inputStream.readInd = headerSize;
    png_set_read_fn(pngPtr, &inputStream, &LoadPNGReadData);
    png_set_sig_bytes(pngPtr, headerSize);

    png_read_info(pngPtr, infoPtr);
    png_uint_32 pngWidth, pngHeight;
    int bitDepth, colorType, interlaceMethod;
    png_get_IHDR(pngPtr, infoPtr, &pngWidth, &pngHeight, &bitDepth,
        &colorType, &interlaceMethod, NULL, NULL);
    int width = (int)pngWidth;
    int height = (int)pngHeight;
    //DEBUG_PRINT("image dimensions: %d x %d\n", width, height);
    if (bitDepth != 8) {
        DEBUG_PRINT("Unsupported bit depth: %d\n", bitDepth);
        png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
        DEBUGPlatformFreeFileMemory(thread, &pngFile);
        return 0;
    }
    GLint format;
    switch (colorType) {
        case PNG_COLOR_TYPE_GRAY: {
            format = GL_RED;
        } break;
        case PNG_COLOR_TYPE_RGB: {
            format = GL_RGB;
        } break;
        case PNG_COLOR_TYPE_RGB_ALPHA: {
            format = GL_RGBA;
        } break;

        default: {
            DEBUG_PRINT("Unsupported color type: %d\n", colorType);
            png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
            DEBUGPlatformFreeFileMemory(thread, &pngFile);
            return 0;
        }
    }
    if (interlaceMethod != PNG_INTERLACE_NONE) {
        DEBUG_PRINT("Unsupported interlace method\n");
        png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
        DEBUGPlatformFreeFileMemory(thread, &pngFile);
        return 0;
    }

    png_read_update_info(pngPtr, infoPtr);
    int rowBytes = (int)png_get_rowbytes(pngPtr, infoPtr);
    rowBytes += 3 - ((rowBytes - 1) % 4); // 4-byte align

    // TODO remove this malloc
    // This section of code borrowed from:
    // https://github.com/DavidEGrayson/ahrs-visualizer/blob/master/png_texture.cpp
    png_byte* data = (png_byte*)malloc(rowBytes * height
        * sizeof(png_byte) + 15); // TODO what are these 15 bytes?
    if (!data) {
        DEBUG_PRINT("Load PNG image data memory allocation failed\n");
        png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
        DEBUGPlatformFreeFileMemory(thread, &pngFile);
        return 0;
    }
    png_byte** rowPtrs = (png_byte**)malloc(height * sizeof(png_byte*));
    if (!rowPtrs) {
        DEBUG_PRINT("Load PNG row pointers memory allocation failed\n");
        png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
        DEBUGPlatformFreeFileMemory(thread, &pngFile);
        return 0;
    }
    for (int i = 0; i < height; i++) {
        rowPtrs[height - 1 - i] = data + i * rowBytes;
    }

    png_read_image(pngPtr, rowPtrs);

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height,
        0, format, GL_UNSIGNED_BYTE, (const GLvoid*)data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    png_destroy_read_struct(&pngPtr, &infoPtr, NULL);
    DEBUGPlatformFreeFileMemory(thread, &pngFile);
    return textureID;
}