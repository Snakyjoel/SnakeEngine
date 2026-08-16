#include "MusicPlayerState.hpp"
#include "../debug/DebugMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/ModHandler.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <3ds.h>

MusicPlayerState::MusicPlayerState(const std::string& startDir) {
    rootDir = startDir;
    if (rootDir.back() != '/') rootDir += '/';
    currentDir = rootDir;
}

MusicPlayerState::~MusicPlayerState() {
    if (textBuf) C2D_TextBufDelete(textBuf);
    if (vcrFontBuf) C2D_TextBufDelete(vcrFontBuf);
    
    // Always restore sleep mode when exiting
    aptSetSleepAllowed(true);
    extern bool g_keepMusicPlayingDuringSleep;
    g_keepMusicPlayingDuringSleep = false;
}

void MusicPlayerState::init() {
    VCRFontFix();
    textBuf = C2D_TextBufNew(4096);
    MusicPlayer::stop(); // Make sure no menu music is playing
    
    // Load icons
    sheetIconsData = SpritesheetCache::get().load("preload/images/menus/fileIcons");
    if (sheetIconsData) {
        for (int i = 0; i < (int)sheetIconsData->frames.size(); i++) {
            auto& name = sheetIconsData->frames[i].name;
            if (name == "folder") iconFolder = i;
            else if (name == "sound") iconSound = i;
        }
    }
    
    loadDirectory(currentDir);
}

void MusicPlayerState::loadDirectory(const std::string& path) {
    files.clear();
    curSelected = 0;
    lerpSelected = 0.0f;
    currentDir = path;

    // Add back directory if we're not at root
    if (currentDir != "sdmc:/" && currentDir != "romfs:/") {
        files.push_back({"..", true});
    }

    DIR* dir = opendir(path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;

            std::string fullPath = path + name;
            bool isDir = (entry->d_type == DT_DIR);
            if (entry->d_type == DT_UNKNOWN) {
                struct stat st;
                if (stat(fullPath.c_str(), &st) == 0) {
                    isDir = S_ISDIR(st.st_mode);
                }
            }

            if (isDir) {
                files.push_back({name, true});
            } else if (name.size() > 4) {
                std::string ext = name.substr(name.size() - 4);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".ogg" || ext == ".adp") {
                    files.push_back({name, false});
                }
            }
        }
        closedir(dir);
    }

    std::sort(files.begin(), files.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.name == "..") return true;
        if (b.name == "..") return false;
        if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
        return a.name < b.name;
    });
}

int MusicPlayerState::getIconForFile(const std::string& name, bool isDir) {
    if (isDir) return iconFolder;
    return iconSound; // it's always .ogg or .adp
}

void MusicPlayerState::update(float dt) {
    gridOffset = fmodf(gridOffset + dt * 25.0f, 80.0f);
    lerpSelected += ((float)curSelected - lerpSelected) * dt * 10.0f;
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();

    // Volume Boost (L/R)
    holdVolumeBoost = false;
    if (kHeld & KEY_L) {
        volumeBoost = 2.0f; // 200% volume
        holdVolumeBoost = true;
    } else if (kHeld & KEY_R) {
        volumeBoost = 3.0f; // 300% volume
        holdVolumeBoost = true;
    } else {
        volumeBoost = 1.0f;
    }

    if (isPlaying) {
        if (currentFile.find(".ogg") != std::string::npos) {
            MusicPlayer::setVolume(volumeBoost);
        } else {
            AudioEngine::setVocalsVolume(volumeBoost);
            // Assuming we use AudioEngine for ADP fallback or direct if AudioEngine handles ADP
        }
    }

    if (kDown & KEY_Y) {
        sleepModeActive = !sleepModeActive;
        aptSetSleepAllowed(!sleepModeActive);
        
        extern bool g_keepMusicPlayingDuringSleep;
        g_keepMusicPlayingDuringSleep = sleepModeActive;
        
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
    }

    if (kDown & KEY_B) {
        if (currentDir == "sdmc:/" || currentDir == "romfs:/") {
            AudioEngine::exit();
            MusicPlayer::stop();
            MusicBeatState::switchState(new DebugMenuState());
            return;
        } else {
            // Go up
            size_t lastSlash = currentDir.find_last_of('/', currentDir.length() - 2);
            if (lastSlash != std::string::npos) {
                std::string parent = currentDir.substr(0, lastSlash + 1);
                loadDirectory(parent);
                AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            }
        }
    }

    if (!files.empty()) {
        if (kDown & (KEY_DUP | KEY_CPAD_UP | KEY_DLEFT | KEY_CPAD_LEFT)) {
            curSelected--;
            if (curSelected < 0) curSelected = files.size() - 1;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN | KEY_DRIGHT | KEY_CPAD_RIGHT)) {
            curSelected++;
            if (curSelected >= (int)files.size()) curSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        
        if (kDown & KEY_A) {
            auto& f = files[curSelected];
            if (f.isDirectory) {
                if (f.name == "..") {
                    size_t lastSlash = currentDir.find_last_of('/', currentDir.length() - 2);
                    if (lastSlash != std::string::npos) {
                        loadDirectory(currentDir.substr(0, lastSlash + 1));
                    }
                } else {
                    loadDirectory(currentDir + f.name + "/");
                }
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            } else {
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                playSelected();
            }
        }
    }

    if (kDown & KEY_START) {
        MusicPlayer::stop();
        AudioEngine::exit();
        isPlaying = false;
    }

    MusicPlayer::update();
    AudioEngine::update();
}

