#include "ImageViewerState.hpp"
#include "../debug/DebugMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/stb_image.h"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

void ImageViewerState::init() {
    textBuf = C2D_TextBufNew(4096);
    MusicPlayer::stop();
    if (modFolder.empty()) {
        printf("\x1b[1;1HScanning Image files in ALL mods...\x1b[K\n");
    } else {
        printf("\x1b[1;1HScanning Image files in %s...\x1b[K\n", modFolder.c_str());
    }
    scanFiles();
    printf("\x1b[2;1HFound %d files.\x1b[K\n", (int)imageFiles.size());
}

void ImageViewerState::scanFiles() {
    imageFiles.clear();
    std::string modDir = std::string("sdmc:/SnakeEngine/") + (modFolder.empty() ? "" : modFolder + "/");
    
    std::vector<std::string> dirsToScan = {modDir, "romfs:/preload/images/"};
    while(!dirsToScan.empty()) {
        std::string currentDir = dirsToScan.back();
        dirsToScan.pop_back();
        
        DIR* dp = opendir(currentDir.c_str());
        if (!dp) continue;
        
        struct dirent* entry;
        while((entry = readdir(dp))) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            
            std::string fullPath = currentDir + name;
            bool isDir = (entry->d_type == DT_DIR);
            if (entry->d_type == DT_UNKNOWN) {
                struct stat st;
                if (stat(fullPath.c_str(), &st) == 0) {
                    isDir = S_ISDIR(st.st_mode);
                }
            }

            if (isDir) {
                dirsToScan.push_back(fullPath + "/");
            } else if ((name.size() > 4 && name.substr(name.size() - 4) == ".png") ||
                       (name.size() > 4 && name.substr(name.size() - 4) == ".t3x")) {
                if (currentDir.find("sdmc:/") == 0) {
                    imageFiles.push_back(fullPath.substr(modDir.size()));
                } else {
                    imageFiles.push_back(fullPath);
                }
            }
        }
        closedir(dp);
    }
    std::sort(imageFiles.begin(), imageFiles.end());
}

void ImageViewerState::update(float dt) {
    (void)dt;
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();

    if (isViewing) {
        if (kDown & KEY_B) {
            closeImage();
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            return;
        }
        
        if (kHeld & KEY_UP) panY += 5.0f * dt * 60.0f;
        if (kHeld & KEY_DOWN) panY -= 5.0f * dt * 60.0f;
        if (kHeld & KEY_LEFT) panX += 5.0f * dt * 60.0f;
        if (kHeld & KEY_RIGHT) panX -= 5.0f * dt * 60.0f;

        if (kHeld & KEY_L) zoomLevel -= 1.0f * dt;
        if (kHeld & KEY_R) zoomLevel += 1.0f * dt;
        if (zoomLevel < 0.1f) zoomLevel = 0.1f;
        if (zoomLevel > 5.0f) zoomLevel = 5.0f;
        return;
    }

    if (kDown & KEY_B) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        MusicBeatState::switchState(new DebugMenuState());
        return;
    }

    if (!imageFiles.empty()) {
        if (kDown & (KEY_DUP | KEY_CPAD_UP | KEY_DLEFT | KEY_CPAD_LEFT)) {
            curSelected--;
            if (curSelected < 0) curSelected = imageFiles.size() - 1;
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN | KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            curSelected++;
            if (curSelected >= (int)imageFiles.size()) curSelected = 0;
        }
        if (kDown & KEY_A) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            openImage();
        }
        if (kDown & KEY_X) {
            std::string path = imageFiles[curSelected];
            if (path.find("romfs:/") != 0) path = std::string("sdmc:/SnakeEngine/") + (modFolder.empty() ? "" : modFolder + "/") + path;
            DebugMenuState::dumpFile(path);
        }
    }
}

