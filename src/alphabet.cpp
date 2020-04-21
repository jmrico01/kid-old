#include "alphabet.h"

const Array<char> ALPHABET_BIN_PATH = ToString("data/fonts/alphabet.bin");

// We define the extraction-time-computed fields as a "unique" ID for the letter even though it's not
// strictly unique. Unique enough for our purposes.
int CompareLetterUniqueIds(const void* value1, const void* value2)
{
    return MemComp(value1, value2, sizeof(uint16) * 4 + sizeof(uint32) * 1 + sizeof(int) * 1);
}

bool IsLetterUnassigned(const Letter& letter)
{
    return letter.ascii == 0 && !(letter.flags & LetterFlag::IGNORE);
}

template <typename Allocator>
Array<Letter> LoadSavedLetters(Allocator* allocator)
{
    Array<uint8> savedLettersFile = LoadEntireFile(ALPHABET_BIN_PATH, allocator);
    if (savedLettersFile.data == nullptr) {
        LOG_ERROR("Failed to read alphabet bin file: %.*s\n", ALPHABET_BIN_PATH.size, ALPHABET_BIN_PATH.data);
        return { .size = 0, .data = nullptr };
    }
    if (savedLettersFile.size % sizeof(Letter) != 0) {
        LOG_ERROR("Saved letters file mismatch, likely a different struct version\n");
        return { .size = 0, .data = nullptr };
    }
    
    return {
        .size = savedLettersFile.size / sizeof(Letter),
        .data = (Letter*)savedLettersFile.data
    };
}

bool SaveLetters(Array<Letter> letters)
{
    const Array<uint8> lettersData = {
        .size = sizeof(Letter) * letters.size,
        .data = (uint8*)letters.data
    };
    if (!WriteFile(ALPHABET_BIN_PATH, lettersData, false)) {
        LOG_ERROR("Failed to save alphabet letters to file: %.*s\n", ALPHABET_BIN_PATH.size, ALPHABET_BIN_PATH.data);
        return false;
    }
    
    return true;
}

