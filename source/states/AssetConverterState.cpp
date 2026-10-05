#include "AssetConverterState.hpp"
#include "ModsMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "AdpcmEncoder.hpp"
#include "../backend/codecs/AdpcmDecoder.hpp"
#include <sys/stat.h>
#include <algorithm>
#include <cmath>
#include <tremor/ivorbisfile.h>
#include "../backend/stb_image.h"

static size_t conv_vorbis_read_cb(void* ptr, size_t size, size_t nmemb, void* datasource) {
    return fread(ptr, size, nmemb, (FILE*)datasource);
}

static int conv_vorbis_seek_cb(void* datasource, ogg_int64_t offset, int whence) {
    return fseek((FILE*)datasource, (long)offset, whence);
}

static int conv_vorbis_close_cb(void* datasource) {
    return fclose((FILE*)datasource);
}

static long conv_vorbis_tell_cb(void* datasource) {
    return ftell((FILE*)datasource);
}

static ov_callbacks s_convVorbisCallbacks = {
    conv_vorbis_read_cb,
    conv_vorbis_seek_cb,
    conv_vorbis_close_cb,
    conv_vorbis_tell_cb
};

struct RawTexHeader {
    char magic[4];
    uint16_t width;
    uint16_t height;
    uint16_t origW;
    uint16_t origH;
};

// Helper function to scale coordinates in Sparrow XML files when images are resized
static void scaleXmlFile3DS(const std::string& xmlPath, float scaleFactor) {
    FILE* f = fopen(xmlPath.c_str(), "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return; }

    std::string content(size, '\0');
    fread(&content[0], 1, size, f);
    fclose(f);

    const char* attrs[] = {"x=\"", "y=\"", "width=\"", "height=\"", "frameX=\"", "frameY=\"", "frameWidth=\"", "frameHeight=\""};
    std::string newContent;
    newContent.reserve(content.size());
    
    size_t pos = 0;
    while (pos < content.size()) {
        bool matched = false;
        for (const char* attr : attrs) {
            size_t attrLen = strlen(attr);
            if (pos + attrLen <= content.size() && content.compare(pos, attrLen, attr) == 0) {
                newContent += attr;
                pos += attrLen;
                size_t valEnd = content.find('"', pos);
                if (valEnd != std::string::npos) {
                    std::string valStr = content.substr(pos, valEnd - pos);
                    char* endPtr = nullptr;
                    float val = strtof(valStr.c_str(), &endPtr);
                    if (endPtr != valStr.c_str()) {
                        int newVal = (int)roundf(val * scaleFactor);
                        newContent += std::to_string(newVal);
                    } else {
                        newContent += valStr;
                    }
                    pos = valEnd;
                }
                matched = true;
                break;
            }
        }
        if (!matched) {
            newContent += content[pos++];
        }
    }

    FILE* outF = fopen(xmlPath.c_str(), "wb");
    if (outF) {
        fwrite(newContent.data(), 1, newContent.size(), outF);
        fclose(outF);
    }
}

AssetConverterState::AssetConverterState(const std::string& startDir, bool audioOnly) {
    rootDir = startDir;
    if (rootDir.back() != '/') rootDir += '/';
    currentDir = rootDir;
    isAudioMode = audioOnly;
}

AssetConverterState::~AssetConverterState() {
    closeImagePreview();
}

void AssetConverterState::init() {
    vcrFont = globalVCRFont;
    if (!vcrFont) {
        vcrFont = C2D_FontLoad("romfs:/fonts/vcr.bcfnt");
    }
    if (!converterTextBuf) {
        converterTextBuf = C2D_TextBufNew(8192);
    }
    
    // Load cached UI assets ONCE in init() to avoid frame memory leaks
    sheetIcons = SpritesheetCache::get().load("preload/images/menus/fileIcons");
    menuBG = SpritesheetCache::get().load("shared/images/menuBG");

    if (sheetIcons) {
        for (int i = 0; i < (int)sheetIcons->frames.size(); i++) {
            auto& name = sheetIcons->frames[i].name;
            if (name == "folder") iconFolder = i;
            else if (name == "image") iconImage = i;
            else if (name == "json") iconJson = i;
            else if (name == "lua") iconLua = i;
            else if (name == "sound") iconSound = i;
            else if (name == "txt") iconTxt = i;
            else if (name == "unknow") iconUnknow = i;
            else if (name == "video") iconVideo = i;
            else if (name == "xml") iconXml = i;
        }
    }
    
    loadDirectory(currentDir);
}

void AssetConverterState::drawText(const std::string& text, float x, float y, float scale, bool centered, u32 color, float depth, float maxWidth, bool rightAlign) {
    if (!vcrFont || !converterTextBuf || text.empty()) return;
    C2D_Text gText;
    C2D_TextFontParse(&gText, vcrFont, converterTextBuf, text.c_str());
    C2D_TextOptimize(&gText);
    float tw = 0.0f, th = 0.0f;
    C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
    float actualScale = scale;
    if (maxWidth > 0.0f && tw > maxWidth) {
        actualScale *= (maxWidth / tw);
        C2D_TextGetDimensions(&gText, actualScale, actualScale, &tw, &th);
    }
    float dx = x;
    if (rightAlign) {
        dx = x - tw;
    } else if (centered) {
        dx = x - tw * 0.5f;
    }
    float dy = centered ? (y - th * 0.5f) : y;
    dx = std::round(dx);
    dy = std::round(dy);
    C2D_DrawText(&gText, C2D_WithColor, dx, dy, depth, actualScale, actualScale, color);
}

void AssetConverterState::drawStyledButton(float x, float y, float w, float h, const std::string& title, const std::string& subtitle, u32 topCol, u32 botCol, u32 borderCol) {
    C2D_DrawRectangle(x, y, 0.5f, w, h, topCol, topCol, botCol, botCol);
    C2D_DrawLine(x, y, borderCol, x + w, y, borderCol, 1.0f, 0.52f);
    C2D_DrawLine(x, y, borderCol, x, y + h, borderCol, 1.0f, 0.52f);
    C2D_DrawLine(x + w, y, borderCol, x + w, y + h, borderCol, 1.0f, 0.52f);
    C2D_DrawLine(x, y + h, borderCol, x + w, y + h, borderCol, 1.0f, 0.52f);

    if (subtitle.empty()) {
        drawText(title, x + w * 0.5f, y + h * 0.5f, 0.40f, true, CWhite, 0.85f, w - 8.0f);
    } else {
        drawText(title, x + w * 0.5f, y + h * 0.33f, 0.38f, true, CWhite, 0.85f, w - 8.0f);
        drawText(subtitle, x + w * 0.5f, y + h * 0.72f, 0.30f, true, C2D_Color32(0, 210, 255, 255), 0.85f, w - 8.0f);
    }
}

void AssetConverterState::loadDirectory(const std::string& path) {
    files.clear();
    curSelected = 0;
    lerpSelected = 0;

    DIR* dir = opendir(path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;

            FileEntry f;
            f.name = name;
            f.isSelected = false;
            f.imageFormat = globalFormat;
            f.audioHz = 0;
            
            auto it = customSettings.find(path + name);
            if (it != customSettings.end()) {
                f.imageFormat = it->second.first;
                f.audioHz = it->second.second;
            }

            std::string fullPath = path + name;
            bool isDir = (entry->d_type == DT_DIR);
            if (entry->d_type == DT_UNKNOWN) {
                struct stat st;
                if (stat(fullPath.c_str(), &st) == 0) {
                    isDir = S_ISDIR(st.st_mode);
                }
            }
            f.isDirectory = isDir;
            f.isConvertible = isImageFile(f.name, f.isDirectory);
            f.iconIndex = getIconForFile(f.name, f.isDirectory);
            files.push_back(f);
        }
        closedir(dir);
    }

    std::sort(files.begin(), files.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
        return a.name < b.name;
    });
}

bool AssetConverterState::isImageFile(const std::string& name, bool isDir) {
    if (isDir) return false;
    std::string lower = name;
    for (char& c : lower) c = (char)tolower((unsigned char)c);
    
    if (isAudioMode) {
        return (lower.find(".ogg") != std::string::npos ||
                lower.find(".adp") != std::string::npos ||
                lower.find(".wav") != std::string::npos);
    } else {
        return (lower.find(".png") != std::string::npos ||
                lower.find(".jpg") != std::string::npos ||
                lower.find(".jpeg") != std::string::npos ||
                lower.find(".rawtex") != std::string::npos ||
                lower.find(".t3x") != std::string::npos);
    }
}

void AssetConverterState::selectAll(bool select) {
    for (auto& f : files) {
        if (f.isConvertible) {
            f.isSelected = select;
        }
    }
}

int AssetConverterState::getIconForFile(const std::string& name, bool isDir) {
    if (isDir) return iconFolder;
    std::string lower = name;
    for (char& c : lower) c = (char)tolower((unsigned char)c);

    if (lower.find(".png") != std::string::npos || lower.find(".jpg") != std::string::npos || lower.find(".jpeg") != std::string::npos || lower.find(".rawtex") != std::string::npos || lower.find(".t3x") != std::string::npos) return iconImage;
    if (lower.find(".ogg") != std::string::npos || lower.find(".adp") != std::string::npos || lower.find(".wav") != std::string::npos) return iconSound;
    if (lower.find(".json") != std::string::npos) return iconJson;
    if (lower.find(".xml") != std::string::npos) return iconXml;
    if (lower.find(".lua") != std::string::npos) return iconLua;
    if (lower.find(".txt") != std::string::npos) return iconTxt;
    if (lower.find(".snaky") != std::string::npos || lower.find(".mp4") != std::string::npos) return iconVideo;
    return iconUnknow;
}

void AssetConverterState::closeImagePreview() {
    isPreviewing = false;
    previewFileName = "";
    stopAudioSample();
    if (viewSheet) {
        C2D_SpriteSheetFree(viewSheet);
        viewSheet = nullptr;
    }
    if (viewTex) {
        C3D_TexDelete(viewTex);
        delete viewTex;
        viewTex = nullptr;
    }
    if (viewSubtex) {
        delete viewSubtex;
        viewSubtex = nullptr;
    }
    viewImg = {nullptr, nullptr};
    previewW = 0;
    previewH = 0;
}