void ImageViewerState::openImage() {
    closeImage();
    
    std::string fullPath;
    if (imageFiles[curSelected].find("romfs:/") == 0) {
        fullPath = imageFiles[curSelected];
    } else {
        fullPath = std::string("sdmc:/SnakeEngine/") + (modFolder.empty() ? "" : modFolder + "/") + imageFiles[curSelected];
    }

    if (fullPath.find(".t3x") != std::string::npos) {
        viewSheet = C2D_SpriteSheetLoad(fullPath.c_str());
        if (viewSheet) {
            viewImg = C2D_SpriteSheetGetImage(viewSheet, 0);
            isViewing = true;
        }
    } else if (fullPath.find(".png") != std::string::npos) {
        FILE* f = fopen(fullPath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = ftell(f);
            fseek(f, 0, SEEK_SET);
            unsigned char* fileData = (unsigned char*)malloc(size);
            fread(fileData, 1, size, f);
            fclose(f);

            int w, h, c;
            unsigned char* data = stbi_load_from_memory(fileData, size, &w, &h, &c, 4);
            free(fileData);
            
            if (data) {
                int maxDim = std::max(w, h);
                float scale = 1.0f;
                int scaledW = w;
                int scaledH = h;
                
                if (maxDim > 1024) {
                    scale = 1024.0f / maxDim;
                    scaledW = w * scale;
                    scaledH = h * scale;
                }

                int pw = 1, ph = 1;
                while(pw < scaledW) pw *= 2;
                while(ph < scaledH) ph *= 2;

                viewTex = new C3D_Tex();
                C3D_TexInit(viewTex, pw, ph, GPU_RGBA8);
                C3D_TexSetFilter(viewTex, GPU_LINEAR, GPU_LINEAR);
                
                uint32_t* swizzled = (uint32_t*)linearAlloc(pw * ph * 4);
                memset(swizzled, 0, pw * ph * 4);
                
                for(int y=0; y<scaledH; y++) {
                    for(int x=0; x<scaledW; x++) {
                        int origX = (int)(x / scale);
                        int origY = (int)(y / scale);
                        if (origX >= w) origX = w - 1;
                        if (origY >= h) origY = h - 1;
                        
                        int src = (origY * w + origX) * 4;
                        // Revert back to original layout that worked correctly
                        uint32_t px = (data[src] << 24) | (data[src+1] << 16) | (data[src+2] << 8) | data[src+3];
                        
                        uint32_t i = (x & 7) | ((y & 7) << 8);
                        i = (i ^ (i << 2)) & 0x1313;
                        i = (i ^ (i << 1)) & 0x1515;
                        
                        uint32_t tx = x >> 3;
                        uint32_t ty = y >> 3;
                        uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                        uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                        
                        swizzled[tile_start + local_idx] = px;
                    }
                }
                C3D_TexUpload(viewTex, swizzled);
                C3D_TexFlush(viewTex);
                linearFree(swizzled);
                stbi_image_free(data);

                viewSubtex = new Tex3DS_SubTexture();
                viewSubtex->width = scaledW; viewSubtex->height = scaledH;
                viewSubtex->left = 0.0f; viewSubtex->top = 1.0f;
                viewSubtex->right = (float)scaledW / pw; viewSubtex->bottom = 1.0f - ((float)scaledH / ph);

                viewImg = {viewTex, viewSubtex};
                isViewing = true;
            } else {
                printf("\x1b[1;1Hstbi_load failed: %s\x1b[K\n", stbi_failure_reason());
                svcSleepThread(2000000000); // 2s
            }
        } else {
            printf("\x1b[1;1Hfopen failed: %s\x1b[K\n", fullPath.c_str());
            svcSleepThread(2000000000); // 2s
        }
    }
    
    panX = 0;
    panY = 0;
    zoomLevel = 1.0f;
}

void ImageViewerState::closeImage() {
    isViewing = false;
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
}

void ImageViewerState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_TextBufClear(textBuf);

    if (isViewing) {
        C2D_SceneBegin(top);
        C2D_TargetClear(top, C2D_Color32(20, 20, 20, 255));
        
        for (float x = fmodf(panX, 32); x < 400; x += 32) {
            for (float y = fmodf(panY, 32); y < 240; y += 32) {
                if (((int)(x - panX) / 32 + (int)(y - panY) / 32) % 2 == 0) {
                    C2D_DrawRectSolid(x, y, 0.1f, 32, 32, C2D_Color32(40, 40, 40, 255));
                }
            }
        }
        
        float drawX = 200.0f - ((viewImg.subtex->width * zoomLevel) / 2.0f) + panX;
        float drawY = 120.0f - ((viewImg.subtex->height * zoomLevel) / 2.0f) + panY;
        C2D_DrawImageAt(viewImg, drawX, drawY, 0.5f, nullptr, zoomLevel, zoomLevel);
        
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, C2D_Color32(20, 20, 20, 255));
        
        C2D_Text t;
        C2D_TextParse(&t, textBuf, "D-Pad: Pan Image\nL/R: Zoom In/Out\nB: Close");
        C2D_TextOptimize(&t);
        C2D_DrawText(&t, C2D_WithColor, 10, 10, 0, 0.5f, 0.5f, C2D_Color32(150, 150, 150, 255));
        return;
    }

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(30, 30, 40, 255));
    
    C2D_DrawRectSolid(0, 0, 0, 400, 30, C2D_Color32(50, 50, 70, 255));
    C2D_Text txt;
    C2D_TextParse(&txt, textBuf, "IMAGE VIEWER - DEBUG");
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_AtBaseline | C2D_WithColor, 10, 22, 0, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));

    if (imageFiles.empty()) {
        C2D_Text t;
        C2D_TextParse(&t, textBuf, "No image files found.");
        C2D_TextOptimize(&t);
        C2D_DrawText(&t, C2D_WithColor, 20, 100, 0, 0.6f, 0.6f, C2D_Color32(255, 100, 100, 255));
    } else {
        for (int i = 0; i < (int)imageFiles.size(); i++) {
            if (i < curSelected - 8 || i > curSelected + 8) continue;
            
            u32 color = (i == curSelected) ? C2D_Color32(255, 255, 0, 255) : C2D_Color32(200, 200, 200, 255);
            float x = (i == curSelected) ? 20 : 10;
            float textY = 120 + (i - curSelected) * 20;

            C2D_Text t;
            C2D_TextParse(&t, textBuf, imageFiles[i].c_str());
            C2D_TextOptimize(&t);
            C2D_DrawText(&t, C2D_WithColor, x, textY, 0, 0.5f, 0.5f, color);
        }
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(20, 20, 30, 255));
    
    C2D_Text t;
    C2D_TextParse(&t, textBuf, "Select a file and press [A] to view\nPress [X] to dump to SD (RomFS only)\nPress [B] to return");
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, 10, 10, 0, 0.5f, 0.5f, C2D_Color32(150, 150, 150, 255));
}

void ImageViewerState::exitState() {
    closeImage();
    C2D_TextBufDelete(textBuf);
}
