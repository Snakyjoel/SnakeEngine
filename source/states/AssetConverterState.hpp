#pragma once
#include "../backend/MusicBeatState.hpp"
#include <vector>
#include <string>
#include <citro2d.h>
#include <dirent.h>
#include <map>
#include "../backend/SpritesheetCache.hpp"

enum class ConversionMode {
    ALL,
    FOLDER,
    SELECTED,
    SINGLE
};

struct FileEntry {
    std::string name;
    bool isDirectory = false;
    bool isSelected = false;
    bool isConvertible = false;
    int iconIndex = 0;
    // For images
    int imageFormat = 0; // 0 = RGBA8, 1 = RGBA4444, 2 = RGB565
    // For audio
    int audioHz = 0; // 0 = Original, 1 = 22050, 2 = 11025
};

class AssetConverterState : public MusicBeatState {
public:
    AssetConverterState(const std::string& startDir, bool audioOnly = false);
    ~AssetConverterState();
    void init() override;
    void update(float dt) override;
    void draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) override;
    void exitState() override;

private:
    std::string currentDir;
    std::string rootDir;
    bool isAudioMode = false;
    
    C2D_Font vcrFont = nullptr;
    C2D_TextBuf converterTextBuf = nullptr;
    std::vector<FileEntry> files;
    int curSelected = 0;
    float lerpSelected = 0;

    void drawText(const std::string& text, float x, float y, float scale, bool centered = false, u32 color = C2D_Color32(255,255,255,255), float depth = 0.85f, float maxWidth = 0.0f, bool rightAlign = false);
    void drawStyledButton(float x, float y, float w, float h, const std::string& title, const std::string& subtitle, u32 topCol, u32 botCol, u32 borderCol);

    CachedSpritesheet* sheetIcons = nullptr;
    CachedSpritesheet* menuBG = nullptr;
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

    // Conversion Options
    int maxTextureSize = 1024; // Strict max texture ceiling for 3DS (1024x1024)
    int globalFormat = 0;       // 0 = RGBA8, 1 = RGBA4444, 2 = RGB565

    bool scaleXmlCoords = true;

    void loadDirectory(const std::string& path);
    int getIconForFile(const std::string& name, bool isDir);
    bool isImageFile(const std::string& name, bool isDir);
    void selectAll(bool select);

    // Image Preview Overlay
    bool isPreviewing = false;
    std::string previewFileName = "";
    int previewW = 0;
    int previewH = 0;
    float previewPanX = 0.0f;
    float previewPanY = 0.0f;
    float previewZoom = 1.0f;
    C3D_Tex* viewTex = nullptr;
    Tex3DS_SubTexture* viewSubtex = nullptr;
    C2D_SpriteSheet viewSheet = nullptr;
    C2D_Image viewImg;

    void openImagePreview(const std::string& fullPath, const std::string& fileName);
    void closeImagePreview();

    // Audio Preview Overlay
    bool isAudioPreviewing = false;
    std::string audioPreviewFileName = "";
    std::string audioPreviewFullPath = "";
    float audioAnimTimer = 0.0f;

    void openAudioPreview(const std::string& fullPath, const std::string& fileName);
    void closeAudioPreview();

    // Quick Sample / Audio Preview
    void playAudioSample(const std::string& path, int hzMode);
    void stopAudioSample();

    // Conversion Queue
    bool isConverting = false;
    float conversionProgress = 0.0f;
    std::string conversionStatus = "";
    std::vector<std::string> convertQueue;
    int currentConvertIdx = 0;
    ConversionMode currentMode = ConversionMode::ALL;
    
    // Audio Conversion State
    FILE* convFIn = nullptr;
    FILE* convFOut = nullptr;
    void* convVfPtr = nullptr;
    uint32_t convSamplesProcessed = 0;
    uint32_t convTotalSamples = 0;
    int convChannels = 0;
    void* convStatePtr = nullptr;
    bool isAudioPhase = false;

    void startConversionMode(ConversionMode mode);
    void processConversion();
};