void AssetConverterState::openImagePreview(const std::string& fullPath, const std::string& fileName) {
    closeImagePreview();

    std::string lower = fileName;
    for (char& c : lower) c = (char)tolower((unsigned char)c);

    isPreviewing = true;
    previewFileName = fileName;
    previewPanX = 0.0f;
    previewPanY = 0.0f;
    previewZoom = 1.0f;

    if (lower.find(".ogg") != std::string::npos || lower.find(".adp") != std::string::npos || lower.find(".wav") != std::string::npos) {
        playAudioSample(fullPath, globalFormat);
    } else if (lower.find(".t3x") != std::string::npos) {
        viewSheet = C2D_SpriteSheetLoad(fullPath.c_str());
        if (viewSheet) {
            viewImg = C2D_SpriteSheetGetImage(viewSheet, 0);
            previewW = viewImg.subtex->width;
            previewH = viewImg.subtex->height;
        }
    } else if (lower.find(".rawtex") != std::string::npos) {
        FILE* f = fopen(fullPath.c_str(), "rb");
        if (f) {
            RawTexHeader header;
            if (fread(&header, sizeof(RawTexHeader), 1, f) == 1) {
                GPU_TEXCOLOR fmt = GPU_RGBA8;
                int bytesPerPixel = 4;
                if (memcmp(header.magic, "RWT4", 4) == 0) { fmt = GPU_RGBA4; bytesPerPixel = 2; }
                else if (memcmp(header.magic, "RWT5", 4) == 0) { fmt = GPU_RGB565; bytesPerPixel = 2; }

                viewTex = new C3D_Tex();
                if (C3D_TexInit(viewTex, header.width, header.height, fmt)) {
                    C3D_TexSetFilter(viewTex, GPU_LINEAR, GPU_LINEAR);
                    size_t dataSize = (size_t)header.width * header.height * bytesPerPixel;
                    void* swizzled = linearAlloc(dataSize);
                    if (swizzled) {
                        fread(swizzled, 1, dataSize, f);
                        C3D_TexUpload(viewTex, swizzled);
                        C3D_TexFlush(viewTex);
                        linearFree(swizzled);

                        viewSubtex = new Tex3DS_SubTexture();
                        viewSubtex->width = header.origW; viewSubtex->height = header.origH;
                        viewSubtex->left = 0.0f; viewSubtex->top = 1.0f;
                        viewSubtex->right = (float)header.origW / header.width;
                        viewSubtex->bottom = 1.0f - ((float)header.origH / header.height);

                        viewImg = {viewTex, viewSubtex};
                        previewW = header.origW;
                        previewH = header.origH;
                    }
                }
            }
            fclose(f);
        }
    } else if (lower.find(".png") != std::string::npos || lower.find(".jpg") != std::string::npos || lower.find(".jpeg") != std::string::npos) {
        FILE* f = fopen(fullPath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = ftell(f);
            fseek(f, 0, SEEK_SET);
            unsigned char* fileData = (unsigned char*)malloc(size);
            if (fileData) {
                fread(fileData, 1, size, f);
                fclose(f);

                int w = 0, h = 0, c = 0;
                unsigned char* data = stbi_load_from_memory(fileData, size, &w, &h, &c, 4);
                free(fileData);

                if (data) {
                    int maxDim = std::max(w, h);
                    float scale = 1.0f;
                    int scaledW = w;
                    int scaledH = h;
                    if (maxDim > 1024) {
                        scale = 1024.0f / maxDim;
                        scaledW = (int)(w * scale);
                        scaledH = (int)(h * scale);
                    }
                    int pw = 1, ph = 1;
                    while (pw < scaledW) pw *= 2;
                    while (ph < scaledH) ph *= 2;

                    viewTex = new C3D_Tex();
                    if (C3D_TexInit(viewTex, pw, ph, GPU_RGBA8)) {
                        C3D_TexSetFilter(viewTex, GPU_LINEAR, GPU_LINEAR);
                        uint32_t* swizzled = (uint32_t*)linearAlloc(pw * ph * 4);
                        if (swizzled) {
                            memset(swizzled, 0, pw * ph * 4);
                            for (int y = 0; y < scaledH; y++) {
                                for (int x = 0; x < scaledW; x++) {
                                    int origX = (int)(x / scale);
                                    int origY = (int)(y / scale);
                                    if (origX >= w) origX = w - 1;
                                    if (origY >= h) origY = h - 1;

                                    int src = (origY * w + origX) * 4;
                                    uint32_t px = (data[src] << 24) | (data[src+1] << 16) | (data[src+2] << 8) | data[src+3];

                                    uint32_t i = (x & 7) | ((y & 7) << 8);
                                    i = (i ^ (i << 2)) & 0x1313; i = (i ^ (i << 1)) & 0x1515;
                                    uint32_t tx = x >> 3; uint32_t ty = y >> 3;
                                    uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                                    uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);

                                    swizzled[tile_start + local_idx] = px;
                                }
                            }
                            C3D_TexUpload(viewTex, swizzled);
                            C3D_TexFlush(viewTex);
                            linearFree(swizzled);

                            viewSubtex = new Tex3DS_SubTexture();
                            viewSubtex->width = scaledW; viewSubtex->height = scaledH;
                            viewSubtex->left = 0.0f; viewSubtex->top = 1.0f;
                            viewSubtex->right = (float)scaledW / pw;
                            viewSubtex->bottom = 1.0f - ((float)scaledH / ph);

                            viewImg = {viewTex, viewSubtex};
                            previewW = w;
                            previewH = h;
                        }
                    }
                    stbi_image_free(data);
                }
            } else {
                fclose(f);
            }
        }
    }
}

void AssetConverterState::openAudioPreview(const std::string& fullPath, const std::string& fileName) {
    closeAudioPreview();
    isAudioPreviewing = true;
    audioPreviewFileName = fileName;
    audioPreviewFullPath = fullPath;
    audioAnimTimer = 0.0f;
    playAudioSample(fullPath, globalFormat);
}

void AssetConverterState::closeAudioPreview() {
    isAudioPreviewing = false;
    audioPreviewFileName = "";
    audioPreviewFullPath = "";
    stopAudioSample();
}

void AssetConverterState::update(float dt) {
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    touchPosition touch;
    hidTouchRead(&touch);

    if (isConverting) {
        processConversion();
        return;
    }

    if (isAudioPreviewing) {
        audioAnimTimer += dt;
        if (kDown & KEY_B) {
            closeAudioPreview();
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            return;
        }

        if (kDown & (KEY_A | KEY_SELECT)) {
            playAudioSample(audioPreviewFullPath, globalFormat);
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        if (kDown & (KEY_X | KEY_Y | KEY_R)) {
            startConversionMode(ConversionMode::SINGLE);
            return;
        }

        if (kDown & KEY_TOUCH) {
            int tx = touch.px;
            int ty = touch.py;
            if (tx >= 10 && tx <= 310 && ty >= 112 && ty <= 146) {
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                startConversionMode(ConversionMode::SINGLE);
                return;
            }
            if (tx >= 10 && tx <= 310 && ty >= 152 && ty <= 186) {
                playAudioSample(audioPreviewFullPath, globalFormat);
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
                return;
            }
            if (tx >= 10 && tx <= 310 && ty >= 192 && ty <= 226) {
                closeAudioPreview();
                AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
                return;
            }
        }
        return;
    }

    if (isPreviewing) {
        if (kDown & KEY_B) {
            closeImagePreview();
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            return;
        }

        if (kHeld & (KEY_DUP | KEY_CPAD_UP)) previewPanY += 120.0f * dt;
        if (kHeld & (KEY_DDOWN | KEY_CPAD_DOWN)) previewPanY -= 120.0f * dt;
        if (kHeld & (KEY_DLEFT | KEY_CPAD_LEFT)) previewPanX += 120.0f * dt;
        if (kHeld & (KEY_DRIGHT | KEY_CPAD_RIGHT)) previewPanX -= 120.0f * dt;

        if (kHeld & KEY_L) { previewZoom -= 1.0f * dt; if (previewZoom < 0.2f) previewZoom = 0.2f; }
        if (kHeld & KEY_R) { previewZoom += 1.0f * dt; if (previewZoom > 5.0f) previewZoom = 5.0f; }
        if (kDown & KEY_SELECT) { previewPanX = 0; previewPanY = 0; previewZoom = 1.0f; }

        if (kDown & (KEY_X | KEY_Y)) {
            closeImagePreview();
            startConversionMode(ConversionMode::SINGLE);
            return;
        }

        if (kDown & KEY_TOUCH) {
            int tx = touch.px;
            int ty = touch.py;
            if (tx >= 10 && tx <= 310 && ty >= 112 && ty <= 146) {
                closeImagePreview();
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                startConversionMode(ConversionMode::SINGLE);
                return;
            }
            if (tx >= 10 && tx <= 310 && ty >= 152 && ty <= 186) {
                previewPanX = 0; previewPanY = 0; previewZoom = 1.0f;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
                return;
            }
            if (tx >= 10 && tx <= 310 && ty >= 192 && ty <= 226) {
                closeImagePreview();
                AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
                return;
            }
        }
        return;
    }

    if (kDown & KEY_TOUCH) {
        int tx = touch.px;
        int ty = touch.py;

        if (tx >= 8 && tx <= 156 && ty >= 28 && ty <= 64) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            startConversionMode(ConversionMode::ALL);
            return;
        }
        if (tx >= 164 && tx <= 312 && ty >= 28 && ty <= 64) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            startConversionMode(ConversionMode::FOLDER);
            return;
        }
        if (tx >= 8 && tx <= 156 && ty >= 70 && ty <= 106) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            startConversionMode(ConversionMode::SELECTED);
            return;
        }
        if (tx >= 164 && tx <= 312 && ty >= 70 && ty <= 106) {
            if (!files.empty() && files[curSelected].isConvertible) {
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                startConversionMode(ConversionMode::SINGLE);
            }
            return;
        }
        if (tx >= 8 && tx <= 156 && ty >= 112 && ty <= 146) {
            if (!files.empty() && files[curSelected].isConvertible) {
                files[curSelected].isSelected = !files[curSelected].isSelected;
                AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            }
            return;
        }
        if (tx >= 164 && tx <= 312 && ty >= 112 && ty <= 146) {
            bool anyUnselected = false;
            for (auto& f : files) {
                if (f.isConvertible && !f.isSelected) {
                    anyUnselected = true;
                    break;
                }
            }
            selectAll(anyUnselected);
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            return;
        }
        if (tx >= 8 && tx <= 312 && ty >= 152 && ty <= 186) {
            globalFormat = (globalFormat + 1) % 3;
            for (auto& f : files) f.imageFormat = globalFormat;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            return;
        }
        if (tx >= 8 && tx <= 312 && ty >= 192 && ty <= 228) {
            if (currentDir == rootDir) {
                switchState(new ModsMenuState());
            } else {
                std::string path = currentDir;
                path.pop_back();
                size_t lastSlash = path.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    currentDir = path.substr(0, lastSlash + 1);
                    loadDirectory(currentDir);
                }
            }
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            return;
        }
    }

    if (kDown & KEY_A) {
        if (!files.empty()) {
            if (files[curSelected].isDirectory) {
                currentDir += files[curSelected].name + "/";
                loadDirectory(currentDir);
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            } else if (files[curSelected].isConvertible) {
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                if (isAudioMode) {
                    openAudioPreview(currentDir + files[curSelected].name, files[curSelected].name);
                } else {
                    openImagePreview(currentDir + files[curSelected].name, files[curSelected].name);
                }
            }
        }
    }

    if (kDown & KEY_Y) {
        startConversionMode(ConversionMode::FOLDER);
        return;
    }

    if (kDown & KEY_X) {
        startConversionMode(ConversionMode::SELECTED);
        return;
    }

    if (kDown & KEY_R) {
        if (!files.empty() && files[curSelected].isConvertible) {
            startConversionMode(ConversionMode::SINGLE);
        }
        return;
    }

    if (kDown & KEY_L) {
        if (!files.empty() && files[curSelected].isConvertible) {
            files[curSelected].isSelected = !files[curSelected].isSelected;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
    }

    if (kDown & KEY_SELECT) {
        bool anyUnselected = false;
        for (auto& f : files) {
            if (f.isConvertible && !f.isSelected) {
                anyUnselected = true;
                break;
            }
        }
        selectAll(anyUnselected);
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }

    if (!files.empty()) {
        if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
            curSelected--;
            if (curSelected < 0) curSelected = (int)files.size() - 1;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            curSelected++;
            if (curSelected >= (int)files.size()) curSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
    }

    if (keyJustPressed(KEY_B)) {
        if (currentDir == rootDir) {
            switchState(new ModsMenuState());
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        } else {
            std::string path = currentDir;
            path.pop_back();
            size_t lastSlash = path.find_last_of('/');
            if (lastSlash != std::string::npos) {
                currentDir = path.substr(0, lastSlash + 1);
                loadDirectory(currentDir);
                AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            }
        }
    }

    lerpSelected += (curSelected - lerpSelected) * (1.0f - exp2f(-10.0f * dt));
}

