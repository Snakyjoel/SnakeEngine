// I fucking need to rework this bullshit, new ui, more speed, everything...
#include "AssetConverterState.hpp"
#include "ModsMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "AdpcmEncoder.hpp"
#include <sys/stat.h>
#include <algorithm>
#include <tremor/ivorbisfile.h>

#include "../backend/stb_image.h"

struct RawTexHeader {
    char magic[4];
    uint16_t width;
    uint16_t height;
    uint16_t origW;
    uint16_t origH;
};

AssetConverterState::AssetConverterState(const std::string& startDir) {
    rootDir = startDir;
    if (rootDir.back() != '/') rootDir += '/';
    currentDir = rootDir;
}

void AssetConverterState::init() {
    VCRFontFix();
    
    sheetIcons = SpritesheetCache::get().load("preload/images/menus/fileIcons");
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
            f.imageFormat = 0; // RGBA8
            f.audioHz = 0;     // Original
            
            // Restore custom settings if any
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
            files.push_back(f);
        }
        closedir(dir);
    }

    std::sort(files.begin(), files.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
        return a.name < b.name;
    });
}

int AssetConverterState::getIconForFile(const std::string& name, bool isDir) {
    if (isDir) return iconFolder;
    if (name.find(".png") != std::string::npos || name.find(".jpg") != std::string::npos || name.find(".rawtex") != std::string::npos) return iconImage;
    if (name.find(".ogg") != std::string::npos || name.find(".adp") != std::string::npos) return iconSound;
    if (name.find(".json") != std::string::npos) return iconJson;
    if (name.find(".xml") != std::string::npos) return iconXml;
    if (name.find(".lua") != std::string::npos) return iconLua;
    if (name.find(".txt") != std::string::npos) return iconTxt;
    if (name.find(".snaky") != std::string::npos || name.find(".mp4") != std::string::npos) return iconVideo;
    return iconUnknow;
}