void MusicPlayerState::playSelected() {
    MusicPlayer::stop();
    AudioEngine::exit(); 
    
    std::string fullPath = currentDir + files[curSelected].name;
    
    if (fullPath.find(".ogg") != std::string::npos) {
        if (MusicPlayer::play(fullPath.c_str(), volumeBoost)) {
            isPlaying = true;
            currentFile = files[curSelected].name;
        }
    } else if (fullPath.find(".adp") != std::string::npos) {
        if (AudioEngine::init(fullPath.c_str())) {
            AudioEngine::setVocalsVolume(volumeBoost);
            AudioEngine::start();
            isPlaying = true;
            currentFile = files[curSelected].name;
        }
    }
}

void MusicPlayerState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(20, 20, 30, 255));
    
    // Header
    AddText("Music Player / File Explorer", 10, 10, 0.5f, false, 1.0f, CWhite, 0.0f);
    
    std::string cleanPath = currentDir;
    if (cleanPath.length() > 45) {
        cleanPath = "..." + cleanPath.substr(cleanPath.length() - 42);
    }
    AddText(cleanPath, 10, 30, 0.4f, false, 1.0f, C2D_Color32(200, 200, 255, 255), 0.0f);
    
    // Status
    std::string statusStr = "Status: ";
    statusStr += isPlaying ? "Playing" : "Stopped";
    if (sleepModeActive) statusStr += " | SLEEP: OFF (Plays Closed)";
    else statusStr += " | SLEEP: ON (Lid Sleep)";
    
    AddText(statusStr, 10, 50, 0.4f, false, 1.0f, C2D_Color32(150, 255, 150, 255), 0.0f);

    // Volume Boost Info
    if (holdVolumeBoost) {
        std::string volStr = "VOLUME BOOST: " + std::to_string((int)(volumeBoost * 100)) + "%";
        AddText(volStr, 10, 70, 0.4f, false, 1.0f, CRed, 0.0f);
    } else {
        AddText("Hold L(200%) or R(300%) to boost volume", 10, 70, 0.4f, false, 1.0f, CGray, 0.0f);
    }

    // Time progress info
    if (isPlaying) {
        double curTimeMs = 0;
        double totalTimeMs = 0;
        if (currentFile.find(".ogg") != std::string::npos) {
            curTimeMs = MusicPlayer::getPosition();
            totalTimeMs = MusicPlayer::getDuration();
        } else {
            curTimeMs = AudioEngine::getSongPosition();
            totalTimeMs = AudioEngine::getTotalTime();
        }
        char timeStr[64];
        int curSec = (int)(curTimeMs / 1000.0) % 60;
        int curMin = (int)(curTimeMs / 1000.0) / 60;
        int totSec = (int)(totalTimeMs / 1000.0) % 60;
        int totMin = (int)(totalTimeMs / 1000.0) / 60;
        sprintf(timeStr, "%02d:%02d / %02d:%02d", curMin, curSec, totMin, totSec);
        AddText(timeStr, 280, 70, 0.4f, false, 1.0f, CWhite, 0.0f);

        float prog = 0.0f;
        if (totalTimeMs > 0) prog = curTimeMs / totalTimeMs;
        if (prog > 1.0f) prog = 1.0f;
        if (prog < 0.0f) prog = 0.0f;
        
        float barW = 380;
        float barH = 8;
        float barX = 10;
        float barY = 95;
        C2D_DrawRectSolid(barX, barY, 0, barW, barH, C2D_Color32(50, 50, 50, 255));
        C2D_DrawRectSolid(barX, barY, 0, barW * prog, barH, C2D_Color32(100, 255, 150, 255));
    }

    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(10, 10, 20, 255));

    int maxItems = 9;
    int startIndex = curSelected - maxItems / 2;
    if (startIndex < 0) startIndex = 0;
    if (startIndex + maxItems > (int)files.size()) {
        startIndex = (int)files.size() - maxItems;
        if (startIndex < 0) startIndex = 0;
    }

    for (int i = 0; i < maxItems && (startIndex + i) < (int)files.size(); i++) {
        int idx = startIndex + i;
        auto& f = files[idx];
        
        float yPos = 15 + i * 22;
        bool isSel = (idx == curSelected);
        
        // Selection highlight box
        if (isSel) {
            C2D_DrawRectSolid(2, yPos - 2, 0, 316, 20, C2D_Color32(50, 50, 80, 255));
        }

        u32 color = isSel ? C2D_Color32(255, 255, 100, 255) : C2D_Color32(240, 240, 240, 255);
        if (f.isDirectory) color = isSel ? C2D_Color32(255, 200, 0, 255) : C2D_Color32(200, 200, 150, 255);
        
        float xOffset = isSel ? 10.0f : 0.0f;
        std::string displayName = f.name;
        if (isSel) {
            displayName = "> " + displayName;
        }

        // Draw Icon
        if (sheetIconsData && !sheetIconsData->frames.empty()) {
            int iconId = getIconForFile(f.name, f.isDirectory);
            if (iconId >= 0 && iconId < (int)sheetIconsData->frames.size()) {
                const Frame& iconFrame = sheetIconsData->frames[iconId];
                drawFrameAt(iconFrame, 10 + xOffset, yPos, 0.5f, nullptr, 0.7f, 0.7f);
            }
        }

        AddText(displayName, 30 + xOffset, yPos, 0.4f, false, 1.0f, color, 0.0f);
    }

    // Controls guide bar at the very bottom
    C2D_DrawRectSolid(0, 220, 0, 320, 20, C2D_Color32(40, 40, 55, 255));
    AddText("A: Play/Enter  B: Up  Y: Sleep  L/R: Vol", 10, 222, 0.35f, false, 1.0f, CWhite, 0.0f);
}