void AssetConverterState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    if (converterTextBuf) {
        C2D_TextBufClear(converterTextBuf);
    }

    if (isAudioPreviewing) {
        C2D_SceneBegin(top);
        C2D_TargetClear(top, C2D_Color32(16, 18, 26, 255));

        if (menuBG && !menuBG->frames.empty()) {
            Frame& f = menuBG->frames[0];
            C2D_ImageTint tint; C2D_PlainImageTint(&tint, C2D_Color32(40, 45, 65, 255), 1.0f);
            drawFrameAt(f, 0, 0, 0.1f, &tint);
        }

        float cardX = 30.0f, cardY = 32.0f, cardW = 340.0f, cardH = 176.0f;
        C2D_DrawRectSolid(cardX, cardY, 0.3f, cardW, cardH, C2D_Color32(22, 24, 34, 245));
        C2D_DrawLine(cardX, cardY, C2D_Color32(0, 180, 230, 255), cardX + cardW, cardY, C2D_Color32(0, 180, 230, 255), 1.0f, 0.32f);
        C2D_DrawLine(cardX, cardY, C2D_Color32(0, 180, 230, 255), cardX, cardY + cardH, C2D_Color32(0, 180, 230, 255), 1.0f, 0.32f);
        C2D_DrawLine(cardX + cardW, cardY, C2D_Color32(0, 180, 230, 255), cardX + cardW, cardY + cardH, C2D_Color32(0, 180, 230, 255), 1.0f, 0.32f);
        C2D_DrawLine(cardX, cardY + cardH, C2D_Color32(0, 180, 230, 255), cardX + cardW, cardY + cardH, C2D_Color32(0, 180, 230, 255), 1.0f, 0.32f);

        C2D_DrawRectSolid(cardX, cardY, 0.35f, cardW, 24, C2D_Color32(30, 36, 50, 255));
        C2D_DrawRectSolid(cardX, cardY + 23, 0.36f, cardW, 1, C2D_Color32(0, 210, 255, 255));
        drawText("AUDIO PLAYER & REALTIME FREQUENCY ANALYZER", cardX + cardW * 0.5f, cardY + 12.0f, 0.38f, true, CWhite, 0.85f);

        drawText(audioPreviewFileName, cardX + cardW * 0.5f, cardY + 42.0f, 0.46f, true, CYellow, 0.85f, cardW - 20.0f);

        float scopeX = cardX + 15.0f;
        float scopeW = cardW - 30.0f;
        float scopeY = cardY + 56.0f;
        float scopeH = 110.0f;
        float centerY = scopeY + scopeH * 0.5f;

        C2D_DrawRectSolid(scopeX, scopeY, 0.35f, scopeW, scopeH, C2D_Color32(10, 12, 20, 255));
        C2D_DrawLine(scopeX, scopeY, C2D_Color32(0, 160, 210, 180), scopeX + scopeW, scopeY, C2D_Color32(0, 160, 210, 180), 1.0f, 0.36f);
        C2D_DrawLine(scopeX, scopeY + scopeH, C2D_Color32(0, 160, 210, 180), scopeX + scopeW, scopeY + scopeH, C2D_Color32(0, 160, 210, 180), 1.0f, 0.36f);
        C2D_DrawLine(scopeX, scopeY, C2D_Color32(0, 160, 210, 180), scopeX, scopeY + scopeH, C2D_Color32(0, 160, 210, 180), 1.0f, 0.36f);
        C2D_DrawLine(scopeX + scopeW, scopeY, C2D_Color32(0, 160, 210, 180), scopeX + scopeW, scopeY + scopeH, C2D_Color32(0, 160, 210, 180), 1.0f, 0.36f);

        for (int gy = 1; gy <= 3; gy++) {
            float gridY = scopeY + (scopeH * 0.25f) * gy;
            u32 gridCol = (gy == 2) ? C2D_Color32(0, 180, 220, 120) : C2D_Color32(0, 100, 150, 50);
            C2D_DrawLine(scopeX, gridY, gridCol, scopeX + scopeW, gridY, gridCol, 1.0f, 0.36f);
        }
        for (int gx = 1; gx <= 7; gx++) {
            float gridX = scopeX + (scopeW * 0.125f) * gx;
            C2D_DrawLine(gridX, scopeY, C2D_Color32(0, 100, 150, 40), gridX, scopeY + scopeH, C2D_Color32(0, 100, 150, 40), 1.0f, 0.36f);
        }

        drawText("+12dB", scopeX + 4.0f, scopeY + 2.0f, 0.37f, false, C2D_Color32(0, 160, 200, 150), 0.65f);
        drawText("0dB", scopeX + 4.0f, centerY - 6.0f, 0.37f, false, C2D_Color32(0, 220, 255, 200), 0.65f);
        drawText("-12dB", scopeX + 4.0f, scopeY + scopeH - 12.0f, 0.37f, false, C2D_Color32(0, 160, 200, 150), 0.65f);
        drawText("L / R OSCILLOSCOPE", scopeX + scopeW - 4.0f, scopeY + 2.0f, 0.37f, false, C2D_Color32(0, 200, 240, 160), 0.65f);

        const int NUM_WAVE_PTS = 50;
        float realPcm[NUM_WAVE_PTS];
        MusicPlayer::getRealtimeWaveform(realPcm, NUM_WAVE_PTS);

        float ptStep = scopeW / (float)(NUM_WAVE_PTS - 1);

        for (int i = 0; i < NUM_WAVE_PTS - 1; i++) {
            float n1 = (float)i / (float)(NUM_WAVE_PTS - 1);
            float n2 = (float)(i + 1) / (float)(NUM_WAVE_PTS - 1);

            float x1 = scopeX + i * ptStep;
            float x2 = scopeX + (i + 1) * ptStep;

            float env1 = sinf(n1 * 3.14159265f);
            float env2 = sinf(n2 * 3.14159265f);

            float pcmVal1 = realPcm[i];
            float pcmVal2 = realPcm[i + 1];

            float y1_ch1 = centerY - pcmVal1 * 44.0f * env1;
            float y2_ch1 = centerY - pcmVal2 * 44.0f * env2;

            float pcmStereo1 = (i > 0) ? realPcm[i - 1] * 0.85f : pcmVal1;
            float pcmStereo2 = realPcm[i] * 0.85f;
            float y1_ch2 = centerY + pcmStereo1 * 36.0f * env1;
            float y2_ch2 = centerY + pcmStereo2 * 36.0f * env2;

            float specVal1 = fabsf(pcmVal1);
            float specVal2 = fabsf(pcmVal2);
            float y1_spec = (scopeY + scopeH - 4.0f) - specVal1 * 50.0f * env1;
            float y2_spec = (scopeY + scopeH - 4.0f) - specVal2 * 50.0f * env2;

            C2D_DrawLine(x1, y1_spec, C2D_Color32(50, 230, 120, 160), x2, y2_spec, C2D_Color32(50, 230, 120, 160), 1.5f, 0.37f);
            C2D_DrawLine(x1, y1_ch2, C2D_Color32(255, 0, 180, 70), x2, y2_ch2, C2D_Color32(255, 0, 180, 70), 2.5f, 0.38f);
            C2D_DrawLine(x1, y1_ch2, C2D_Color32(255, 140, 230, 200), x2, y2_ch2, C2D_Color32(255, 140, 230, 200), 1.0f, 0.39f);
            C2D_DrawLine(x1, y1_ch1, C2D_Color32(0, 240, 255, 100), x2, y2_ch1, C2D_Color32(0, 240, 255, 100), 3.0f, 0.40f);
            C2D_DrawLine(x1, y1_ch1, C2D_Color32(220, 255, 255, 255), x2, y2_ch1, C2D_Color32(220, 255, 255, 255), 1.2f, 0.41f);
        }

        C2D_DrawRectSolid(0, 0, 0.8f, 400, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 21, 0.81f, 400, 1, C2D_Color32(0, 210, 255, 255));
        drawText("AUDIO PLAYER: " + audioPreviewFileName, 10, 4, 0.38f, false, C2D_Color32(0, 210, 255, 255), 0.85f, 380.0f);

        C2D_DrawRectSolid(0, 218, 0.8f, 400, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 218, 0.81f, 400, 1, C2D_Color32(0, 100, 180, 200));
        drawText("[ A / SELECT: RESTART AUDIO | R: CONVERT | B: CLOSE PLAYER ]", 200, 229, 0.35f, true, C2D_Color32(190, 200, 215, 220), 0.85f);

        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, C2D_Color32(18, 18, 22, 255));

        C2D_DrawRectSolid(0, 0, 0.8f, 320, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 21, 0.81f, 320, 1, C2D_Color32(0, 210, 255, 255));
        drawText("AUDIO PLAYER CONTROLS", 160, 11, 0.38f, true, CWhite, 0.85f);

        C2D_DrawRectSolid(10, 28, 0.4f, 300, 78, C2D_Color32(25, 25, 32, 255));
        C2D_DrawLine(10, 28, C2D_Color32(60, 62, 78, 255), 310, 28, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(10, 28, C2D_Color32(60, 62, 78, 255), 10, 106, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(310, 28, C2D_Color32(60, 62, 78, 255), 310, 106, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(10, 106, C2D_Color32(60, 62, 78, 255), 310, 106, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);

        drawText("Track File: " + audioPreviewFileName, 20, 35, 0.36f, false, C2D_Color32(0, 210, 255, 255), 0.85f, 280.0f);
        
        std::string hzStr = "Target Sample Rate: 44.1 kHz (Original)";
        if (globalFormat == 1) hzStr = "Target Sample Rate: 22.05 kHz (Half Rate)";
        if (globalFormat == 2) hzStr = "Target Sample Rate: 11.025 kHz (Low Memory)";
        drawText(hzStr, 20, 56, 0.34f, false, C2D_Color32(190, 200, 215, 255), 0.85f);
        
        drawText("Status: PLAYING AUDIO SAMPLE...", 20, 77, 0.34f, false, C2D_Color32(80, 240, 120, 255), 0.85f);

        drawStyledButton(10, 112, 300, 34, "CONVERT THIS AUDIO", "[ R / TOUCH ]", C2D_Color32(180, 80, 20, 255), C2D_Color32(120, 50, 12, 255), C2D_Color32(230, 110, 30, 255));
        drawStyledButton(10, 152, 300, 34, "RESTART AUDIO SAMPLE", "[ SELECT / TOUCH ]", C2D_Color32(48, 50, 64, 255), C2D_Color32(32, 34, 46, 255), C2D_Color32(60, 62, 78, 255));
        drawStyledButton(10, 192, 300, 34, "CLOSE PLAYER", "[ B / TOUCH ]", C2D_Color32(140, 30, 30, 255), C2D_Color32(95, 20, 20, 255), C2D_Color32(200, 50, 50, 255));
        return;
    }

    if (isPreviewing) {
        C2D_SceneBegin(top);
        C2D_TargetClear(top, C2D_Color32(20, 20, 26, 255));

        for (float x = 0; x < 400; x += 32) {
            for (float y = 0; y < 240; y += 32) {
                if (((int)(x / 32) + (int)(y / 32)) % 2 == 0) {
                    C2D_DrawRectSolid(x, y, 0.1f, 32, 32, C2D_Color32(30, 30, 38, 255));
                }
            }
        }

        if (viewImg.tex && viewImg.subtex) {
            float drawX = 200.0f - ((previewW * previewZoom) * 0.5f) + previewPanX;
            float drawY = 120.0f - ((previewH * previewZoom) * 0.5f) + previewPanY;
            C2D_DrawImageAt(viewImg, drawX, drawY, 0.4f, nullptr, previewZoom, previewZoom);
        }

        C2D_DrawRectSolid(0, 0, 0.8f, 400, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 21, 0.81f, 400, 1, C2D_Color32(0, 210, 255, 255));
        std::string titleStr = "PREVIEW: " + previewFileName + " (" + std::to_string(previewW) + "x" + std::to_string(previewH) + ")";
        drawText(titleStr, 10, 4, 0.38f, false, C2D_Color32(0, 210, 255, 255), 0.85f, 380.0f);

        C2D_DrawRectSolid(0, 218, 0.8f, 400, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 218, 0.81f, 400, 1, C2D_Color32(0, 100, 180, 200));
        std::string zoomStr = "Zoom: " + std::to_string((int)(previewZoom * 100)) + "%  |  [ D-PAD: PAN | L/R: ZOOM | B: CLOSE ]";
        drawText(zoomStr, 200, 229, 0.35f, true, C2D_Color32(190, 200, 215, 220), 0.85f);

        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, C2D_Color32(18, 18, 22, 255));

        C2D_DrawRectSolid(0, 0, 0.8f, 320, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 21, 0.81f, 320, 1, C2D_Color32(0, 210, 255, 255));
        drawText("IMAGE PREVIEW CONTROLS", 160, 11, 0.38f, true, CWhite, 0.85f);

        C2D_DrawRectSolid(10, 28, 0.4f, 300, 78, C2D_Color32(25, 25, 32, 255));
        C2D_DrawLine(10, 28, C2D_Color32(60, 62, 78, 255), 310, 28, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(10, 28, C2D_Color32(60, 62, 78, 255), 10, 106, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(310, 28, C2D_Color32(60, 62, 78, 255), 310, 106, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(10, 106, C2D_Color32(60, 62, 78, 255), 310, 106, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);

        drawText("File: " + previewFileName, 20, 35, 0.36f, false, C2D_Color32(0, 210, 255, 255), 0.85f, 280.0f);
        drawText("Original Resolution: " + std::to_string(previewW) + " x " + std::to_string(previewH) + " px", 20, 56, 0.34f, false, C2D_Color32(190, 200, 215, 255), 0.85f);
        
        std::string fmtStr = "Target Format: RGBA8";
        if (globalFormat == 1) fmtStr = "Target Format: RGBA4444";
        if (globalFormat == 2) fmtStr = "Target Format: RGB565";
        drawText(fmtStr, 20, 77, 0.34f, false, CYellow, 0.85f);

        drawStyledButton(10, 112, 300, 34, "CONVERT THIS IMAGE", "[ R / TOUCH ]", C2D_Color32(180, 80, 20, 255), C2D_Color32(120, 50, 12, 255), C2D_Color32(230, 110, 30, 255));
        drawStyledButton(10, 152, 300, 34, "RESET ZOOM & PAN", "[ SELECT / TOUCH ]", C2D_Color32(48, 50, 64, 255), C2D_Color32(32, 34, 46, 255), C2D_Color32(60, 62, 78, 255));
        drawStyledButton(10, 192, 300, 34, "CLOSE PREVIEW", "[ B / TOUCH ]", C2D_Color32(140, 30, 30, 255), C2D_Color32(95, 20, 20, 255), C2D_Color32(200, 50, 50, 255));
        return;
    }

    if (isConverting) {
        C2D_SceneBegin(top);
        C2D_TargetClear(top, C2D_Color32(16, 18, 24, 255));

        if (menuBG && !menuBG->frames.empty()) {
            Frame& f = menuBG->frames[0];
            C2D_ImageTint tint; C2D_PlainImageTint(&tint, C2D_Color32(40, 45, 60, 255), 1.0f);
            drawFrameAt(f, 0, 0, 0.1f, &tint);
        }

        C2D_DrawRectSolid(0, 0, 0.8f, 400, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 21, 0.81f, 400, 1, C2D_Color32(0, 210, 255, 255));
        if (sheetIcons) {
            DrawFrameCentered(sheetIcons, iconFolder, 14, 11, 0.85f, 0.65f);
        }
        drawText("3DS ASSET ENCODER - BATCH PROCESSING", 28, 4, 0.38f, false, C2D_Color32(0, 210, 255, 255), 0.88f);

        float cardX = 25.0f, cardY = 34.0f, cardW = 350.0f, cardH = 176.0f;
        C2D_DrawRectSolid(cardX, cardY, 0.3f, cardW, cardH, C2D_Color32(22, 24, 32, 245));
        C2D_DrawLine(cardX, cardY, C2D_Color32(50, 55, 75, 255), cardX + cardW, cardY, C2D_Color32(50, 55, 75, 255), 1.0f, 0.32f);
        C2D_DrawLine(cardX, cardY, C2D_Color32(50, 55, 75, 255), cardX, cardY + cardH, C2D_Color32(50, 55, 75, 255), 1.0f, 0.32f);
        C2D_DrawLine(cardX + cardW, cardY, C2D_Color32(50, 55, 75, 255), cardX + cardW, cardY + cardH, C2D_Color32(50, 55, 75, 255), 1.0f, 0.32f);
        C2D_DrawLine(cardX, cardY + cardH, C2D_Color32(50, 55, 75, 255), cardX + cardW, cardY + cardH, C2D_Color32(50, 55, 75, 255), 1.0f, 0.32f);

        C2D_DrawRectSolid(cardX, cardY, 0.35f, cardW, 26, C2D_Color32(30, 34, 46, 255));
        C2D_DrawRectSolid(cardX, cardY + 25, 0.36f, cardW, 1, C2D_Color32(0, 180, 230, 255));
        if (isAudioMode) {
            drawText("HARDWARE AUDIO & DATA ENCODING", cardX + cardW * 0.5f, cardY + 13.0f, 0.40f, true, CWhite, 0.85f);
        } else {
            drawText("HARDWARE TEXTURE & DATA COMPILING", cardX + cardW * 0.5f, cardY + 13.0f, 0.40f, true, CWhite, 0.85f);
        }

        drawText("ENCODING ASSET:", cardX + 16.0f, cardY + 34.0f, 0.32f, false, C2D_Color32(150, 160, 180, 255), 0.85f);
        drawText(conversionStatus, cardX + 16.0f, cardY + 52.0f, 0.40f, false, CYellow, 0.85f, cardW - 32.0f);

        float barX = cardX + 16.0f;
        float barY = cardY + 86.0f;
        float barW = cardW - 32.0f;
        float barH = 26.0f;

        C2D_DrawRectSolid(barX, barY, 0.5f, barW, barH, C2D_Color32(14, 16, 22, 255));
        C2D_DrawLine(barX, barY, C2D_Color32(60, 65, 80, 255), barX + barW, barY, C2D_Color32(60, 65, 80, 255), 1.0f, 0.52f);
        C2D_DrawLine(barX, barY, C2D_Color32(60, 65, 80, 255), barX, barY + barH, C2D_Color32(60, 65, 80, 255), 1.0f, 0.52f);
        C2D_DrawLine(barX + barW, barY, C2D_Color32(60, 65, 80, 255), barX + barW, barY + barH, C2D_Color32(60, 65, 80, 255), 1.0f, 0.52f);
        C2D_DrawLine(barX, barY + barH, C2D_Color32(60, 65, 80, 255), barX + barW, barY + barH, C2D_Color32(60, 65, 80, 255), 1.0f, 0.52f);

        float fillW = std::max(0.0f, std::min(barW, barW * conversionProgress));
        if (fillW > 0.0f) {
            u32 colLeft = C2D_Color32(0, 130, 210, 255);
            u32 colRight = C2D_Color32(0, 220, 255, 255);
            C2D_DrawRectangle(barX, barY, 0.51f, fillW, barH, colLeft, colRight, colLeft, colRight);
            if (fillW > 3.0f) {
                C2D_DrawRectSolid(barX + fillW - 2.0f, barY, 0.52f, 2.0f, barH, CWhite);
            }
        }

        std::string percentStr = std::to_string((int)(conversionProgress * 100.0f)) + "%";
        drawText(percentStr, barX + barW * 0.5f, barY + barH * 0.5f, 0.40f, true, CWhite, 0.88f);

        std::string countInfo = "Processed: " + std::to_string(currentConvertIdx) + " / " + std::to_string(convertQueue.size()) + " files";
        drawText(countInfo, cardX + 16.0f, cardY + 124.0f, 0.36f, false, C2D_Color32(190, 200, 215, 255), 0.85f);

        static int dotTimer = 0;
        dotTimer++;
        std::string dots = "";
        int numDots = (dotTimer / 15) % 4;
        for (int d = 0; d < numDots; d++) dots += " .";
        
        if (isAudioMode) {
            drawText("Writing Audio data" + dots, cardX + 16.0f, cardY + 146.0f, 0.34f, false, C2D_Color32(0, 210, 255, 255), 0.85f);
        } else {
            drawText("Writing GPU data" + dots, cardX + 16.0f, cardY + 146.0f, 0.34f, false, C2D_Color32(0, 210, 255, 255), 0.85f);
        }

        C2D_DrawRectSolid(0, 218, 0.8f, 400, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 218, 0.81f, 400, 1, C2D_Color32(180, 100, 0, 200));
        drawText("[ DO NOT POWER OFF THE SYSTEM OR REMOVE SD CARD ]", 200, 229, 0.34f, true, C2D_Color32(255, 170, 0, 255), 0.85f);

        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, C2D_Color32(18, 18, 22, 255));

        C2D_DrawRectSolid(0, 0, 0.8f, 320, 22, C2D_Color32(10, 10, 16, 245));
        C2D_DrawRectSolid(0, 21, 0.81f, 320, 1, C2D_Color32(0, 210, 255, 255));
        drawText("CONVERSION BATCH DETAILS", 160, 11, 0.38f, true, CWhite, 0.85f);

        float bCardX = 16.0f, bCardY = 32.0f, bCardW = 288.0f, bCardH = 192.0f;
        C2D_DrawRectSolid(bCardX, bCardY, 0.3f, bCardW, bCardH, C2D_Color32(25, 27, 36, 255));
        C2D_DrawLine(bCardX, bCardY, C2D_Color32(60, 62, 78, 255), bCardX + bCardW, bCardY, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(bCardX, bCardY, C2D_Color32(60, 62, 78, 255), bCardX, bCardY + bCardH, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(bCardX + bCardW, bCardY, C2D_Color32(60, 62, 78, 255), bCardX + bCardW, bCardY + bCardH, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);
        C2D_DrawLine(bCardX, bCardY + bCardH, C2D_Color32(60, 62, 78, 255), bCardX + bCardW, bCardY + bCardH, C2D_Color32(60, 62, 78, 255), 1.0f, 0.42f);

        if (isAudioMode) {
            std::string fmtName = "44.1 kHz (Original Quality)";
            if (globalFormat == 1) fmtName = "22.05 kHz (Half Rate)";
            if (globalFormat == 2) fmtName = "11.025 kHz (Low Memory)";

            drawText("Audio Sample Rate:", bCardX + 16.0f, bCardY + 14.0f, 0.34f, false, C2D_Color32(150, 160, 180, 255), 0.85f);
            drawText(fmtName, bCardX + 16.0f, bCardY + 32.0f, 0.38f, false, C2D_Color32(0, 210, 255, 255), 0.85f);

            drawText("Codec Type:", bCardX + 16.0f, bCardY + 56.0f, 0.34f, false, C2D_Color32(150, 160, 180, 255), 0.85f);
            drawText("SADP (Ima ADPCM) / WAV", bCardX + 16.0f, bCardY + 74.0f, 0.36f, false, CYellow, 0.85f);
        } else {
            std::string fmtName = "RGBA8 (32-bit High Quality)";
            if (globalFormat == 1) fmtName = "RGBA4444 (16-bit Alpha)";
            if (globalFormat == 2) fmtName = "RGB565 (16-bit Fast)";

            drawText("Target Format:", bCardX + 16.0f, bCardY + 14.0f, 0.34f, false, C2D_Color32(150, 160, 180, 255), 0.85f);
            drawText(fmtName, bCardX + 16.0f, bCardY + 32.0f, 0.38f, false, C2D_Color32(0, 210, 255, 255), 0.85f);

            drawText("Texture Limit:", bCardX + 16.0f, bCardY + 56.0f, 0.34f, false, C2D_Color32(150, 160, 180, 255), 0.85f);
            drawText("1024 x 1024 px (Hardware Safe)", bCardX + 16.0f, bCardY + 74.0f, 0.36f, false, CYellow, 0.85f);
        }

        drawText("Working Folder:", bCardX + 16.0f, bCardY + 98.0f, 0.34f, false, C2D_Color32(150, 160, 180, 255), 0.85f);
        drawText(currentDir, bCardX + 16.0f, bCardY + 116.0f, 0.34f, false, CWhite, 0.85f, bCardW - 32.0f);

        C2D_DrawRectSolid(bCardX + 12.0f, bCardY + 144.0f, 0.4f, bCardW - 24.0f, 36.0f, C2D_Color32(16, 45, 30, 255));
        C2D_DrawLine(bCardX + 12.0f, bCardY + 144.0f, C2D_Color32(30, 120, 70, 255), bCardX + bCardW - 12.0f, bCardY + 144.0f, C2D_Color32(30, 120, 70, 255), 1.0f, 0.45f);
        C2D_DrawLine(bCardX + 12.0f, bCardY + 144.0f, C2D_Color32(30, 120, 70, 255), bCardX + 12.0f, bCardY + 180.0f, C2D_Color32(30, 120, 70, 255), 1.0f, 0.45f);
        C2D_DrawLine(bCardX + bCardW - 12.0f, bCardY + 144.0f, C2D_Color32(30, 120, 70, 255), bCardX + bCardW - 12.0f, bCardY + 180.0f, C2D_Color32(30, 120, 70, 255), 1.0f, 0.45f);
        C2D_DrawLine(bCardX + 12.0f, bCardY + 180.0f, C2D_Color32(30, 120, 70, 255), bCardX + bCardW - 12.0f, bCardY + 180.0f, C2D_Color32(30, 120, 70, 255), 1.0f, 0.45f);

        drawText("ENCODING DATA IN PROGRESS...", bCardX + bCardW * 0.5f, bCardY + 162.0f, 0.36f, true, C2D_Color32(80, 240, 120, 255), 0.85f);
        return;
    }

    // Top Screen File Explorer
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(20, 20, 26, 255));
    
    if (menuBG && !menuBG->frames.empty()) {
        Frame& f = menuBG->frames[0];
        C2D_ImageTint tint; C2D_PlainImageTint(&tint, C2D_Color32(60, 60, 90, 255), 1.0f);
        drawFrameAt(f, 0, 0, 0.1f, &tint);
    }

    C2D_DrawRectSolid(0, 0, 0.8f, 400, 22, C2D_Color32(10, 10, 16, 245));
    C2D_DrawRectSolid(0, 21, 0.81f, 400, 1, C2D_Color32(0, 210, 255, 255));

    if (sheetIcons) {
        DrawFrameCentered(sheetIcons, iconFolder, 14, 11, 0.85f, 0.65f);
    }
    std::string displayPath = currentDir.substr(rootDir.size());
    if (displayPath.empty()) displayPath = "/";
    std::string headerTitle = isAudioMode ? "AUDIO CONVERTER - /" : "IMAGE CONVERTER - /";
    drawText(headerTitle + displayPath, 28, 4, 0.38f, false, C2D_Color32(0, 210, 255, 255), 0.88f, 250.0f);

    std::string countStr = std::to_string(files.size()) + " items";
    drawText(countStr, 390.0f, 4.0f, 0.36f, false, C2D_Color32(190, 200, 215, 220), 0.88f, 0.0f, true);

    // Render File Rows with strict Y Culling using cached FileEntry flags
    for (int i = 0; i < (int)files.size(); i++) {
        float rowY = 24.0f + (i - lerpSelected) * 26.0f + 70.0f;
        
        if (rowY < 20.0f || rowY > 234.0f) continue;

        bool isImg = files[i].isConvertible;
        float scale = (i == curSelected) ? 0.48f : 0.40f;
        u32 color = (i == curSelected) ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(190, 200, 215, 255);
        
        if (i == curSelected) {
            C2D_DrawRectSolid(6, rowY - 2, 0.3f, 388, 22, C2D_Color32(0, 80, 160, 200));
            C2D_DrawRectSolid(6, rowY - 2, 0.31f, 3, 22, C2D_Color32(0, 210, 255, 255));
        }

        if (isImg) {
            if (files[i].isSelected) {
                drawText("[X]", 12, rowY + 1, 0.42f, false, CYellow, 0.85f);
            } else {
                drawText("[ ]", 12, rowY + 1, 0.42f, false, C2D_Color32(100, 100, 120, 255), 0.85f);
            }
        } else {
            drawText("-", 14, rowY + 1, 0.42f, false, C2D_Color32(60, 60, 75, 255), 0.85f);
        }

        int iconIdx = files[i].iconIndex;
        if (sheetIcons) {
            DrawFrameCentered(sheetIcons, iconIdx, 38, rowY + 9, 0.5f, 0.70f);
        }

        std::string displayName = files[i].name;
        if (files[i].isDirectory) displayName += "/";
        drawText(displayName, 52, rowY + 1, scale, false, color, 0.85f, 270.0f);

        std::string badge = "";
        u32 badgeColor = CWhite;
        if (files[i].name.find(".rawtex") != std::string::npos) {
            badge = "[RAW]"; badgeColor = C2D_Color32(100, 255, 100, 255);
        } else if (files[i].name.find(".png") != std::string::npos || files[i].name.find(".jpg") != std::string::npos) {
            badge = "[PNG]"; badgeColor = C2D_Color32(255, 200, 50, 255);
        } else if (files[i].name.find(".t3x") != std::string::npos) {
            badge = "[T3X]"; badgeColor = C2D_Color32(0, 220, 255, 255);
        } else if (files[i].name.find(".ogg") != std::string::npos) {
            badge = "[OGG]"; badgeColor = C2D_Color32(255, 120, 0, 255);
        } else if (files[i].name.find(".adp") != std::string::npos) {
            badge = "[ADP]"; badgeColor = C2D_Color32(0, 255, 180, 255);
        } else if (files[i].name.find(".wav") != std::string::npos) {
            badge = "[WAV]"; badgeColor = C2D_Color32(200, 100, 255, 255);
        } else if (files[i].isDirectory) {
            badge = "[DIR]"; badgeColor = C2D_Color32(150, 150, 160, 255);
        }
        if (!badge.empty()) {
            drawText(badge, 332, rowY + 1, 0.40f, false, badgeColor, 0.85f);
        }
    }

    // Bottom Screen Action Dashboard
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(18, 18, 22, 255));
    
    C2D_DrawRectSolid(0, 0, 0.8f, 320, 22, C2D_Color32(10, 10, 16, 245));
    C2D_DrawRectSolid(0, 21, 0.81f, 320, 1, C2D_Color32(0, 210, 255, 255));
    drawText(isAudioMode ? "AUDIO CONVERSION CONTROLS" : "IMAGE CONVERSION CONTROLS", 160, 11, 0.38f, true, CWhite, 0.85f);

    drawStyledButton(8, 28, 148, 36, "CONVERT ALL", "[ TOUCH ]", C2D_Color32(34, 110, 50, 255), C2D_Color32(22, 75, 34, 255), C2D_Color32(45, 160, 70, 255));
    drawStyledButton(164, 28, 148, 36, "CONVERT FOLDER", "[ Y BUTTON ]", C2D_Color32(20, 100, 160, 255), C2D_Color32(12, 65, 110, 255), C2D_Color32(30, 150, 220, 255));

    drawStyledButton(8, 70, 148, 36, "CONVERT SELECTED", "[ X BUTTON ]", C2D_Color32(110, 40, 140, 255), C2D_Color32(75, 25, 95, 255), C2D_Color32(160, 60, 200, 255));
    drawStyledButton(164, 70, 148, 36, isAudioMode ? "CONVERT THIS AUDIO" : "CONVERT THIS IMAGE", "[ R BUTTON ]", C2D_Color32(180, 80, 20, 255), C2D_Color32(120, 50, 12, 255), C2D_Color32(230, 110, 30, 255));

    drawStyledButton(8, 112, 148, 34, isAudioMode ? "MARK AUDIO" : "MARK IMAGE", "[ L BUTTON ]", C2D_Color32(48, 50, 64, 255), C2D_Color32(32, 34, 46, 255), C2D_Color32(60, 62, 78, 255));
    drawStyledButton(164, 112, 148, 34, isAudioMode ? "MARK ALL AUDIO" : "MARK ALL IMAGES", "[ SELECT ]", C2D_Color32(48, 50, 64, 255), C2D_Color32(32, 34, 46, 255), C2D_Color32(60, 62, 78, 255));

    std::string fmtStr = "";
    if (isAudioMode) {
        if (globalFormat == 0) fmtStr = "SAMPLE RATE: 44.1 kHz (HIGH QUALITY)";
        else if (globalFormat == 1) fmtStr = "SAMPLE RATE: 22.05 kHz (BALANCED)";
        else fmtStr = "SAMPLE RATE: 11.025 kHz (LOW MEMORY)";
    } else {
        if (globalFormat == 0) fmtStr = "FORMAT: RGBA8 (HIGH QUALITY)";
        else if (globalFormat == 1) fmtStr = "FORMAT: RGBA4444 (16-BIT ALPHA)";
        else fmtStr = "FORMAT: RGB565 (16-BIT FAST)";
    }
    drawStyledButton(8, 152, 304, 34, fmtStr, "[ TOUCH / PRESS SELECT TO TOGGLE ]", C2D_Color32(35, 45, 55, 255), C2D_Color32(22, 30, 38, 255), C2D_Color32(0, 180, 220, 255));

    drawStyledButton(8, 192, 304, 36, "BACK TO MODS MENU", "[ B BUTTON ]", C2D_Color32(140, 30, 30, 255), C2D_Color32(95, 20, 20, 255), C2D_Color32(200, 50, 50, 255));
}

void AssetConverterState::exitState() {
    closeImagePreview();
    stopAudioSample();
    if (converterTextBuf) { C2D_TextBufDelete(converterTextBuf); converterTextBuf = nullptr; }
}

void AssetConverterState::playAudioSample(const std::string& path, int hzMode) {
    MusicPlayer::play(path.c_str());
}

void AssetConverterState::stopAudioSample() {
    MusicPlayer::stop();
}

static uint32_t crc32_table[256];
static bool crc32_init = false;

static void init_crc32() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_init = true;
}

static uint32_t calculate_crc32(const uint8_t* buf, size_t len) {
    if (!crc32_init) init_crc32();
    uint32_t c = 0xFFFFFFFFL;
    for (size_t i = 0; i < len; i++) {
        c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFL;
}

static uint32_t calculate_adler32(const uint8_t* buf, size_t len) {
    uint32_t s1 = 1;
    uint32_t s2 = 0;
    for (size_t i = 0; i < len; i++) {
        s1 = (s1 + buf[i]) % 65521;
        s2 = (s2 + s1) % 65521;
    }
    return (s2 << 16) | s1;
}

static bool savePngFile(const std::string& path, int w, int h, const uint8_t* rgba) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    const uint8_t header[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    fwrite(header, 1, 8, f);

    uint8_t ihdrData[13];
    ihdrData[0] = (w >> 24) & 0xFF; ihdrData[1] = (w >> 16) & 0xFF; ihdrData[2] = (w >> 8) & 0xFF; ihdrData[3] = w & 0xFF;
    ihdrData[4] = (h >> 24) & 0xFF; ihdrData[5] = (h >> 16) & 0xFF; ihdrData[6] = (h >> 8) & 0xFF; ihdrData[7] = h & 0xFF;
    ihdrData[8] = 8; ihdrData[9] = 6; ihdrData[10] = 0; ihdrData[11] = 0; ihdrData[12] = 0;

    uint32_t ihdrLen = 13;
    uint32_t ihdrLenBE = ((ihdrLen >> 24) & 0xFF) | ((ihdrLen >> 8) & 0xFF00) | ((ihdrLen << 8) & 0xFF0000) | ((ihdrLen << 24) & 0xFF000000);
    fwrite(&ihdrLenBE, 4, 1, f);
    
    uint8_t ihdrChunkBuf[17];
    memcpy(ihdrChunkBuf, "IHDR", 4);
    memcpy(ihdrChunkBuf + 4, ihdrData, 13);
    fwrite(ihdrChunkBuf, 1, 17, f);

    uint32_t ihdrCrc = calculate_crc32(ihdrChunkBuf, 17);
    uint32_t ihdrCrcBE = ((ihdrCrc >> 24) & 0xFF) | ((ihdrCrc >> 8) & 0xFF00) | ((ihdrCrc << 8) & 0xFF0000) | ((ihdrCrc << 24) & 0xFF000000);
    fwrite(&ihdrCrcBE, 4, 1, f);

    size_t rowSize = 1 + (size_t)w * 4;
    size_t rawDataSize = rowSize * h;
    uint8_t* rawData = (uint8_t*)malloc(rawDataSize);
    if (!rawData) { fclose(f); return false; }

    for (int y = 0; y < h; y++) {
        uint8_t* rowPtr = rawData + y * rowSize;
        rowPtr[0] = 0;
        memcpy(rowPtr + 1, rgba + y * w * 4, w * 4);
    }

    uint32_t adler = calculate_adler32(rawData, rawDataSize);

    std::vector<uint8_t> zlibStream;
    zlibStream.push_back(0x78); zlibStream.push_back(0x01);

    size_t bytesLeft = rawDataSize;
    size_t offset = 0;

    while (bytesLeft > 0) {
        uint16_t blockSize = (bytesLeft > 65535) ? 65535 : (uint16_t)bytesLeft;
        bool isFinal = (bytesLeft == blockSize);
        
        zlibStream.push_back(isFinal ? 0x01 : 0x00);
        zlibStream.push_back(blockSize & 0xFF);
        zlibStream.push_back((blockSize >> 8) & 0xFF);
        
        uint16_t nlen = ~blockSize;
        zlibStream.push_back(nlen & 0xFF);
        zlibStream.push_back((nlen >> 8) & 0xFF);

        zlibStream.insert(zlibStream.end(), rawData + offset, rawData + offset + blockSize);
        offset += blockSize;
        bytesLeft -= blockSize;
    }

    free(rawData);

    zlibStream.push_back((adler >> 24) & 0xFF);
    zlibStream.push_back((adler >> 16) & 0xFF);
    zlibStream.push_back((adler >> 8) & 0xFF);
    zlibStream.push_back(adler & 0xFF);

    uint32_t idatLen = (uint32_t)zlibStream.size();
    uint32_t idatLenBE = ((idatLen >> 24) & 0xFF) | ((idatLen >> 8) & 0xFF00) | ((idatLen << 8) & 0xFF0000) | ((idatLen << 24) & 0xFF000000);
    fwrite(&idatLenBE, 4, 1, f);

    std::vector<uint8_t> idatChunkBuf;
    idatChunkBuf.push_back('I'); idatChunkBuf.push_back('D'); idatChunkBuf.push_back('A'); idatChunkBuf.push_back('T');
    idatChunkBuf.insert(idatChunkBuf.end(), zlibStream.begin(), zlibStream.end());
    fwrite(idatChunkBuf.data(), 1, idatChunkBuf.size(), f);

    uint32_t idatCrc = calculate_crc32(idatChunkBuf.data(), idatChunkBuf.size());
    uint32_t idatCrcBE = ((idatCrc >> 24) & 0xFF) | ((idatCrc >> 8) & 0xFF00) | ((idatCrc << 8) & 0xFF0000) | ((idatCrc << 24) & 0xFF000000);
    fwrite(&idatCrcBE, 4, 1, f);

    const uint8_t iendChunk[12] = { 0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82 };
    fwrite(iendChunk, 1, 12, f);

    fclose(f);
    return true;
}

static bool convertRawTexToPng(const std::string& rawtexPath, const std::string& pngPath) {
    FILE* f = fopen(rawtexPath.c_str(), "rb");
    if (!f) return false;

    RawTexHeader header;
    if (fread(&header, sizeof(RawTexHeader), 1, f) != 1) {
        fclose(f);
        return false;
    }

    int pw = header.width;
    int ph = header.height;
    int w = header.origW;
    int h = header.origH;

    if (pw <= 0 || ph <= 0 || w <= 0 || h <= 0) {
        fclose(f);
        return false;
    }

    bool isRWTX = (memcmp(header.magic, "RWTX", 4) == 0);
    bool isRWT4 = (memcmp(header.magic, "RWT4", 4) == 0);
    bool isRWT5 = (memcmp(header.magic, "RWT5", 4) == 0);

    if (!isRWTX && !isRWT4 && !isRWT5) {
        fclose(f);
        return false;
    }

    size_t bytesPerPixel = isRWTX ? 4 : 2;
    size_t dataSize = (size_t)pw * ph * bytesPerPixel;
    uint8_t* swizzled = (uint8_t*)malloc(dataSize);
    if (!swizzled) {
        fclose(f);
        return false;
    }

    if (fread(swizzled, 1, dataSize, f) != dataSize) {
        free(swizzled);
        fclose(f);
        return false;
    }
    fclose(f);

    uint8_t* rgba = (uint8_t*)malloc((size_t)w * h * 4);
    if (!rgba) {
        free(swizzled);
        return false;
    }

    uint32_t* swizzled32 = (uint32_t*)swizzled;
    uint16_t* swizzled16 = (uint16_t*)swizzled;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t i = (x & 7) | ((y & 7) << 8);
            i = (i ^ (i << 2)) & 0x1313; i = (i ^ (i << 1)) & 0x1515;
            uint32_t tx = x >> 3; uint32_t ty = y >> 3;
            
            uint8_t r = 0, g = 0, b = 0, a = 255;

            if (isRWTX) {
                uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                uint32_t px = swizzled32[tile_start + local_idx];
                r = (px >> 24) & 0xFF;
                g = (px >> 16) & 0xFF;
                b = (px >> 8) & 0xFF;
                a = px & 0xFF;
            } else if (isRWT4) {
                uint32_t tile_start = (ty * (pw >> 3) + tx) << 5;
                uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                uint16_t px = swizzled16[tile_start + (local_idx >> 1)];
                r = ((px >> 12) & 0x0F) * 17;
                g = ((px >> 8) & 0x0F) * 17;
                b = ((px >> 4) & 0x0F) * 17;
                a = (px & 0x0F) * 17;
            } else if (isRWT5) {
                uint32_t tile_start = (ty * (pw >> 3) + tx) << 5;
                uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                uint16_t px = swizzled16[tile_start + (local_idx >> 1)];
                r = ((px >> 11) & 0x1F) * 255 / 31;
                g = ((px >> 5) & 0x3F) * 255 / 63;
                b = (px & 0x1F) * 255 / 31;
                a = 255;
            }

            size_t dstIdx = ((size_t)y * w + x) * 4;
            rgba[dstIdx + 0] = r;
            rgba[dstIdx + 1] = g;
            rgba[dstIdx + 2] = b;
            rgba[dstIdx + 3] = a;
        }
    }

    free(swizzled);

    bool ok = savePngFile(pngPath, w, h, rgba);
    free(rgba);

    if (ok) {
        remove(rawtexPath.c_str());
    }
    return ok;
}