void AssetConverterState::update(float dt) {
    u32 kDown = hidKeysDown();

    if (isConverting) {
        processConversion();
        return;
    }

    if (isConfiguring) {
        if (kDown & KEY_B) {
            isConfiguring = false;
            stopAudioSample();
            if (sampleTex) { C3D_TexDelete(sampleTex); delete sampleTex; sampleTex = nullptr; }
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        }
        
        FileEntry& curF = files[curSelected];
        bool isImage = (curF.name.find(".png") != std::string::npos);
        bool isAudio = (curF.name.find(".ogg") != std::string::npos);

        if (kDown & KEY_LEFT) {
            if (isImage) {
                curF.imageFormat = std::max(0, curF.imageFormat - 1);
                generateImageSample(currentDir + curF.name, curF.imageFormat);
            }
            if (isAudio) {
                curF.audioHz = std::max(0, curF.audioHz - 1);
                playAudioSample(currentDir + curF.name, curF.audioHz);
            }
            for (auto& f : files) {
                if (f.isSelected && !f.isDirectory && ((isImage && f.name.find(".png") != std::string::npos) || (isAudio && f.name.find(".ogg") != std::string::npos))) {
                    if (isImage) f.imageFormat = curF.imageFormat;
                    if (isAudio) f.audioHz = curF.audioHz;
                    customSettings[currentDir + f.name] = {f.imageFormat, f.audioHz};
                }
            }
            customSettings[currentDir + curF.name] = {curF.imageFormat, curF.audioHz};
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & KEY_RIGHT) {
            if (isImage) {
                curF.imageFormat = std::min(2, curF.imageFormat + 1);
                generateImageSample(currentDir + curF.name, curF.imageFormat);
            }
            if (isAudio) {
                curF.audioHz = std::min(2, curF.audioHz + 1);
                playAudioSample(currentDir + curF.name, curF.audioHz);
            }
            for (auto& f : files) {
                if (f.isSelected && !f.isDirectory && ((isImage && f.name.find(".png") != std::string::npos) || (isAudio && f.name.find(".ogg") != std::string::npos))) {
                    if (isImage) f.imageFormat = curF.imageFormat;
                    if (isAudio) f.audioHz = curF.audioHz;
                    customSettings[currentDir + f.name] = {f.imageFormat, f.audioHz};
                }
            }
            customSettings[currentDir + curF.name] = {curF.imageFormat, curF.audioHz};
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        if (kDown & KEY_Y) {
            if (isImage) generateImageSample(currentDir + curF.name, curF.imageFormat);
            if (isAudio) playAudioSample(currentDir + curF.name, curF.audioHz);
        }
        return;
    }

    if (kDown & KEY_START) {
        startConversion();
        return;
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

        if (kDown & KEY_A) {
            if (files[curSelected].isDirectory) {
                currentDir += files[curSelected].name + "/";
                loadDirectory(currentDir);
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            }
        }

        if (kDown & KEY_X) {
            files[curSelected].isSelected = !files[curSelected].isSelected;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        if (kDown & KEY_Y) {
            if (!files[curSelected].isDirectory && 
                (files[curSelected].name.find(".png") != std::string::npos || files[curSelected].name.find(".ogg") != std::string::npos)) {
                isConfiguring = true;
                if (sampleSheet) { C2D_SpriteSheetFree(sampleSheet); sampleSheet = nullptr; }
                sampleTex = nullptr;
                stopAudioSample();
                
                FileEntry& curF = files[curSelected];
                if (curF.name.find(".png") != std::string::npos) generateImageSample(currentDir + curF.name, curF.imageFormat);
                if (curF.name.find(".ogg") != std::string::npos) playAudioSample(currentDir + curF.name, curF.audioHz);
                
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            }
        }
    }

    if (keyJustPressed(KEY_B)) {
        if (currentDir == rootDir) {
            switchState(new ModsMenuState());
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        } else {
            // Go up a directory
            std::string path = currentDir;
            path.pop_back(); // remove trailing slash
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
    ClearTextBuf();

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(20, 20, 25, 255));
    
    // Draw Background (like ModsMenuState)
    CachedSpritesheet* menuBGB = SpritesheetCache::get().load("shared/images/menuBGB");
    if (menuBGB && !menuBGB->frames.empty()) {
        Frame& f = menuBGB->frames[0];
        C2D_ImageTint tint; C2D_PlainImageTint(&tint, C2D_Color32(100, 100, 150, 255), 1.0f);
        drawFrameAt(f, 0, 0, 0.1f, &tint);
    }

    if (isConverting) {
        AddText("CONVERTING BATCH...", 200, 80, 0.7f, true, 2.0f, CWhite, 0.0f);
        AddText(conversionStatus, 200, 110, 0.45f, true, 1.5f, CWhite, 0.0f);
        
        float bx = 50, by = 140, bw = 300, bh = 20;
        C2D_DrawRectSolid(bx, by, 0.5f, bw, bh, C2D_Color32(60, 60, 60, 255));
        C2D_DrawRectSolid(bx, by, 0.51f, bw * conversionProgress, bh, CYellow);
        
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, C2D_Color32(20, 20, 25, 255));
        return;
    }

    std::string displayPath = currentDir.substr(rootDir.size());
    if (displayPath.empty()) displayPath = "/";
    AddText("Path: " + displayPath, 10, 10, 0.45f, false, 1.0f, CWhite, 0.0f);

    float centerY = 120.0f;
    for (int i = 0; i < (int)files.size(); i++) {
        float dist = (i - lerpSelected);
        if (dist > 6 || dist < -6) continue;

        float y = centerY + (dist * 20.0f);
        float scale = (i == curSelected) ? 0.6f : 0.5f;
        u32 color = (i == curSelected) ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(150, 150, 150, 255);
        if (files[i].isSelected) color = C2D_Color32(255, 255, 0, 255);

        int iconIdx = getIconForFile(files[i].name, files[i].isDirectory);
        if (sheetIcons) {
            DrawFrameCentered(sheetIcons, iconIdx, 20, y, 0.5f, 0.8f);
        }

        AddText(files[i].name, 35, y - 5, scale, false, 1.0f, color, 0.0f);
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(20, 20, 25, 255));
    
    if (menuBGB && !menuBGB->frames.empty()) {
        Frame& f = menuBGB->frames[0];
        C2D_ImageTint tint; C2D_PlainImageTint(&tint, C2D_Color32(80, 80, 120, 255), 1.0f);
        drawFrameAt(f, 0, -40, 0.1f, &tint);
    }

    if (isConfiguring) {
        FileEntry& curF = files[curSelected];
        AddText("- CONFIGURATION -", 160, 20, 0.6f, true, 2.0f, CYellow, 0.0f);
        AddText(curF.name, 160, 45, 0.45f, true, 1.0f, CWhite, 0.0f);

        if (curF.name.find(".png") != std::string::npos) {
            std::string fmt = "RGBA8 (Best Quality)";
            if (curF.imageFormat == 1) fmt = "RGBA4444 (16-bit, Good)";
            if (curF.imageFormat == 2) fmt = "RGB565 (16-bit, No Alpha)";
            
            AddText("< Format: " + fmt + " >", 160, 80, 0.5f, true, 1.0f, CWhite, 0.0f);
            AddText("Press Y to generate sample", 160, 110, 0.4f, true, 1.0f, C2D_Color32(150, 150, 150, 255), 0.0f);
            
            if (sampleTex) {
                C2D_Image img; img.tex = sampleTex; img.subtex = &sampleSubtex;
                C2D_DrawImageAt(img, 160 - (sampleSubtex.width/2), 160 - (sampleSubtex.height/2), 0.5f);
            }
        } else if (curF.name.find(".ogg") != std::string::npos) {
            std::string hzStr = "Original";
            if (curF.audioHz == 1) hzStr = "22050 Hz";
            if (curF.audioHz == 2) hzStr = "11025 Hz";
            
            AddText("< Rate: " + hzStr + " >", 160, 80, 0.5f, true, 1.0f, CWhite, 0.0f);
            AddText("Press Y to play sample", 160, 110, 0.4f, true, 1.0f, C2D_Color32(150, 150, 150, 255), 0.0f);
        }
        
        AddText("B to Cancel", 160, 220, 0.4f, true, 1.0f, CWhite, 0.0f);
    } else {
        AddText("- ADVANCED CONVERTER -", 160, 20, 0.6f, true, 2.0f, CYellow, 0.0f);
        AddText("A: Enter Dir", 160, 60, 0.5f, true, 1.0f, CWhite, 0.0f);
        AddText("X: Toggle Selection", 160, 85, 0.5f, true, 1.0f, CWhite, 0.0f);
        AddText("Y: Configure File", 160, 110, 0.5f, true, 1.0f, CWhite, 0.0f);
        AddText("START: Convert Selected", 160, 145, 0.5f, true, 1.0f, C2D_Color32(0, 255, 0, 255), 0.0f);
        AddText("B: Back / Up Dir", 160, 220, 0.4f, true, 1.0f, CWhite, 0.0f);
    }
}

void AssetConverterState::exitState() {
    // SpritesheetCache manages sheetIcons, no need to free it.
    if (sampleSheet) { C2D_SpriteSheetFree(sampleSheet); sampleSheet = nullptr; }
    stopAudioSample();
    if (vcrFontBuf) C2D_TextBufDelete(vcrFontBuf);
}

void AssetConverterState::generateImageSample(const std::string& path, int format) {
    if (sampleSheet) { C2D_SpriteSheetFree(sampleSheet); sampleSheet = nullptr; }
    sampleTex = nullptr;
    
    std::string samplePath = "romfs:/preload/images/ColorTest_RGBA8.t3x";
    if (format == 1) samplePath = "romfs:/preload/images/ColorTest_RGBA4444.t3x";
    if (format == 2) samplePath = "romfs:/preload/images/ColorTest_RGB565.t3x";
    
    sampleSheet = C2D_SpriteSheetLoad(samplePath.c_str());
    if (sampleSheet) {
        C2D_Image img = C2D_SpriteSheetGetImage(sampleSheet, 0);
        sampleSubtex = *img.subtex;
        sampleTex = (C3D_Tex*)img.tex;
        sampleW = img.subtex->width;
        sampleH = img.subtex->height;
    }
}

void AssetConverterState::playAudioSample(const std::string& path, int hzMode) {
    // Can't resample on the fly, so just play the original file as a reference FUCK
    MusicPlayer::play(path.c_str());
}

void AssetConverterState::stopAudioSample() {
    MusicPlayer::stop();
}

void AssetConverterState::startConversion() {
    convertQueue.clear();
    
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
                } else if (name.find(".png") != std::string::npos || name.find(".ogg") != std::string::npos) {
                    convertQueue.push_back(fullPath);
                }
            }
            closedir(dir);
        }
    }
    
    if (convertQueue.empty()) return;

    isConverting = true;
    conversionProgress = 0.0f;
    currentConvertIdx = 0;
    conversionStatus = "Initializing...";
}