uint64 GetLetterIndOfCharVariation(const Alphabet* alphabet, char c, int variation)
{
    const int targetMatches = variation + 1;
    int matches = 0;
    for (uint64 i = 0; i < alphabet->letters.size; i++) {
        if (!IsLetterUnassigned(alphabet->letters[i]) && alphabet->letters[i].ascii == c) {
            matches++;
            if (matches >= targetMatches) {
                return i;
            }
        }
    }
    
    return alphabet->letters.size;
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
    
    Array<Letter> savedLetters = LoadSavedLetters(&allocator);
    if (savedLetters.data == nullptr) {
        LOG_ERROR("Failed to load saved alphabet letters\n");
        return false;
    }
    
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
    
    if (!SaveLetters(outAlphabet->letters.ToArray())) {
        LOG_ERROR("Failed to save alphabet letters\n");
        return false;
    }
    
    for (int i = 0; i < 256; i++) {
        outAlphabet->numVariations[i] = 0;
    }
    for (uint64 i = 0; i < outAlphabet->letters.size; i++) {
        if (!IsLetterUnassigned(outAlphabet->letters[i])) {
            outAlphabet->numVariations[outAlphabet->letters[i].ascii]++;
        }
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
    
    LOG_INFO("%d total letters\n", outAlphabet->letters.size);
    LOG_INFO("%d sections\n", possibleLetters.size);
    LOG_INFO("%d total previously saved letters\n", savedLetters.size);
    LOG_INFO("%d new letters\n", outAlphabet->letters.size - matchedLetters);
    
    return true;
}

void AlphabetAtlasUpdateAndRender(Alphabet* alphabet, const GameInput& input, MemoryBlock transient,
                                  RectGL rectGL, TexturedRectGL texturedRectGL, TextGL textGL,
                                  ScreenInfo screenInfo, const FontFace& fontText, const FontFace& fontHeader)
{
    static bool panelInputMinimized = true;
    static InputString inputString = {};
    static FixedArray<int, INPUT_BUFFER_MAX> inputCharVariation = {};
    
    LinearAllocator tempAlloc(transient.size, transient.memory);
    
    const Vec2Int MARGIN_SCREEN = { 30, 30 };
    const Vec2Int BORDER_PANEL = { 10, 10 };
    
    const Vec4 COLOR_TEXT = { 0.95f, 0.95f, 0.95f, 1.0f };
    const Vec4 COLOR_BACKGROUND = { 0.0f, 0.0f, 0.0f, 0.5f };
    // TODO for aesthetic purposes, change these highlights to something less intrusive
    const Vec4 COLOR_HIGHLIGHT_UNASSIGNED = Vec4 { 1.0f, 0.2f, 0.2f, 0.3f };
    const Vec4 COLOR_HIGHLIGHT_SELECTED = Vec4 { 1.0f, 0.2f, 1.0f, 0.7f };
    const Vec4 COLOR_HIGHLIGHT_HOVERED = Vec4 { 0.2f, 1.0f, 0.2f, 0.5f };
    
    const bool mousePressed = input.mouseButtons[0].isDown && input.mouseButtons[0].transitions == 1;
    bool mousePressHandled = false;
    
    Panel panelInput;
    panelInput.Begin(input, &fontText, 0,
                     Vec2Int { screenInfo.size.x - MARGIN_SCREEN.x, screenInfo.size.y - MARGIN_SCREEN.y },
                     Vec2 { 1.0f, 1.0f });
    panelInput.TitleBar(ToString("The Typewriter"), &panelInputMinimized, Vec4::zero, &fontHeader);
    
    // For rendering corner panels
    int selectedLetter = -1;
    int hoveredLetter = -1;
    
    if (!panelInputMinimized) {
        static int selectedLetterInd = -1;
        int hoveredLetterInd = -1;
        
        const int SPACE_WIDTH = 80;
        const int NEWLINE_HEIGHT = 200;
        
        // static bool inputStringFocused = false; TODO eventually use this and handle set/reset in InputText
        bool inputStringFocused = true;
        InputString inputStringPrev = inputString;
        if (panelInput.InputText(&inputString, &inputStringFocused)) {
            if (selectedLetterInd >= inputString.size) {
                selectedLetterInd = -1;
            }
            
            inputCharVariation.size = inputString.size;
            for (uint64 i = 0; i < inputString.size; i++) {
                if (i >= inputStringPrev.size || inputString[i] != inputStringPrev[i]) {
                    int numVariations = alphabet->numVariations[inputString[i]];
                    if (numVariations > 0) {
                        inputCharVariation[i] = rand() % numVariations;
                    }
                    else {
                        inputCharVariation[i] = 0;
                    }
                }
            }
        }
        
        Vec2Int cursorPosStart = Vec2Int { MARGIN_SCREEN.x, screenInfo.size.y - NEWLINE_HEIGHT };
        Vec2Int cursorPos = cursorPosStart;
        DrawRect(rectGL, screenInfo, Vec2Int::zero, Vec2::zero, screenInfo.size, Vec4::one);
        for (uint64 i = 0; i < inputString.size; i++) {
            char c = inputString[i];
            if (c == ' ') {
                cursorPos.x += SPACE_WIDTH;
                continue;
            }
            if (c == '\n' || c == '\r') {
                cursorPos.x = cursorPosStart.x;
                cursorPos.y -= NEWLINE_HEIGHT;
                uint64 next = i + 1;
                if (next < inputString.size && c != inputString[next]
                    &&(inputString[next] == '\n' || inputString[next] == '\r')) {
                    i++;
                }
                continue;
            }
            
            const uint64 letterInd = GetLetterIndOfCharVariation(alphabet, c, inputCharVariation[i]);
            if (letterInd != alphabet->letters.size) {
                const Letter& letter = alphabet->letters[letterInd];
                const Vec2Int letterOffset = { letter.offsetX, letter.offsetY };
                const Vec2Int letterSize = { letter.maxX - letter.minX, letter.maxY - letter.minY };
                DrawTexturedRect(texturedRectGL, screenInfo, cursorPos + letterOffset, Vec2::zero, letterSize,
                                 false, false, alphabet->letterTextures[letterInd].textureID);
                
                const RectInt letterRect = {
                    .min = cursorPos,
                    .max = cursorPos + letterSize
                };
                if (IsInside(input.mousePos, letterRect)) {
                    hoveredLetterInd = (int)i;
                    DrawRect(rectGL, screenInfo, cursorPos, Vec2 { 0.0f, 1.0f },
                             Vec2Int { letterSize.x, 10 }, COLOR_HIGHLIGHT_HOVERED);
                }
                
                // NOTE this will lag 1 frame behind the selectedLetterInd on-click change
                if (selectedLetterInd == i) {
                    DrawRect(rectGL, screenInfo, cursorPos, Vec2 { 0.0f, 1.0f },
                             Vec2Int { letterSize.x, 10 }, COLOR_HIGHLIGHT_SELECTED);
                }
                
                cursorPos.x += letterOffset.x + letterSize.x;
            }
        }
        
        if (input.mouseButtons[0].isDown && input.mouseButtons[0].transitions == 1) {
            selectedLetterInd = hoveredLetterInd;
        }
        
        if (selectedLetterInd != -1) {
            char c = inputString[selectedLetterInd];
            if (WasKeyPressed(input, KM_KEY_ARROW_LEFT)) {
                if (inputCharVariation[selectedLetterInd] == alphabet->numVariations[c] - 1) {
                    inputCharVariation[selectedLetterInd] = 0;
                }
                else {
                    inputCharVariation[selectedLetterInd]++;
                }
            }
            if (WasKeyPressed(input, KM_KEY_ARROW_RIGHT)) {
                if (inputCharVariation[selectedLetterInd] == 0) {
                    inputCharVariation[selectedLetterInd] = alphabet->numVariations[c] - 1;
                }
                else {
                    inputCharVariation[selectedLetterInd]--;
                }
            }
        }
        
        // Translate local hovered + selected letter inds to global, alphabet letters array inds
        if (hoveredLetterInd == -1) {
            hoveredLetter = -1;
        }
        else {
            hoveredLetter = (int)GetLetterIndOfCharVariation(alphabet, inputString[hoveredLetterInd],
                                                             inputCharVariation[hoveredLetterInd]);
            DEBUG_ASSERT(hoveredLetter != alphabet->letters.size);
        }
        if (selectedLetterInd == -1) {
            selectedLetter = -1;
        }
        else {
            selectedLetter = (int)GetLetterIndOfCharVariation(alphabet, inputString[selectedLetterInd],
                                                              inputCharVariation[selectedLetterInd]);
            DEBUG_ASSERT(selectedLetter != alphabet->letters.size);
        }
    }
    else {
        static int selectedLetterInd = -1;
        static char selectedChar = 0;
        
        if (WasKeyPressed(input, KM_KEY_BACKSPACE)) {
            selectedChar = 0;
        }
        else if (input.keyboardStringLen > 0) {
            selectedChar = input.keyboardString[input.keyboardStringLen - 1];
        }
        
        const TextureGL& lettersTexture = alphabet->lettersTexture;
        const float32 lettersAspect = (float32)lettersTexture.size.x / lettersTexture.size.y;
        const float32 screenAspect = (float32)screenInfo.size.x / screenInfo.size.y;
        float32 lettersScale;
        if (screenAspect < lettersAspect) {
            lettersScale = (float32)screenInfo.size.x / lettersTexture.size.x;
        }
        else {
            lettersScale = (float32)screenInfo.size.y / lettersTexture.size.y;
        }
        DrawTexturedRect(texturedRectGL, screenInfo, Vec2Int::zero, Vec2::zero,
                         MultiplyVec2IntFloat32(lettersTexture.size, lettersScale),
                         false, false, lettersTexture.textureID);
        
        int selectedCharLetters = 0;
        for (uint64 i = 0; i < alphabet->letters.size; i++) {
            const Letter& letter = alphabet->letters[i];
            bool unassigned = IsLetterUnassigned(letter);
            Vec2Int letterPos = MultiplyVec2IntFloat32(Vec2Int { letter.minX, letter.minY }, lettersScale);
            Vec2Int letterSize = MultiplyVec2IntFloat32(Vec2Int { letter.maxX - letter.minX, letter.maxY - letter.minY },
                                                        lettersScale);
            if (unassigned) {
                DrawRect(rectGL, screenInfo, letterPos, Vec2::zero, letterSize, COLOR_HIGHLIGHT_UNASSIGNED);
            }
            else if (selectedChar != 0 && letter.ascii == selectedChar) {
                selectedCharLetters++;
                DrawRect(rectGL, screenInfo, letterPos, Vec2::zero, letterSize, COLOR_HIGHLIGHT_HOVERED);
            }
            
            const RectInt letterRect = {
                .min = MultiplyVec2IntFloat32(Vec2Int { letter.minX, letter.minY }, lettersScale),
                .max = MultiplyVec2IntFloat32(Vec2Int { letter.maxX, letter.maxY }, lettersScale)
            };
            if (IsInside(input.mousePos, letterRect)) {
                hoveredLetter = (int)i;
                DrawRect(rectGL, screenInfo,
                         MultiplyVec2IntFloat32(Vec2Int { letter.minX, letter.minY }, lettersScale), Vec2::zero,
                         MultiplyVec2IntFloat32(Vec2Int { letter.maxX - letter.minX, letter.maxY - letter.minY },
                                                lettersScale),
                         COLOR_HIGHLIGHT_HOVERED);
            }
        }
        
        if (input.mouseButtons[0].isDown && input.mouseButtons[0].transitions == 1) {
            selectedLetterInd = hoveredLetter;
        }
        
        if (WasKeyPressed(input, KM_KEY_ARROW_LEFT)) {
            for (uint64 i = 0; i < alphabet->letters.size; i++) {
                uint64 ind = (selectedLetterInd - i - 1 + alphabet->letters.size) % alphabet->letters.size;
                bool unassigned = IsLetterUnassigned(alphabet->letters[ind]);
                if ((selectedChar == 0 && unassigned) ||
                    (selectedChar != 0 && !unassigned && selectedChar == alphabet->letters[ind].ascii)) {
                    selectedLetterInd = (int)ind;
                    break;
                }
            }
        }
        if (WasKeyPressed(input, KM_KEY_ARROW_RIGHT)) {
            for (uint64 i = 0; i < alphabet->letters.size; i++) {
                uint64 ind = (selectedLetterInd + i + 1) % alphabet->letters.size;
                bool unassigned = IsLetterUnassigned(alphabet->letters[ind]);
                if ((selectedChar == 0 && unassigned) ||
                    (selectedChar != 0 && !unassigned && selectedChar == alphabet->letters[ind].ascii)) {
                    selectedLetterInd = (int)ind;
                    break;
                }
            }
        }
        
        // Set char / flags on selected letter
        if (selectedLetterInd != -1 && input.keyboardStringLen > 0) {
            int ind = input.keyboardStringLen - 1;
            while (ind >= 0) {
                char c = input.keyboardString[ind];
                if (IsAlphanumeric(c) || c == 8 || c == 9) {
                    break;
                }
                ind--;
            }
            if (ind >= 0) {
                Letter* letter = &alphabet->letters[selectedLetterInd];
                char c = input.keyboardString[ind];
                char asciiPrev = letter->ascii;
                uint8 flagsPrev = letter->flags;
                if (c == 8) {
                    letter->flags |= LetterFlag::IGNORE;
                }
                else if (c == 9) {
                    letter->ascii = 0;
                    letter->flags = 0;
                    
                }
                else {
                    letter->ascii = c;
                }
                if (!SaveLetters(alphabet->letters.ToArray())) {
                    letter->ascii = asciiPrev;
                    letter->flags = flagsPrev;
                }
            }
        }
        
        if (selectedChar != 0) {
            Panel panelSelectedChar;
            panelSelectedChar.Begin(input, &fontText, PanelFlag::GROW_UPWARDS, MARGIN_SCREEN, Vec2::zero);
            panelSelectedChar.Text(AllocPrintf(&tempAlloc, "letters: %d", selectedCharLetters));
            panelSelectedChar.Text(AllocPrintf(&tempAlloc, "'%c' (%d)", selectedChar, selectedChar));
            panelSelectedChar.Draw(screenInfo, rectGL, textGL, BORDER_PANEL, COLOR_TEXT, COLOR_BACKGROUND, &tempAlloc);
        }
        
        if (selectedLetterInd != -1) {
            const Letter& letter = alphabet->letters[selectedLetterInd];
            DrawRect(rectGL, screenInfo,
                     MultiplyVec2IntFloat32(Vec2Int { letter.minX, letter.minY }, lettersScale), Vec2::zero,
                     MultiplyVec2IntFloat32(Vec2Int { letter.maxX - letter.minX, letter.maxY - letter.minY },
                                            lettersScale),
                     COLOR_HIGHLIGHT_SELECTED);
        }
        
        selectedLetter = selectedLetterInd;
    }
    
    // Draw hovered + selected letters
    if (hoveredLetter != -1) {
        const Letter& letter = alphabet->letters[hoveredLetter];
        
        Panel panelLetter;
        panelLetter.Begin(input, &fontText, PanelFlag::GROW_UPWARDS,
                          Vec2Int { MARGIN_SCREEN.x, MARGIN_SCREEN.y },
                          Vec2 { 0.0f, 0.0f });
        panelLetter.Text(AllocPrintf(&tempAlloc, "kernings... maybe later"));
        panelLetter.Text(AllocPrintf(&tempAlloc, "parent: %d", letter.parentIndex));
        panelLetter.Text(AllocPrintf(&tempAlloc, "y-offset: %d", letter.offsetY));
        panelLetter.Text(AllocPrintf(&tempAlloc, "x-offset: %d", letter.offsetX));
        panelLetter.Text(AllocPrintf(&tempAlloc, "flags: %d", letter.flags));
        panelLetter.Text(AllocPrintf(&tempAlloc, "ascii: %d", letter.ascii));
        panelLetter.Text(Array<char>::empty);
        
        panelLetter.Text(AllocPrintf(&tempAlloc, "letter %d: '%c'", hoveredLetter, (char)letter.ascii));
        
        panelLetter.Draw(screenInfo, rectGL, textGL, BORDER_PANEL, COLOR_TEXT, COLOR_BACKGROUND, &tempAlloc);
        
        const int LETTER_FRAME_SIZE = 10;
        Vec2Int letterTexturePos = panelLetter.positionCurrent + Vec2Int { 0, 40 };
        Vec2Int letterTextureSize = alphabet->letterTextures[hoveredLetter].size;
        DrawRect(rectGL, screenInfo,
                 letterTexturePos + Vec2Int { -LETTER_FRAME_SIZE, -LETTER_FRAME_SIZE }, Vec2 { 0.0f, 0.0f },
                 letterTextureSize + Vec2Int { LETTER_FRAME_SIZE, LETTER_FRAME_SIZE } * 2,
                 Vec4::black);
        DrawRect(rectGL, screenInfo, letterTexturePos, Vec2 { 0.0f, 0.0f }, letterTextureSize, Vec4::one);
        DrawTexturedRect(texturedRectGL, screenInfo, letterTexturePos, Vec2 { 0.0f, 0.0f }, letterTextureSize,
                         false, false, alphabet->letterTextures[hoveredLetter].textureID);
    }
    if (selectedLetter != -1) {
        const Letter& letter = alphabet->letters[selectedLetter];
        
        Panel panelLetter;
        panelLetter.Begin(input, &fontText, PanelFlag::GROW_UPWARDS,
                          Vec2Int { screenInfo.size.x - MARGIN_SCREEN.x, MARGIN_SCREEN.y },
                          Vec2 { 1.0f, 0.0f });
        panelLetter.Text(AllocPrintf(&tempAlloc, "kernings... maybe later"));
        panelLetter.Text(AllocPrintf(&tempAlloc, "parent: %d", letter.parentIndex));
        panelLetter.Text(AllocPrintf(&tempAlloc, "y-offset: %d", letter.offsetY));
        panelLetter.Text(AllocPrintf(&tempAlloc, "x-offset: %d", letter.offsetX));
        panelLetter.Text(AllocPrintf(&tempAlloc, "flags: %d", letter.flags));
        panelLetter.Text(AllocPrintf(&tempAlloc, "ascii: %d", letter.ascii));
        static InputString testBuffer;
        static bool testBufferFocused = false;
        panelLetter.InputText(&testBuffer, &testBufferFocused);
        panelLetter.Text(Array<char>::empty);
        
        panelLetter.Text(AllocPrintf(&tempAlloc, "letter %d: '%c'", selectedLetter, (char)letter.ascii));
        
        panelLetter.Draw(screenInfo, rectGL, textGL, BORDER_PANEL, COLOR_TEXT, COLOR_BACKGROUND, &tempAlloc);
        
        const int LETTER_FRAME_SIZE = 10;
        Vec2Int letterTexturePos = panelLetter.positionCurrent + Vec2Int { 0, 40 };
        Vec2Int letterTextureSize = alphabet->letterTextures[selectedLetter].size;
        DrawRect(rectGL, screenInfo,
                 letterTexturePos + Vec2Int { LETTER_FRAME_SIZE, -LETTER_FRAME_SIZE }, Vec2 { 1.0f, 0.0f },
                 letterTextureSize + Vec2Int { LETTER_FRAME_SIZE, LETTER_FRAME_SIZE } * 2,
                 Vec4::black);
        DrawRect(rectGL, screenInfo, letterTexturePos, Vec2 { 1.0f, 0.0f }, letterTextureSize, Vec4::one);
        DrawTexturedRect(texturedRectGL, screenInfo, letterTexturePos, Vec2 { 1.0f, 0.0f }, letterTextureSize,
                         false, false, alphabet->letterTextures[selectedLetter].textureID);
    }
    
    panelInput.Draw(screenInfo, rectGL, textGL, BORDER_PANEL, COLOR_TEXT, COLOR_BACKGROUND, &tempAlloc);
}