static bool convertT3XToPng(const std::string& t3xPath, const std::string& pngPath) {
    C2D_SpriteSheet sheet = C2D_SpriteSheetLoad(t3xPath.c_str());
    if (!sheet) return false;

    C2D_Image img = C2D_SpriteSheetGetImage(sheet, 0);
    if (!img.subtex || !img.tex) {
        C2D_SpriteSheetFree(sheet);
        return false;
    }

    int w = img.subtex->width;
    int h = img.subtex->height;
    C3D_Tex* tex = (C3D_Tex*)img.tex;
    int pw = tex->width;
    int ph = tex->height;

    if (w <= 0 || h <= 0 || pw <= 0 || ph <= 0 || !tex->data) {
        C2D_SpriteSheetFree(sheet);
        return false;
    }

    uint8_t* rgba = (uint8_t*)malloc((size_t)w * h * 4);
    if (!rgba) {
        C2D_SpriteSheetFree(sheet);
        return false;
    }

    uint32_t* swizzled32 = (uint32_t*)tex->data;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t i = (x & 7) | ((y & 7) << 8);
            i = (i ^ (i << 2)) & 0x1313; i = (i ^ (i << 1)) & 0x1515;
            uint32_t tx = x >> 3; uint32_t ty = y >> 3;
            uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
            uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);

            uint32_t px = swizzled32[tile_start + local_idx];
            uint8_t r = (px >> 24) & 0xFF;
            uint8_t g = (px >> 16) & 0xFF;
            uint8_t b = (px >> 8) & 0xFF;
            uint8_t a = px & 0xFF;

            size_t dstIdx = ((size_t)y * w + x) * 4;
            rgba[dstIdx + 0] = r;
            rgba[dstIdx + 1] = g;
            rgba[dstIdx + 2] = b;
            rgba[dstIdx + 3] = a;
        }
    }

    C2D_SpriteSheetFree(sheet);

    bool ok = savePngFile(pngPath, w, h, rgba);
    free(rgba);

    if (ok) {
        remove(t3xPath.c_str());
    }
    return ok;
}

