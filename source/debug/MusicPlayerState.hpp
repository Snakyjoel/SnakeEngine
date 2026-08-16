#pragma once
#include "MusicBeatState.hpp"
#include "../backend/SpritesheetCache.hpp"
#include <citro2d.h>
#include <string>
#include <vector>

class MusicPlayerState : public MusicBeatState {
public:
    MusicPlayerState(const std::string& startDir = "sdmc:/SnakeEngine/");
    ~MusicPlayerState();

    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;

private:
    struct FileEntry {
        std::string name;
        bool isDirectory;
    };

    void loadDirectory(const std::string& path);
    void playSelected();
    int getIconForFile(const std::string& name, bool isDir);

    std::string rootDir;
    std::string currentDir;
    std::vector<FileEntry> files;
    
    int curSelected = 0;
    float lerpSelected = 0.0f;
    float gridOffset = 0.0f;

    bool isPlaying = false;
    std::string currentFile = "";
    
    C2D_TextBuf textBuf;
    CachedSpritesheet* sheetIconsData = nullptr;

    int iconFolder = 0, iconSound = 0;

    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;

    bool sleepModeActive = false;
    float volumeBoost = 1.0f;
    bool holdVolumeBoost = false;
};
