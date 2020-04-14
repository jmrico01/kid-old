#include "alphabet.h"

// We define the extraction-time-computed fields as a "unique" ID for the letter even though it's not
// strictly unique. Unique enough for our purposes.
int CompareLetterUniqueIds(const void* value1, const void* value2)
{
    return MemComp(value1, value2, sizeof(uint16) * 4 + sizeof(uint32) * 1 + sizeof(int) * 1);
}

bool LoadAlphabet(MemoryBlock memory, Alphabet* outAlphabet)
{
    const Array<char> alphabetPngFile = ToString("data/fonts/alphabet.png");
    LinearAllocator allocator(memory.size, memory.memory);
    
    Array<uint8> pngFile = LoadEntireFile(alphabetPngFile, &allocator);
    if (!pngFile.data) {
        LOG_ERROR("Failed to open alphabet PNG file %.*s\n", alphabetPngFile.size, alphabetPngFile.data);
        return false;
    }
    defer (FreeFile(pngFile, &allocator));
    
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    uint8* data = stbi_load_from_memory((const uint8*)pngFile.data, (int)pngFile.size,
                                        &width, &height, &channels, 0);
    if (data == NULL) {
        LOG_ERROR("Failed to load alphabet PNG file with stb\n");
        return false;
    }
    defer (stbi_image_free(data));
    if (channels != 4) {
        LOG_ERROR("PNG expected 4 channels, got %d\n", channels);
        return false;
    }
    
    if (!LoadTexture(data, width, height, GL_RGBA, GL_LINEAR, GL_LINEAR,
                     GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &outAlphabet->lettersTexture)) {
        DEBUG_PANIC("LoadTexture failed for big letters texture\n");
    }
    
    DynamicArray<Letter> possibleLetters;
    uint64 letterIndsBytes = width * height * sizeof(int);
    int* letterInds = (int*)allocator.Allocate(letterIndsBytes);
    DEBUG_ASSERT(letterInds != nullptr);
    MemSet(letterInds, 0, letterIndsBytes);
    
    DynamicArray<int> indexStack(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int index = y * width + x;
            if (letterInds[index] != 0) {
                continue;
            }
            const bool sectionTransparent = data[index * channels + 3] == 0;
            Letter* newLetter = possibleLetters.Append();
            MemSet(newLetter, 0, sizeof(Letter));
            newLetter->minX = (uint16)(width - 1);
            newLetter->minY = (uint16)(height - 1);
            
            int letterInd = (int)possibleLetters.size;
            newLetter->letterInd = letterInd;
            
            indexStack.Append(index);
            while (indexStack.size > 0) {
                int ind = indexStack[indexStack.size - 1];
                indexStack.RemoveLast();
                
                letterInds[ind] = letterInd;
                if (!sectionTransparent) {
                    newLetter->numPixels++;
                    int x2 = ind % width;
                    int y2 = ind / width;
                    if (x2 < newLetter->minX) {
                        newLetter->minX = (uint16)x2;
                    }
                    if (x2 > newLetter->maxX) {
                        newLetter->maxX = (uint16)x2;
                    }
                    if (y2 < newLetter->minY) {
                        newLetter->minY = (uint16)y2;
                    }
                    if (y2 > newLetter->maxY) {
                        newLetter->maxY = (uint16)y2;
                    }
                }
                const int neighborOffsets[4] = { -width, -1, 1, width };
                for (int i = 0; i < 4; i++) {
                    int neighborInd = ind + neighborOffsets[i];
                    if (neighborInd < 0 || neighborInd >= width * height) {
                        continue;
                    }
                    if (letterInds[neighborInd] != 0) {
                        // TODO if it's letterInd, all good. otherwise, seems fishy
                        continue;
                    }
                    const bool transparent = data[neighborInd * channels + 3] == 0;
                    if (transparent == sectionTransparent) {
                        indexStack.Append(neighborInd);
                    }
                }
            }
        }
    }
    
    const int LETTER_MIN_PIXELS = 50;
    for (uint64 i = 0; i < possibleLetters.size; i++) {
        if (possibleLetters[i].numPixels >= LETTER_MIN_PIXELS) {
            outAlphabet->letters.Append(possibleLetters[i]);
        }
    }
    
    qsort(outAlphabet->letters.data, outAlphabet->letters.size, sizeof(Letter), CompareLetterUniqueIds);
    
    const Array<char> alphabetBinPath = ToString("data/fonts/alphabet.bin");
    Array<uint8> savedLettersFile = LoadEntireFile(alphabetBinPath, &allocator);
    if (savedLettersFile.data == nullptr) {
        LOG_ERROR("Failed to read alphabet bin file: %.*s\n", alphabetBinPath.size, alphabetBinPath.data);
        return false;
    }
    
    DEBUG_ASSERT(savedLettersFile.size % sizeof(Letter) == 0);
    Array<Letter> savedLetters = {
        .size = savedLettersFile.size / sizeof(Letter),
        .data = (Letter*)savedLettersFile.data
    };
    
    uint64 matchedLetters = 0;
    for (uint64 i = 0; i < outAlphabet->letters.size; i++) {
        uint64 j = 0;
        for (; j < savedLetters.size; j++) {
            if (CompareLetterUniqueIds(&outAlphabet->letters[i], &savedLetters[j]) == 0) {
                break;
            }
        }
        if (j != savedLetters.size) {
            matchedLetters++;
            MemCopy(&outAlphabet->letters[i], &savedLetters[j], sizeof(Letter));
        }
    }
    
    const Array<uint8> lettersData = {
        .size = sizeof(Letter) * outAlphabet->letters.size,
        .data = (uint8*)outAlphabet->letters.data
    };
    if (!WriteFile(alphabetBinPath, lettersData, false)) {
        LOG_ERROR("Failed to write alphabet bin file: %.*s\n", alphabetBinPath.size, alphabetBinPath.data);
        return false;
    }
    
    uint8* letterData = (uint8*)allocator.Allocate(width * height * channels);
    for (uint64 i = 0; i < outAlphabet->letters.size; i++) {
        const Letter& letter = outAlphabet->letters[i];
        uint16 letterWidth = letter.maxX - letter.minX;
        uint16 letterHeight = letter.maxY - letter.minY;
        MemSet(letterData, 0, letterWidth * letterHeight * channels);
        for (uint16 y = 0; y < letterHeight; y++) {
            for (uint16 x = 0; x < letterWidth; x++) {
                const int indAlphabet = (y + letter.minY) * width + (x + letter.minX);
                const int indLetter = y * letterWidth + x;
                if (letterInds[indAlphabet] == letter.letterInd) {
                    MemCopy(&letterData[indLetter * channels], &data[indAlphabet * channels], 4);
                }
            }
        }
        
        TextureGL* letterTexture = outAlphabet->letterTextures.Append();
        if (!LoadTexture(letterData, letterWidth, letterHeight, GL_RGBA, GL_LINEAR, GL_LINEAR,
                         GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, letterTexture)) {
            LOG_ERROR("Failed to load texture for letter ind %d\n", i);
            return false;
        }
    }
    
    LOG_INFO("All done!\n");
    LOG_INFO("%d total letters, saved to file %.*s\n", outAlphabet->letters.size,
             alphabetBinPath.size, alphabetBinPath.data);
    LOG_INFO("%d sections\n", possibleLetters.size);
    LOG_INFO("%d total previously saved letters\n", savedLetters.size);
    LOG_INFO("%d new letters\n", outAlphabet->letters.size - matchedLetters);
    
#if 0
    DynamicArray<uint8[3]> sectionColors(possibleLetters.size);
    for (uint64 i = 0; i < possibleLetters.size; i++) {
        uint64 ind = sectionColors.size;
        sectionColors.Append();
        sectionColors[ind][0] = rand() % 256;
        sectionColors[ind][1] = rand() % 256;
        sectionColors[ind][2] = rand() % 256;
    }
    
    MemSet(data, 0, width * height * channels);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int ind = y * width + x;
            data[ind * channels + 3] = 255;
            
            int sectionId = sectionIds[ind];
            DEBUG_ASSERT(sectionId != -1);
            if (possibleLetters[sectionId].numPixels < LETTER_MIN_PIXELS) {
                continue;
            }
            data[ind * channels + 0] = sectionColors[sectionId][0];
            data[ind * channels + 1] = sectionColors[sectionId][1];
            data[ind * channels + 2] = sectionColors[sectionId][2];
        }
    }
    
    if (!LoadTexture(data, width, height, GL_RGBA, GL_NEAREST, GL_NEAREST,
                     GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, &outAlphabet->letterIdsTexture)) {
        DEBUG_PANIC("LoadTexture failed for big letters ids texture\n");
    }
#endif
    
    return true;
}