static bool convertAdpToWav(const std::string& adpPath, const std::string& wavPath) {
    FILE* fIn = fopen(adpPath.c_str(), "rb");
    if (!fIn) return false;

    AdpcmEncoder::Header header;
    if (fread(&header, sizeof(header), 1, fIn) != 1 || memcmp(header.magic, "SADP", 4) != 0 || header.numSamples == 0) {
        fclose(fIn);
        return false;
    }

#pragma pack(push, 1)
    struct WavHeader {
        char riff[4];
        uint32_t fileSize;
        char wave[4];
        char fmt[4];
        uint32_t fmtLen;
        uint16_t format;
        uint16_t channels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
        char data[4];
        uint32_t dataSize;
    } wavHead;
#pragma pack(pop)

    memset(&wavHead, 0, sizeof(wavHead));
    memcpy(wavHead.riff, "RIFF", 4);
    memcpy(wavHead.wave, "WAVE", 4);
    memcpy(wavHead.fmt, "fmt ", 4);
    wavHead.fmtLen = 16;
    wavHead.format = 1; // PCM
    wavHead.channels = header.channels;
    wavHead.sampleRate = header.sampleRate;
    wavHead.bitsPerSample = 16;
    wavHead.blockAlign = header.channels * sizeof(int16_t);
    wavHead.byteRate = header.sampleRate * wavHead.blockAlign;
    memcpy(wavHead.data, "data", 4);
    uint32_t dataSize = header.numSamples * wavHead.blockAlign;
    wavHead.dataSize = dataSize;
    wavHead.fileSize = 36 + dataSize;

    FILE* fOut = fopen(wavPath.c_str(), "wb");
    if (!fOut) {
        fclose(fIn);
        return false;
    }

    fwrite(&wavHead, sizeof(wavHead), 1, fOut);

    AdpcmDecoder::State decState;
    const size_t CHUNK_BYTES = 16384;
    uint8_t* adpBuf = (uint8_t*)malloc(CHUNK_BYTES);
    int16_t* pcmBuf = (int16_t*)malloc(CHUNK_BYTES * 2 * sizeof(int16_t));

    size_t bytesRead = 0;
    while ((bytesRead = fread(adpBuf, 1, CHUNK_BYTES, fIn)) > 0) {
        uint32_t samplesDecoded = 0;
        AdpcmDecoder::decodeIMA(adpBuf, (uint32_t)bytesRead, pcmBuf, samplesDecoded, decState);
        if (samplesDecoded > 0) {
            fwrite(pcmBuf, sizeof(int16_t), samplesDecoded, fOut);
        }
    }

    free(adpBuf);
    free(pcmBuf);
    fclose(fIn);
    fclose(fOut);

    remove(adpPath.c_str());
    return true;
}

