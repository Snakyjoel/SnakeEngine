#pragma once
#include <3ds.h>
#include <string>
#include <queue>
#include <map>
#include <set>
#include <vector>
#include "../objects/Character.hpp"

enum class TexTier { VRAM, LINEAR };

struct ImageData {
    std::string imgName;
    void* fileBuffer = nullptr;
    size_t fileSize = 0;
    
    bool isRawTex = false;
    uint16_t rawWidth = 0;
    uint16_t rawHeight = 0;
    uint16_t origWidth = 0;
    uint16_t origHeight = 0;
    
    // Tier tracking
    TexTier tier = TexTier::LINEAR;
    C3D_Tex* tex = nullptr;
    Tex3DS_SubTexture* subtex = nullptr;
    C2D_SpriteSheet sheet = nullptr;
};

class AsyncAssetManager {
public:
    static AsyncAssetManager& get() {
        static AsyncAssetManager instance;
        return instance;
    }

    void init();
    void shutdown();
    
    // Request a character to be loaded in the background
    bool requestCharacter(const std::string& charName);
    
    // Request to unload a character from memory
    void unloadCharacter(const std::string& charName);
    
    // Thread-safe method to request an image
    void requestImageLoad(const std::string& imgName, bool priority = false);
    
    // Check if image is ready
    bool isImageReady(const std::string& imgName);
    
    // Get preloaded image data (returns nullptr if not ready)
    ImageData* consumeImage(const std::string& imgName);

    // Request health icon load
    void requestHealthIcon(const std::string& iconName);
    bool isHealthIconReady(const std::string& iconName);
    ImageData* consumeHealthIcon(const std::string& iconName);

    // Unloads all characters that are not in the activeNames list or activePointers
    void clearUnused(const std::vector<std::string>& activeNames, const std::vector<Character*>& activePointers = {});
    
    // Manually add a pre-loaded character to the cache
    void cacheCharacter(const std::string& charName, Character* c);

    // Clear all cached assets safely
    void clearAll();

    // Call on main thread to check ready queue and instantiate
    void update();
    
    bool isCharacterReady(const std::string& charName);
    Character* getCharacter(const std::string& charName);

    void suspend() {
        suspended = true;
        while (!isSleeping && running) {
            svcSleepThread(1000000LL); // Yield 1ms
        }
    }
    void resume()  { suspended = false; }
    bool isSuspended() const { return suspended; }

private:
    AsyncAssetManager() = default;
    ~AsyncAssetManager() = default;

    static void threadMain(void* arg);
    void clearAllLocked();
    static void freeImageData(ImageData* data);
    void loadIconSync(const std::string& iconName);

    Thread workerThread = nullptr;
    LightLock queueLock;
    volatile bool running = false;
    volatile bool suspended = false;
    volatile bool isSleeping = false;

    std::vector<std::string> pendingCharacters;
    std::vector<std::string> pendingImages;
    std::vector<std::string> pendingIcons;
    std::vector<CharacterData*> readyQueue;
    std::vector<ImageData*> readyImageQueue;
    std::vector<ImageData*> readyIconQueue;
    std::map<std::string, bool> loadingStatus;
    std::map<std::string, bool> loadingImageStatus;
    std::map<std::string, bool> loadingIconStatus;
    std::map<std::string, Character*> cachedCharacters;
    std::set<Character*> allCharacters;
    std::map<std::string, ImageData*> cachedImages;
    std::map<std::string, ImageData*> cachedIcons;
    
    // Memory budgets
    static constexpr size_t VRAM_BUDGET = 5 * 1024 * 1024;
    static constexpr size_t LINEAR_BUDGET = 24 * 1024 * 1024;
    size_t currentVramUsed = 0;
    size_t currentLinearUsed = 0;
};
