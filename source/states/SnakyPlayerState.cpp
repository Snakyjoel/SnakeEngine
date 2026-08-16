#include "SnakyPlayerState.hpp"
#include "../debug/DebugMenuState.hpp"
#include "VideoState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/ModHandler.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

void SnakyPlayerState::init() {
    textBuf = C2D_TextBufNew(4096);
    MusicPlayer::stop();
    if (modFolder.empty()) {
        printf("\x1b[1;1HScanning SNAKY files in ALL mods...\x1b[K\n");
    } else {
        printf("\x1b[1;1HScanning SNAKY files in %s...\x1b[K\n", modFolder.c_str());
    }
    scanFiles();
    printf("\x1b[2;1HFound %d files.\x1b[K\n", (int)snakyFiles.size());
}

void SnakyPlayerState::scanFiles() {
    snakyFiles.clear();
    std::string modDir = std::string("sdmc:/SnakeEngine/") + (modFolder.empty() ? "" : modFolder + "/");
    
    std::vector<std::string> dirsToScan = {modDir, "romfs:/preload/videos/"};
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
            } else if (name.size() > 6 && name.substr(name.size() - 6) == ".snaky") {
                if (currentDir.find("sdmc:/") == 0) {
                    snakyFiles.push_back(fullPath.substr(modDir.size()));
                } else {
                    snakyFiles.push_back(fullPath);
                }
            }
        }
        closedir(dp);
    }
    std::sort(snakyFiles.begin(), snakyFiles.end());
}

void SnakyPlayerState::update(float dt) {
    (void)dt;
    u32 kDown = hidKeysDown();

    if (kDown & KEY_B) {
        AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
        MusicBeatState::switchState(new DebugMenuState());
        return;
    }

    if (!snakyFiles.empty()) {
        if (kDown & (KEY_DUP | KEY_CPAD_UP | KEY_DLEFT | KEY_CPAD_LEFT)) {
            curSelected--;
            if (curSelected < 0) curSelected = snakyFiles.size() - 1;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN | KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            curSelected++;
            if (curSelected >= (int)snakyFiles.size()) curSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & KEY_A) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            playSelected();
        }
        if (kDown & KEY_X) {
            std::string path = snakyFiles[curSelected];
            if (path.find("romfs:/") != 0) path = std::string("sdmc:/SnakeEngine/") + (modFolder.empty() ? "" : modFolder + "/") + path;
            DebugMenuState::dumpFile(path);
        }
    }
}

void SnakyPlayerState::playSelected() {
    std::string fullPath;
    if (snakyFiles[curSelected].find("romfs:/") == 0) {
        fullPath = snakyFiles[curSelected];
    } else {
        fullPath = std::string("sdmc:/SnakeEngine/") + (modFolder.empty() ? "" : modFolder + "/") + snakyFiles[curSelected];
    }

    MusicBeatState::switchState(new VideoState(fullPath, new SnakyPlayerState(modFolder), true));
}

void SnakyPlayerState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_TextBufClear(textBuf);

    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(20, 20, 50, 255));
    
    C2D_DrawRectSolid(0, 0, 0, 400, 30, C2D_Color32(40, 40, 80, 255));
    C2D_Text txt;
    C2D_TextParse(&txt, textBuf, "SNAKY VIDEO PLAYER - DEBUG");
    C2D_TextOptimize(&txt);
    C2D_DrawText(&txt, C2D_AtBaseline | C2D_WithColor, 10, 22, 0, 0.6f, 0.6f, C2D_Color32(255, 255, 255, 255));

    if (snakyFiles.empty()) {
        C2D_Text t;
        C2D_TextParse(&t, textBuf, "No .snaky files found.");
        C2D_TextOptimize(&t);
        C2D_DrawText(&t, C2D_WithColor, 20, 100, 0, 0.6f, 0.6f, C2D_Color32(255, 100, 100, 255));
    } else {
        for (int i = 0; i < (int)snakyFiles.size(); i++) {
            if (i < curSelected - 8 || i > curSelected + 8) continue;
            
            u32 color = (i == curSelected) ? C2D_Color32(255, 255, 0, 255) : C2D_Color32(200, 200, 200, 255);
            float x = (i == curSelected) ? 20 : 10;
            float textY = 120 + (i - curSelected) * 20;

            C2D_Text t;
            C2D_TextParse(&t, textBuf, snakyFiles[i].c_str());
            C2D_TextOptimize(&t);
            C2D_DrawText(&t, C2D_WithColor, x, textY, 0, 0.5f, 0.5f, color);
        }
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(20, 20, 30, 255));
    
    C2D_Text t;
    C2D_TextParse(&t, textBuf, "Select a file and press [A] to play\nPress [X] to dump to SD (RomFS only)\nPress [B] to return");
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, 10, 10, 0, 0.5f, 0.5f, C2D_Color32(150, 150, 150, 255));
}

void SnakyPlayerState::exitState() {
    C2D_TextBufDelete(textBuf);
}