static bool convertWavToAdp(const std::string& wavPath, const std::string& adpPath, int globalFormat) {
    FILE* fIn = fopen(wavPath.c_str(), "rb");
    if (!fIn) return false;

    char riff[4];
    if (fread(riff, 1, 4, fIn) != 4 || memcmp(riff, "RIFF", 4) != 0) { fclose(fIn); return false; }
    fseek(fIn, 4, SEEK_CUR);
    char wave[4];
    if (fread(wave, 1, 4, fIn) != 4 || memcmp(wave, "WAVE", 4) != 0) { fclose(fIn); return false; }

    int channels = 2;
    int origRate = 44100;
    int bitDepth = 16;
    uint32_t dataOffset = 0;
    uint32_t totalSamples = 0;
    bool foundFmt = false, foundData = false;

    while (!foundData) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (fread(chunkId, 1, 4, fIn) != 4) break;
        if (fread(&chunkSize, 4, 1, fIn) != 1) break;

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat = 0;
            fread(&audioFormat, 2, 1, fIn);
            if (audioFormat != 1) { fclose(fIn); return false; }
            uint16_t chans = 0;
            fread(&chans, 2, 1, fIn);
            channels = chans;
            fread(&origRate, 4, 1, fIn);
            fseek(fIn, 6, SEEK_CUR);
            uint16_t bits = 0;
            fread(&bits, 2, 1, fIn);
            bitDepth = bits;
            if (chunkSize > 16) fseek(fIn, chunkSize - 16, SEEK_CUR);
            foundFmt = true;
        } else if (memcmp(chunkId, "data", 4) == 0) {
            dataOffset = ftell(fIn);
            totalSamples = chunkSize / (channels * (bitDepth / 8));
            foundData = true;
        } else {
            fseek(fIn, chunkSize, SEEK_CUR);
        }
    }

    if (!foundFmt || !foundData || bitDepth != 16) {
        fclose(fIn);
        return false;
    }

    fseek(fIn, dataOffset, SEEK_SET);

    int targetRate = origRate;
    if (globalFormat == 1) targetRate = 22050;
    if (globalFormat == 2) targetRate = 11025;
    float step = (targetRate < origRate) ? ((float)origRate / targetRate) : 1.0f;

    FILE* fOut = fopen(adpPath.c_str(), "wb");
    if (!fOut) {
        fclose(fIn);
        return false;
    }

    AdpcmEncoder::Header header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "SADP", 4);
    header.sampleRate = targetRate;
    header.numSamples = 0;
    header.channels = 1;
    fwrite(&header, sizeof(header), 1, fOut);

    AdpcmEncoder::State encState;
    const int IN_BUF_SAMPLES = 4096;
    int16_t* pcmBuf = (int16_t*)malloc(IN_BUF_SAMPLES * channels * sizeof(int16_t));
    int16_t* monoBuf = (int16_t*)malloc(IN_BUF_SAMPLES * sizeof(int16_t));
    uint8_t* adpcmOut = (uint8_t*)malloc(IN_BUF_SAMPLES);

    uint32_t totalEncodedSamples = 0;
    uint32_t samplesReadTotal = 0;

    if (pcmBuf && monoBuf && adpcmOut) {
        while (samplesReadTotal < totalSamples) {
            uint32_t toRead = std::min((uint32_t)IN_BUF_SAMPLES, totalSamples - samplesReadTotal);
            if (toRead == 0) break;
            
            size_t bytesRead = fread(pcmBuf, 1, toRead * channels * sizeof(int16_t), fIn);
            if (bytesRead == 0) break;

            int samplesRead = bytesRead / (channels * sizeof(int16_t));
            samplesReadTotal += samplesRead;
            int newSamples = (int)(samplesRead / step);

            for (int i = 0; i < newSamples; i++) {
                int srcIdx = (int)(i * step);
                if (srcIdx >= samplesRead) srcIdx = samplesRead - 1;
                if (channels == 2) {
                    monoBuf[i] = (int16_t)(((int32_t)pcmBuf[srcIdx*2] + (int32_t)pcmBuf[srcIdx*2+1]) / 2);
                } else {
                    monoBuf[i] = pcmBuf[srcIdx];
                }
            }

            size_t outBytes = AdpcmEncoder::encodeIMABuf(monoBuf, newSamples, encState, adpcmOut);
            if (outBytes > 0) {
                fwrite(adpcmOut, 1, outBytes, fOut);
                totalEncodedSamples += newSamples;
            }
        }

        if (encState.hasPending) {
            uint8_t lastByte = encState.pendingNibble;
            fwrite(&lastByte, 1, 1, fOut);
        }

        fseek(fOut, 0, SEEK_SET);
        header.numSamples = totalEncodedSamples;
        fwrite(&header, sizeof(header), 1, fOut);
    }

    if (pcmBuf) free(pcmBuf);
    if (monoBuf) free(monoBuf);
    if (adpcmOut) free(adpcmOut);
    fclose(fIn);
    fclose(fOut);

    remove(wavPath.c_str());
    return true;
}

