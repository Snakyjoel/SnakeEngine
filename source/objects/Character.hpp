#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include <map>

#include "SparrowParser.hpp"


struct CharCacheHeader {
    char magic[4];
    uint32_t version;
    float charScale;
    bool flipX;
    bool noAntialiasing;
    char healthIcon[32];
    float healthbarR, healthbarG, healthbarB;
    float camOffsetX, camOffsetY;
    float baseX, baseY;
    float singDuration;
    int danceEveryNumBeats;
    char imagePath[128]; // Allow longer image paths
    uint32_t numFrames;
    uint32_t numAnimations;
    bool isRawTex;
    uint32_t fileSize;
    uint16_t rawWidth;
    uint16_t rawHeight;
    uint16_t rawOrigW;
    uint16_t rawOrigH;
};

struct FrameBin {
    char name[128]; // Some mods have long frame names
    int x, y, w, h;
    int frameX, frameY, frameW, frameH;
    bool rotated;
};

struct AnimBin {
    char name[64];
    char prefix[64];
    int fps;
    bool loop;
    float offsetX, offsetY;
    uint32_t numIndices;
};

class CharacterData {
public:
    std::string charName;
    bool isPlaceholder = false;
    
    // JSON properties
    float charScale = 6.0f;
    bool flipX = false;
    bool noAntialiasing = false;
    std::string healthIcon = "face";
    float healthbarR = 0.4f, healthbarG = 1.0f, healthbarB = 0.2f;
    float camOffsetX = 0.0f, camOffsetY = 0.0f;
    float baseX = 0.0f, baseY = 0.0f;
    float singDuration = 4.0f;
    int danceEveryNumBeats = 2;

    std::map<std::string, Animation> animations;
    std::vector<Frame> frames;

    // Texture buffer loaded in background
    void* fileBuffer = nullptr;
    size_t fileSize = 0;
    bool isRawTex = false;

    // For RawTex only
    uint16_t rawWidth = 0;
    uint16_t rawHeight = 0;
    uint16_t rawOrigW = 0;
    uint16_t rawOrigH = 0;

    C2D_SpriteSheet sheet = nullptr;
    
    // Background-decompressed texture and subtexture
    C3D_Tex* rawTex = nullptr;
    Tex3DS_SubTexture* rawSub = nullptr;

    ~CharacterData() {
        if (fileBuffer) linearFree(fileBuffer);
        if (sheet) C2D_SpriteSheetFree(sheet);
        if (rawTex) {
            C3D_TexDelete(rawTex);
            delete rawTex;
        }
        if (rawSub) delete rawSub;
    }
};

class Character {
public:
    Character();
    ~Character();

    void loadFromPsychJson(const std::string& jsonPath);
    void loadSparrowXml(const std::string& xmlPath);
    void addSpriteSheet(const std::string& t3xPath);
    
    bool hasValidTexture() const { return mainImage.tex != nullptr; }

    static CharacterData* parseDataAsync(const std::string& charName);
    static bool loadFromCache(const std::string& path, CharacterData* data);
    static void saveToCache(const std::string& path, CharacterData* data, const std::string& imagePath);
    void instantiateFromData(CharacterData* data);
    
    void playAnim(const std::string& animName, bool forced = false);
    void playAnimFES(const std::string& path, const std::string& animName, int fps, bool loop, float x, float y);
    void dance(bool forced = false);
    void update(float dt);
    void draw(float stageX, float stageY, float depth, float zoom, float camX, float camY, float shakeX = 0, float shakeY = 0);

    bool hasAnimation(const std::string& animName);
    void setAntialiasing(bool antialiased);

    float charScale = 1.0f;
    float charScaleX = 1.0f;
    float charScaleY = 1.0f;
    bool flipX = false;
    bool noAntialiasing = false;
    bool isPlayer = false;
    bool danced = false; // Toggle for danceLeft/danceRight
    
    // Config offsets
    float camOffsetX = 0.0f;
    float camOffsetY = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float baseX = 0.0f;
    float baseY = 0.0f;
    float alpha = 1.0f;
    bool visible = true;
    float angle = 0.0f;

    float holdTimer = 0.0f;
    float singDuration = 4.0f; // Steps en conductor
    int danceEveryNumBeats = 2;
    bool skipIdle = false;

    std::string curAnim = "";
    std::string healthIcon = "face";  // Icon name
    float healthbarR = 0.4f, healthbarG = 1.0f, healthbarB = 0.2f; // Bar color
    bool animFinished = false;
    bool isPlaceholder = false;
    bool specialAnim = false;
    std::string curCharacterName = "";
    std::string charTexturePath = "";

private:
    void* fileBuffer = nullptr;
    C2D_SpriteSheet sheet; 
    C2D_Image mainImage; // Base texture
    C3D_Tex* rawTex = nullptr;
    Tex3DS_SubTexture* rawSub = nullptr;
    std::vector<Frame> frames;
    std::map<std::string, Animation> animations;
    
    int curFrame = 0;
    float frameTimer = 0;
    Animation* currentAnimData = nullptr;

    // External Anim Support
    bool isExternalAnim = false;
    std::vector<Frame> externalFrames;
    Animation externalAnimData;
};