void AssetConverterState::processConversion() {
    if (isAudioPhase) {
        if (!convFIn || !convFOut || !convVfPtr) {
            isAudioPhase = false;
            currentConvertIdx++;
            return;
        }

        OggVorbis_File* vf = (OggVorbis_File*)convVfPtr;
        AdpcmEncoder::State* state = (AdpcmEncoder::State*)convStatePtr;
        
        const int CHUNK_SAMPLES = 8192;
        int16_t* pcmBuffer = (int16_t*)malloc(CHUNK_SAMPLES * convChannels * sizeof(int16_t));

        int bitstream;
        long read = ov_read(vf, (char*)pcmBuffer, CHUNK_SAMPLES * convChannels * sizeof(int16_t), &bitstream);
        
        if (read > 0) {
            int samplesRead = read / (convChannels * sizeof(int16_t));
            
            vorbis_info* vi = ov_info(vf, -1);
            int origRate = vi->rate;
            
            int targetRate = origRate;
            auto it = customSettings.find(convertQueue[currentConvertIdx]);
            if (it != customSettings.end()) {
                if (it->second.second == 1) targetRate = 22050;
                if (it->second.second == 2) targetRate = 11025;
            }
            
            float step = 1.0f;
            if (targetRate < origRate) step = (float)origRate / targetRate;

            int newSamples = (int)(samplesRead / step);
            int16_t* monoBuffer = (int16_t*)malloc(newSamples * sizeof(int16_t));

            for (int i = 0; i < newSamples; i++) {
                int srcIdx = (int)(i * step);
                if (srcIdx >= samplesRead) srcIdx = samplesRead - 1;
                
                if (convChannels == 2) {
                    monoBuffer[i] = (int16_t)(((int32_t)pcmBuffer[srcIdx*2] + (int32_t)pcmBuffer[srcIdx*2+1]) / 2);
                } else {
                    monoBuffer[i] = pcmBuffer[srcIdx];
                }
            }

            std::vector<uint8_t> adpcm = AdpcmEncoder::encodeIMA(monoBuffer, newSamples, *state);
            convSamplesProcessed += newSamples;
            
            if (read < CHUNK_SAMPLES * convChannels * sizeof(int16_t) || convSamplesProcessed >= convTotalSamples) {
                AdpcmEncoder::flush(adpcm, *state);
            }
            fwrite(adpcm.data(), 1, adpcm.size(), convFOut);
            
            conversionProgress = (float)currentConvertIdx / convertQueue.size();
            conversionProgress += 0.5f * (1.0f / convertQueue.size()); // rough progress
            conversionStatus = "Converting Audio... ";
            free(monoBuffer);
        } else {
            fclose(convFOut);
            ov_clear(vf);
            free(convVfPtr);
            delete (AdpcmEncoder::State*)convStatePtr;
            remove(convertQueue[currentConvertIdx].c_str());
            convFIn = nullptr; convFOut = nullptr; convVfPtr = nullptr; convStatePtr = nullptr;
            isAudioPhase = false;
            currentConvertIdx++;
        }

        free(pcmBuffer);
        return;
    }

    if (currentConvertIdx >= (int)convertQueue.size()) {
        isConverting = false;
        for (auto& f : files) f.isSelected = false;
        loadDirectory(currentDir);
        return;
    }
    
    std::string fullPath = convertQueue[currentConvertIdx];
    std::string file = fullPath.substr(rootDir.size());

    conversionStatus = "Checking: " + file;
    conversionProgress = (float)currentConvertIdx / convertQueue.size();

    if (Paths::fileExists(fullPath)) {
        if (file.find(".png") != std::string::npos || file.find(".t3x") != std::string::npos) {
            std::string outPath = currentDir + file.substr(0, file.find_last_of(".")) + ".rawtex";
            conversionStatus = "Converting Image: " + file;
            
            int w, h, c;
            unsigned char* data = nullptr;
            if (file.substr(file.find_last_of(".") + 1) != "t3x") {
                data = stbi_load(fullPath.c_str(), &w, &h, &c, 4);
            }

            if (data) {
                int pw = 1, ph = 1;
                while(pw < w) pw *= 2;
                while(ph < h) ph *= 2;

                FILE* fOut = fopen(outPath.c_str(), "wb");
                if (fOut) {
                    int imgFormat = 0;
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
                    } else if (imgFormat == 1) { // RGBA4444
                        uint16_t* swizzled = (uint16_t*)linearAlloc(pw * ph * 2);
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
                    } else if (imgFormat == 2) { // RGB565
                        uint16_t* swizzled = (uint16_t*)linearAlloc(pw * ph * 2);
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
                    fclose(fOut);
                    remove(fullPath.c_str());
                }
                stbi_image_free(data);
            }
            currentConvertIdx++;
        } else if (file.find(".ogg") != std::string::npos) {
            std::string outPath = currentDir + file.substr(0, file.find_last_of(".")) + ".adp";
            conversionStatus = "Opening Audio: " + file;

            convFIn = fopen(fullPath.c_str(), "rb");
            if (convFIn) {
                OggVorbis_File* vf = (OggVorbis_File*)malloc(sizeof(OggVorbis_File));
                if (ov_open(convFIn, vf, NULL, 0) == 0) {
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
                        return; // Start incremental phase next frame
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
}