void AssetConverterState::startConversionMode(ConversionMode mode) {
    stopAudioSample();
    std::string activeAudioPath = audioPreviewFullPath;
    closeAudioPreview();
    closeImagePreview();

    convertQueue.clear();
    currentMode = mode;

    auto isConvertibleFile = [this](const std::string& name) {
        return this->isImageFile(name, false);
    };

    if (mode == ConversionMode::ALL) {
        std::vector<std::string> dirsToScan = {rootDir};
        while (!dirsToScan.empty()) {
            std::string curDir = dirsToScan.back();
            dirsToScan.pop_back();
            
            DIR* dir = opendir(curDir.c_str());
            if (dir) {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string name = entry->d_name;
                    if (name == "." || name == "..") continue;
                    
                    std::string fullPath = curDir + name;
                    bool isDir = (entry->d_type == DT_DIR);
                    if (entry->d_type == DT_UNKNOWN) {
                        struct stat st;
                        if (stat(fullPath.c_str(), &st) == 0) isDir = S_ISDIR(st.st_mode);
                    }

                    if (isDir) {
                        dirsToScan.push_back(fullPath + "/");
                    } else if (isConvertibleFile(name)) {
                        convertQueue.push_back(fullPath);
                    }
                }
                closedir(dir);
            }
        }
    } else if (mode == ConversionMode::FOLDER) {
        DIR* dir = opendir(currentDir.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name == "." || name == "..") continue;
                if (isConvertibleFile(name)) {
                    convertQueue.push_back(currentDir + name);
                }
            }
            closedir(dir);
        }
    } else if (mode == ConversionMode::SELECTED) {
        for (const auto& f : files) {
            if (f.isSelected && !f.isDirectory) {
                convertQueue.push_back(currentDir + f.name);
            }
        }
    } else if (mode == ConversionMode::SINGLE) {
        if (!activeAudioPath.empty()) {
            convertQueue.push_back(activeAudioPath);
        } else if (!files.empty() && !files[curSelected].isDirectory) {
            convertQueue.push_back(currentDir + files[curSelected].name);
        }
    }

    if (convertQueue.empty()) return;

    isConverting = true;
    conversionProgress = 0.0f;
    currentConvertIdx = 0;
    conversionStatus = "Initializing...";
}

