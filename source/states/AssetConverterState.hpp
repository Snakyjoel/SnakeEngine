#pragma once
#include "../backend/MusicBeatState.hpp"
#include <vector>
#include <string>
#include <citro2d.h>
#include <dirent.h>
#include <map>
#include "../backend/SpritesheetCache.hpp"

struct FileEntry {
    std::string name;
    bool isDirectory;
    bool isSelected;
    // For images
    int imageFormat; // 0 = RGBA8, 1 = RGBA4444, 2 = RGB565
    // For audio
    int audioHz; // 0 = Original, 1 = 22050, 2 = 11025
};

class AssetConverterState : public MusicBeatState {
public:
    AssetConverterState(const std::string& startDir);
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    std::string currentDir;
    std::string rootDir;
    
    C2D_Font vcrFont = nullptr;
    C2D_TextBuf vcrFontBuf = nullptr;
    std::vector<FileEntry> files;
    int curSelected = 0;
    float lerpSelected = 0;

    CachedSpritesheet* sheetIcons = nullptr;
    int iconFolder = 0;
    int iconImage = 0;
    int iconJson = 0;
    int iconLua = 0;
    int iconSound = 0;
    int iconTxt = 0;
    int iconUnknow = 0;
    int iconVideo = 0;
    int iconXml = 0;
    
    std::map<std::string, std::pair<int, int>> customSettings;

    bool isConfiguring = false;
    int configSelection = 0;

    void loadDirectory(const std::string& path);
    void reloadIcons();
    int getIconForFile(const std::string& name, bool isDir);

    // Quick Sample
    C3D_Tex* sampleTex = nullptr;
    C2D_SpriteSheet sampleSheet = nullptr;
    Tex3DS_SubTexture sampleSubtex;
    int sampleW = 0, sampleH = 0;
    void generateImageSample(const std::string& path, int format);
    void playAudioSample(const std::string& path, int hzMode);
    void stopAudioSample();

    // Conversion
    bool isConverting = false;
    float conversionProgress = 0.0f;
    std::string conversionStatus = "";
    std::vector<std::string> convertQueue;
    int currentConvertIdx = 0;
    
    // Audio Conversion State
    FILE* convFIn = nullptr;
    FILE* convFOut = nullptr;
    void* convVfPtr = nullptr;
    uint32_t convSamplesProcessed = 0;
    uint32_t convTotalSamples = 0;
    int convChannels = 0;
    void* convStatePtr = nullptr;
    bool isAudioPhase = false;

    void startConversion();
    void processConversion();
};