void AssetConverterState::processConversion() {
    if (currentConvertIdx >= (int)convertQueue.size()) {
        isConverting = false;
        for (auto& f : files) f.isSelected = false;
        loadDirectory(currentDir);
        return;
    }
    
    std::string fullPath = convertQueue[currentConvertIdx];
    std::string file = fullPath.substr(rootDir.size());
    std::string lowerFile = file;
    for (char& c : lowerFile) c = (char)tolower((unsigned char)c);

    conversionStatus = "Processing: " + file;
    conversionProgress = (float)currentConvertIdx / convertQueue.size();

    if (Paths::fileExists(fullPath)) {
        if (lowerFile.find(".ogg") != std::string::npos) {
            std::string baseWithoutExt = fullPath.substr(0, fullPath.find_last_of("."));
            std::string outPath = baseWithoutExt + ".adp";
            conversionStatus = "Encoding Audio (ADP): " + file;

            FILE* fIn = fopen(fullPath.c_str(), "rb");
            if (fIn) {
                OggVorbis_File vf;
                memset(&vf, 0, sizeof(OggVorbis_File));
                if (ov_open_callbacks(fIn, &vf, NULL, 0, s_convVorbisCallbacks) == 0) {
                    vorbis_info* vi = ov_info(&vf, -1);
                    int origRate = vi->rate;
                    int channels = vi->channels;

                    int targetRate = origRate;
                    if (globalFormat == 1) targetRate = 22050;
                    if (globalFormat == 2) targetRate = 11025;

                    float step = (targetRate < origRate) ? ((float)origRate / targetRate) : 1.0f;

                    FILE* fOut = fopen(outPath.c_str(), "wb");
                    if (fOut) {
                        AdpcmEncoder::Header header;
                        memset(&header, 0, sizeof(header));
                        memcpy(header.magic, "SADP", 4);
                        header.sampleRate = targetRate;
                        header.numSamples = 0;
                        header.channels = 1;
                        fwrite(&header, sizeof(header), 1, fOut);

                        AdpcmEncoder::State encState;
                        const int IN_BUF_SAMPLES = 4096;
                        int16_t* pcmBuf = (int16_t*)malloc(IN_BUF_SAMPLES * channels * sizeof(int16_t));
                        int16_t* monoBuf = (int16_t*)malloc(IN_BUF_SAMPLES * sizeof(int16_t));
                        uint8_t* adpcmOut = (uint8_t*)malloc(IN_BUF_SAMPLES);

                        uint32_t totalEncodedSamples = 0;

                        if (pcmBuf && monoBuf && adpcmOut) {
                            int bitstream = 0;
                            while (true) {
                                long readBytes = ov_read(&vf, (char*)pcmBuf, IN_BUF_SAMPLES * channels * sizeof(int16_t), &bitstream);
                                if (readBytes < 0) {
                                    if (readBytes == OV_HOLE) continue;
                                    break;
                                }
                                if (readBytes == 0) break;

                                int samplesRead = readBytes / (channels * sizeof(int16_t));
                                int newSamples = (int)(samplesRead / step);

                                for (int i = 0; i < newSamples; i++) {
                                    int srcIdx = (int)(i * step);
                                    if (srcIdx >= samplesRead) srcIdx = samplesRead - 1;
                                    if (channels == 2) {
                                        monoBuf[i] = (int16_t)(((int32_t)pcmBuf[srcIdx*2] + (int32_t)pcmBuf[srcIdx*2+1]) / 2);
                                    } else {
                                        monoBuf[i] = pcmBuf[srcIdx];
                                    }
                                }

                                size_t outBytes = AdpcmEncoder::encodeIMABuf(monoBuf, newSamples, encState, adpcmOut);
                                if (outBytes > 0) {
                                    fwrite(adpcmOut, 1, outBytes, fOut);
                                    totalEncodedSamples += newSamples;
                                }
                            }

                            if (encState.hasPending) {
                                uint8_t lastByte = encState.pendingNibble;
                                fwrite(&lastByte, 1, 1, fOut);
                            }

                            fseek(fOut, 0, SEEK_SET);
                            header.numSamples = totalEncodedSamples;
                            fwrite(&header, sizeof(header), 1, fOut);
                        }

                        if (pcmBuf) free(pcmBuf);
                        if (monoBuf) free(monoBuf);
                        if (adpcmOut) free(adpcmOut);
                        fclose(fOut);
                        ov_clear(&vf);
                        remove(fullPath.c_str());
                    } else {
                        ov_clear(&vf);
                    }
                } else {
                    fclose(fIn);
                }
            }
            currentConvertIdx++;
        } else if (lowerFile.find(".wav") != std::string::npos) {
            std::string baseWithoutExt = fullPath.substr(0, fullPath.find_last_of("."));
            std::string outPath = baseWithoutExt + ".adp";
            conversionStatus = "Encoding Audio (ADP): " + file;
            convertWavToAdp(fullPath, outPath, globalFormat);
            currentConvertIdx++;
        } else if (lowerFile.find(".adp") != std::string::npos) {
            std::string baseWithoutExt = fullPath.substr(0, fullPath.find_last_of("."));
            std::string outPath = baseWithoutExt + ".wav";
            conversionStatus = "Deconverting ADP -> WAV: " + file;
            convertAdpToWav(fullPath, outPath);
            currentConvertIdx++;
        } else if (lowerFile.find(".rawtex") != std::string::npos) {
            std::string baseWithoutExt = fullPath.substr(0, fullPath.find_last_of("."));
            std::string outPath = baseWithoutExt + ".png";
            conversionStatus = "Deconverting RAW -> PNG: " + file;
            convertRawTexToPng(fullPath, outPath);
            currentConvertIdx++;
        } else if (lowerFile.find(".t3x") != std::string::npos) {
            std::string baseWithoutExt = fullPath.substr(0, fullPath.find_last_of("."));
            std::string outPath = baseWithoutExt + ".png";
            conversionStatus = "Deconverting T3X -> PNG: " + file;
            convertT3XToPng(fullPath, outPath);
            currentConvertIdx++;
        } else if (lowerFile.find(".png") != std::string::npos || lowerFile.find(".jpg") != std::string::npos || lowerFile.find(".jpeg") != std::string::npos) {
            std::string baseWithoutExt = fullPath.substr(0, fullPath.find_last_of("."));
            std::string outPath = baseWithoutExt + ".rawtex";
            conversionStatus = "Converting PNG -> RAW: " + file;
            
            int w = 0, h = 0, c = 0;
            unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &c, 4);

            if (data) {
                if (w > maxTextureSize || h > maxTextureSize) {
                    float scaleFactor = (w > h) ? ((float)maxTextureSize / w) : ((float)maxTextureSize / h);
                    int newW = (int)(w * scaleFactor);
                    int newH = (int)(h * scaleFactor);

                    unsigned char* resizedData = (unsigned char*)malloc(newW * newH * 4);
                    for (int ry = 0; ry < newH; ry++) {
                        for (int rx = 0; rx < newW; rx++) {
                            int sx = (int)(rx / scaleFactor);
                            int sy = (int)(ry / scaleFactor);
                            if (sx >= w) sx = w - 1;
                            if (sy >= h) sy = h - 1;

                            int srcP = (sy * w + sx) * 4;
                            int dstP = (ry * newW + rx) * 4;
                            resizedData[dstP + 0] = data[srcP + 0];
                            resizedData[dstP + 1] = data[srcP + 1];
                            resizedData[dstP + 2] = data[srcP + 2];
                            resizedData[dstP + 3] = data[srcP + 3];
                        }
                    }
                    stbi_image_free(data);
                    data = resizedData;
                    w = newW;
                    h = newH;

                    if (scaleXmlCoords) {
                        std::string xmlPath = baseWithoutExt + ".xml";
                        if (Paths::fileExists(xmlPath)) {
                            scaleXmlFile3DS(xmlPath, scaleFactor);
                        }
                    }
                }

                int pw = 1, ph = 1;
                while(pw < w) pw *= 2;
                while(ph < h) ph *= 2;

                FILE* fOut = fopen(outPath.c_str(), "wb");
                if (fOut) {
                    int imgFormat = globalFormat;
                    auto it = customSettings.find(fullPath);
                    if (it != customSettings.end()) imgFormat = it->second.first;
                    
                    RawTexHeader header;
                    if (imgFormat == 0) memcpy(header.magic, "RWTX", 4);
                    else if (imgFormat == 1) memcpy(header.magic, "RWT4", 4);
                    else if (imgFormat == 2) memcpy(header.magic, "RWT5", 4);
                    
                    header.width = pw; header.height = ph;
                    header.origW = w; header.origH = h;
                    fwrite(&header, sizeof(RawTexHeader), 1, fOut);
                    
                    if (imgFormat == 0) { // RGBA8
                        uint32_t* swizzled = (uint32_t*)linearAlloc(pw * ph * 4);
                        if (swizzled) {
                            memset(swizzled, 0, pw * ph * 4);
                            for(int y=0; y<h; y++) {
                                for(int x=0; x<w; x++) {
                                    int src = (y*w+x)*4;
                                    uint32_t px = (data[src]<<24)|(data[src+1]<<16)|(data[src+2]<<8)|data[src+3];
                                    uint32_t i = (x & 7) | ((y & 7) << 8);
                                    i = (i ^ (i << 2)) & 0x1313; i = (i ^ (i << 1)) & 0x1515;
                                    uint32_t tx = x >> 3; uint32_t ty = y >> 3;
                                    uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                                    uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                                    swizzled[tile_start + local_idx] = px;
                                }
                            }
                            fwrite(swizzled, pw * ph * 4, 1, fOut);
                            linearFree(swizzled);
                        }
                    } else if (imgFormat == 1) { // RGBA4444
                        uint16_t* swizzled = (uint16_t*)linearAlloc(pw * ph * 2);
                        if (swizzled) {
                            memset(swizzled, 0, pw * ph * 2);
                            for(int y=0; y<h; y++) {
                                for(int x=0; x<w; x++) {
                                    int src = (y*w+x)*4;
                                    uint16_t px = ((data[src]>>4)<<12) | ((data[src+1]>>4)<<8) | ((data[src+2]>>4)<<4) | (data[src+3]>>4);
                                    uint32_t i = (x & 7) | ((y & 7) << 8);
                                    i = (i ^ (i << 2)) & 0x1313; i = (i ^ (i << 1)) & 0x1515;
                                    uint32_t tx = x >> 3; uint32_t ty = y >> 3;
                                    uint32_t tile_start = (ty * (pw >> 3) + tx) << 5;
                                    uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                                    swizzled[tile_start + (local_idx>>1)] = px;
                                }
                            }
                            fwrite(swizzled, pw * ph * 2, 1, fOut);
                            linearFree(swizzled);
                        }
                    } else if (imgFormat == 2) { // RGB565
                        uint16_t* swizzled = (uint16_t*)linearAlloc(pw * ph * 2);
                        if (swizzled) {
                            memset(swizzled, 0, pw * ph * 2);
                            for(int y=0; y<h; y++) {
                                for(int x=0; x<w; x++) {
                                    int src = (y*w+x)*4;
                                    uint16_t px = ((data[src]>>3)<<11) | ((data[src+1]>>2)<<5) | (data[src+2]>>3);
                                    uint32_t i = (x & 7) | ((y & 7) << 8);
                                    i = (i ^ (i << 2)) & 0x1313; i = (i ^ (i << 1)) & 0x1515;
                                    uint32_t tx = x >> 3; uint32_t ty = y >> 3;
                                    uint32_t tile_start = (ty * (pw >> 3) + tx) << 5;
                                    uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                                    swizzled[tile_start + (local_idx>>1)] = px;
                                }
                            }
                            fwrite(swizzled, pw * ph * 2, 1, fOut);
                            linearFree(swizzled);
                        }
                    }
                    fclose(fOut);
                    remove(fullPath.c_str());
                }
                stbi_image_free(data);
            }
            currentConvertIdx++;
        } else if (lowerFile.find(".ogg") != std::string::npos) {
            std::string outPath = currentDir + file.substr(0, file.find_last_of(".")) + ".adp";
            conversionStatus = "Opening Audio: " + file;

            convFIn = fopen(fullPath.c_str(), "rb");
            if (convFIn) {
                OggVorbis_File* vf = (OggVorbis_File*)malloc(sizeof(OggVorbis_File));
                memset(vf, 0, sizeof(OggVorbis_File));
                if (ov_open_callbacks(convFIn, vf, NULL, 0, s_convVorbisCallbacks) == 0) {
                    vorbis_info* vi = ov_info(vf, -1);
                    convChannels = vi->channels;
                    
                    int origRate = vi->rate;
                    int targetRate = origRate;
                    auto it = customSettings.find(fullPath);
                    if (it != customSettings.end()) {
                        if (it->second.second == 1) targetRate = 22050;
                        if (it->second.second == 2) targetRate = 11025;
                    }
                    float step = 1.0f;
                    if (targetRate < origRate) step = (float)origRate / targetRate;
                    
                    long totalOrigSamples = (long)ov_pcm_total(vf, -1);
                    convTotalSamples = (uint32_t)(totalOrigSamples / step);
                    
                    convFOut = fopen(outPath.c_str(), "wb");
                    if (convFOut && convTotalSamples > 0) {
                        AdpcmEncoder::Header header;
                        memset(&header, 0, sizeof(header));
                        memcpy(header.magic, "SADP", 4);
                        header.sampleRate = targetRate;
                        header.numSamples = convTotalSamples;
                        header.channels = 1;
                        fwrite(&header, sizeof(header), 1, convFOut);

                        convVfPtr = vf;
                        convStatePtr = new AdpcmEncoder::State();
                        convSamplesProcessed = 0;
                        isAudioPhase = true;
                        return;
                    }
                }
                if (vf) { ov_clear(vf); free(vf); }
            }
            currentConvertIdx++;
        } else {
            currentConvertIdx++;
        }
    } else {
        currentConvertIdx++;
    }

    if (!convertQueue.empty()) {
        conversionProgress = (float)currentConvertIdx / (float)convertQueue.size();
    }
}
