#include "AsyncAssetManager.hpp"
#include "ModHandler.hpp"
#include <algorithm>
#include <stdio.h>
#include <unistd.h>
#include <tex3ds.h>
#include "stb_image.h"

// Helper to determine if an address is in VRAM
static inline bool addrIsVRAM(const void* addr) {
    u32 v = (u32)addr;
    return v >= 0x1F000000 && v < 0x1F600000;
}

void AsyncAssetManager::init() {
    if (running) return;
    LightLock_Init(&queueLock);
    running = true;
    // Create background thread (priority 30, stack size 32KB, default CPU)
    workerThread = threadCreate(threadMain, this, 32 * 1024, 30, -1, true);
}

void AsyncAssetManager::shutdown() {
    if (running) {
        running = false;
        suspended = false;
        if (workerThread) {
            threadJoin(workerThread, U64_MAX);
            threadFree(workerThread);
            workerThread = nullptr;
        }
    }

    LightLock_Lock(&queueLock);
    clearAllLocked();
    LightLock_Unlock(&queueLock);
}

void AsyncAssetManager::freeImageData(ImageData* data) {
    if (!data) return;
    if (data->sheet) {
        C2D_SpriteSheetFree(data->sheet);
        data->sheet = nullptr;
    }
    if (data->fileBuffer) {
        if (addrIsVRAM(data->fileBuffer)) {
            vramFree(data->fileBuffer);
        } else {
            linearFree(data->fileBuffer);
        }
        data->fileBuffer = nullptr;
    }
    if (data->tex) {
        C3D_TexDelete(data->tex);
        delete data->tex;
        data->tex = nullptr;
    }
    if (data->subtex) {
        delete data->subtex;
        data->subtex = nullptr;
    }
    delete data;
}

void AsyncAssetManager::clearAllLocked() {
    pendingCharacters.clear();
    pendingImages.clear();

    for (CharacterData* data : readyQueue) {
        delete data;
    }
    readyQueue.clear();

    for (ImageData* data : readyImageQueue) {
        freeImageData(data);
    }
    readyImageQueue.clear();

    for (ImageData* data : readyIconQueue) {
        freeImageData(data);
    }
    readyIconQueue.clear();

    for (Character* c : allCharacters) {
        delete c;
    }
    allCharacters.clear();
    cachedCharacters.clear();

    for (auto& pair : cachedImages) {
        freeImageData(pair.second);
    }
    cachedImages.clear();

    for (auto& pair : cachedIcons) {
        freeImageData(pair.second);
    }
    cachedIcons.clear();

    loadingStatus.clear();
    loadingImageStatus.clear();
    loadingIconStatus.clear();
    currentVramUsed = 0;
    currentLinearUsed = 0;
}

void AsyncAssetManager::clearAll() {
    LightLock_Lock(&queueLock);
    clearAllLocked();
    LightLock_Unlock(&queueLock);
}

bool AsyncAssetManager::requestCharacter(const std::string& charName) {
    bool newlyRequested = false;
    LightLock_Lock(&queueLock);
    if (loadingStatus.find(charName) == loadingStatus.end() && cachedCharacters.find(charName) == cachedCharacters.end()) {
        loadingStatus[charName] = true;
        pendingCharacters.push_back(charName);
        newlyRequested = true;
    }
    LightLock_Unlock(&queueLock);
    return newlyRequested;
}

void AsyncAssetManager::requestImageLoad(const std::string& imgName, bool priority) {
    LightLock_Lock(&queueLock);
    if (loadingImageStatus.find(imgName) == loadingImageStatus.end() && cachedImages.find(imgName) == cachedImages.end()) {
        loadingImageStatus[imgName] = true;
        if (priority) {
            pendingImages.insert(pendingImages.begin(), imgName); // Immediate priority
        } else {
            pendingImages.push_back(imgName); // FIFO queue
        }
    }
    LightLock_Unlock(&queueLock);
}

bool AsyncAssetManager::isImageReady(const std::string& imgName) {
    LightLock_Lock(&queueLock);
    bool ready = (cachedImages.find(imgName) != cachedImages.end());
    LightLock_Unlock(&queueLock);
    return ready;
}

ImageData* AsyncAssetManager::consumeImage(const std::string& imgName) {
    LightLock_Lock(&queueLock);
    ImageData* img = nullptr;
    if (cachedImages.find(imgName) != cachedImages.end()) {
        img = cachedImages[imgName];
        cachedImages.erase(imgName); // Consumed
    }
    LightLock_Unlock(&queueLock);
    return img;
}

void AsyncAssetManager::requestHealthIcon(const std::string& iconName) {
    LightLock_Lock(&queueLock);
    if (loadingIconStatus.find(iconName) == loadingIconStatus.end() && cachedIcons.find(iconName) == cachedIcons.end()) {
        loadingIconStatus[iconName] = true;
        pendingIcons.push_back(iconName);
    }
    LightLock_Unlock(&queueLock);
}

bool AsyncAssetManager::isHealthIconReady(const std::string& iconName) {
    LightLock_Lock(&queueLock);
    bool ready = (cachedIcons.find(iconName) != cachedIcons.end());
    LightLock_Unlock(&queueLock);
    return ready;
}

ImageData* AsyncAssetManager::consumeHealthIcon(const std::string& iconName) {
    LightLock_Lock(&queueLock);
    ImageData* img = nullptr;
    if (cachedIcons.find(iconName) != cachedIcons.end()) {
        img = cachedIcons[iconName];
        cachedIcons.erase(iconName); // Consumed
    }
    LightLock_Unlock(&queueLock);
    return img;
}

void AsyncAssetManager::unloadCharacter(const std::string& charName) {
    LightLock_Lock(&queueLock);
    if (cachedCharacters.find(charName) != cachedCharacters.end()) {
        Character* c = cachedCharacters[charName];
        delete c;
        cachedCharacters.erase(charName);
    }
    loadingStatus.erase(charName);
    LightLock_Unlock(&queueLock);
}

void AsyncAssetManager::clearUnused(const std::vector<std::string>& activeNames, const std::vector<Character*>& activePointers) {
    LightLock_Lock(&queueLock);
    std::vector<std::string> toRemove;
    for (const auto& pair : cachedCharacters) {
        Character* c = pair.second;
        bool needed = false;

        // Check if character pointer is in activePointers
        for (Character* activePtr : activePointers) {
            if (activePtr && activePtr == c) {
                needed = true;
                break;
            }
        }

        if (!needed) {
            for (const auto& name : activeNames) {
                if (!name.empty() && pair.first == name) {
                    needed = true;
                    break;
                }
            }
        }
        if (!needed) toRemove.push_back(pair.first);
    }
    
    for (const auto& name : toRemove) {
        Character* c = cachedCharacters[name];
        if (allCharacters.count(c)) {
            allCharacters.erase(c);
            delete c;
        }
        cachedCharacters.erase(name);
        loadingStatus.erase(name);
    }
    
    // Unload cached images that might be stale (not strictly necessary but good for memory)
    // We leave this to manual consumption for now unless we track active images.
    LightLock_Unlock(&queueLock);
}

void AsyncAssetManager::cacheCharacter(const std::string& charName, Character* c) {
    LightLock_Lock(&queueLock);
    cachedCharacters[charName] = c;
    allCharacters.insert(c);
    LightLock_Unlock(&queueLock);
}

static bool readChunked(FILE* f, void* buffer, size_t size) {
    uint8_t* ptr = (uint8_t*)buffer;
    size_t remaining = size;
    const size_t CHUNK_SIZE = 128 * 1024; // 128KB chunks
    while (remaining > 0) {
        size_t toRead = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        if (fread(ptr, 1, toRead, f) != toRead) {
            return false;
        }
        ptr += toRead;
        remaining -= toRead;
        if (remaining > 0) {
            svcSleepThread(1000000LL); // 1ms yield
        }
    }
    return true;
}

void AsyncAssetManager::threadMain(void* arg) {
    AsyncAssetManager* mgr = (AsyncAssetManager*)arg;
    while (mgr->running) {
        // Pause if main thread requested suspension (e.g. during PlayState init)
        if (mgr->suspended) {
            mgr->isSleeping = true;
            svcSleepThread(5000000LL); // 5ms
            continue;
        }
        mgr->isSleeping = false;

        std::string targetChar = "";
        std::string targetImage = "";
        std::string targetIcon = "";
        
        LightLock_Lock(&mgr->queueLock);
        if (!mgr->pendingIcons.empty()) {
            targetIcon = mgr->pendingIcons.front();
            mgr->pendingIcons.erase(mgr->pendingIcons.begin());
        } else if (!mgr->pendingCharacters.empty()) {
            targetChar = mgr->pendingCharacters.front();
            mgr->pendingCharacters.erase(mgr->pendingCharacters.begin());
        } else if (!mgr->pendingImages.empty()) {
            targetImage = mgr->pendingImages.front();
            mgr->pendingImages.erase(mgr->pendingImages.begin());
        }
        LightLock_Unlock(&mgr->queueLock);
        
        if (!targetIcon.empty()) {
            mgr->loadIconSync(targetIcon);
        } else if (!targetChar.empty()) {
            // Perform slow I/O and parsing on this thread!
            CharacterData* data = Character::parseDataAsync(targetChar);
            
            if (data) {
                mgr->loadIconSync(data->healthIcon);
                LightLock_Lock(&mgr->queueLock);
                if (mgr->running && !mgr->suspended) {
                    mgr->readyQueue.push_back(data);
                } else {
                    delete data;
                }
                LightLock_Unlock(&mgr->queueLock);
            } else {
                LightLock_Lock(&mgr->queueLock);
                mgr->loadingStatus.erase(targetChar); // Failed
                LightLock_Unlock(&mgr->queueLock);
            }
        } else if (!targetImage.empty()) {
            std::string path = Paths::image(targetImage);
            std::string rawPath = ModHandler::get().getModPath("images/" + targetImage + ".rawtex");
            if (rawPath.empty() && Paths::fileExists("romfs:/preload/images/" + targetImage + ".rawtex")) {
                rawPath = "romfs:/preload/images/" + targetImage + ".rawtex";
            }
            if (rawPath.empty() && Paths::fileExists("romfs:/shared/images/" + targetImage + ".rawtex")) {
                rawPath = "romfs:/shared/images/" + targetImage + ".rawtex";
            }

            ImageData* data = nullptr;
            FILE* f = nullptr;
            if (Paths::fileExists(rawPath)) {
                f = fopen(rawPath.c_str(), "rb");
                if (f) {
                    struct RawTexHeader { char magic[4]; uint16_t width; uint16_t height; uint16_t origW; uint16_t origH; } header;
                    if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                        size_t dataSize = (size_t)header.width * header.height * 4;
                        void* buffer = linearAlloc(dataSize);
                        if (buffer) {
                            readChunked(f, buffer, dataSize);
                            GSPGPU_FlushDataCache(buffer, dataSize);
                            data = new ImageData();
                            data->imgName = targetImage;
                            data->fileSize = dataSize;
                            data->isRawTex = true;
                            data->rawWidth = header.width;
                            data->rawHeight = header.height;
                            data->origWidth = header.origW;
                            data->origHeight = header.origH;

                            data->tex = new C3D_Tex();
                            memset(data->tex, 0, sizeof(C3D_Tex));
                            if (C3D_TexInit(data->tex, header.width, header.height, GPU_RGBA8)) {
                                if (data->tex->data) linearFree(data->tex->data);
                                data->tex->data = buffer;
                                C3D_TexSetFilter(data->tex, GPU_NEAREST, GPU_NEAREST);

                                data->subtex = new Tex3DS_SubTexture();
                                data->subtex->width = header.origW;
                                data->subtex->height = header.origH;
                                data->subtex->left = 0.0f;
                                data->subtex->top = 1.0f;
                                data->subtex->right = (float)header.origW / header.width;
                                data->subtex->bottom = 1.0f - ((float)header.origH / header.height);
                            } else {
                                delete data->tex;
                                data->tex = nullptr;
                                linearFree(buffer);
                            }
                        }
                    }
                    fclose(f);
                }
            // I wish someone would love me
            } else if (Paths::fileExists(path)) {
                f = fopen(path.c_str(), "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    size_t fileSize = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    
                    void* tempBuf = linearAlloc(fileSize);
                    if (tempBuf) {
                        readChunked(f, tempBuf, fileSize);
                        GSPGPU_FlushDataCache(tempBuf, fileSize);
                        
                        data = new ImageData();
                        data->imgName = targetImage;
                        data->fileSize = fileSize;
                        data->isRawTex = false;

                        // Decompress t3x on background thread (same as Character.cpp)
                        data->tex = new C3D_Tex();
                        memset(data->tex, 0, sizeof(C3D_Tex));
                        Tex3DS_Texture t3x = Tex3DS_TextureImport(tempBuf, fileSize, data->tex, nullptr, false);
                        if (t3x) {
                            const Tex3DS_SubTexture* sub = Tex3DS_GetSubTexture(t3x, 0);
                            data->subtex = new Tex3DS_SubTexture();
                            if (sub) {
                                *data->subtex = *sub;
                            } else {
                                data->subtex->width = data->tex->width;
                                data->subtex->height = data->tex->height;
                                data->subtex->left = 0.0f;
                                data->subtex->top = 1.0f;
                                data->subtex->right = 1.0f;
                                data->subtex->bottom = 0.0f;
                            }
                            Tex3DS_TextureFree(t3x);
                        } else {
                            delete data->tex;
                            data->tex = nullptr;
                        }
                        linearFree(tempBuf);
                    }
                    fclose(f);
                }
            } else {
                // Fallback: check png
                std::string pngPath = ModHandler::get().getModPath("images/" + targetImage + ".png");
                if (pngPath.empty() && Paths::fileExists("romfs:/preload/images/" + targetImage + ".png"))
                    pngPath = "romfs:/preload/images/" + targetImage + ".png";
                if (pngPath.empty() && Paths::fileExists("romfs:/shared/images/" + targetImage + ".png"))
                    pngPath = "romfs:/shared/images/" + targetImage + ".png";

                if (!pngPath.empty()) {
                    int w, h, c;
                    unsigned char* imgRaw = stbi_load(pngPath.c_str(), &w, &h, &c, 4);
                    if (imgRaw) {
                        int pw = 1, ph = 1;
                        while (pw < w) pw *= 2;
                        while (ph < h) ph *= 2;

                        data = new ImageData();
                        data->imgName = targetImage;
                        data->tex = new C3D_Tex();
                        memset(data->tex, 0, sizeof(C3D_Tex));
                        if (C3D_TexInit(data->tex, pw, ph, GPU_RGBA8)) {
                            uint32_t* swizzled = (uint32_t*)linearAlloc(pw * ph * 4);
                            if (swizzled) {
                                memset(swizzled, 0, pw * ph * 4);
                                for (int y = 0; y < h; y++) {
                                    for (int x = 0; x < w; x++) {
                                        int src = (y * w + x) * 4;
                                        uint32_t px = ((uint32_t)imgRaw[src]   << 24) |
                                                      ((uint32_t)imgRaw[src+1] << 16) |
                                                      ((uint32_t)imgRaw[src+2] <<  8) |
                                                       (uint32_t)imgRaw[src+3];
                                        uint32_t i  = (x & 7) | ((y & 7) << 8);
                                        i = (i ^ (i << 2)) & 0x1313;
                                        i = (i ^ (i << 1)) & 0x1515;
                                        uint32_t tx = x >> 3, ty = y >> 3;
                                        uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                                        uint32_t local_idx  = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                                        swizzled[tile_start + local_idx] = px;
                                    }
                                }
                                C3D_TexUpload(data->tex, swizzled);
                                C3D_TexFlush(data->tex);
                                linearFree(swizzled);

                                data->subtex = new Tex3DS_SubTexture();
                                data->subtex->width  = (u16)w;
                                data->subtex->height = (u16)h;
                                data->subtex->left   = 0.0f;
                                data->subtex->top    = 1.0f;
                                data->subtex->right  = (float)w / pw;
                                data->subtex->bottom = 1.0f - ((float)h / ph);
                            } else {
                                delete data->tex;
                                data->tex = nullptr;
                            }
                        } else {
                            delete data->tex;
                            data->tex = nullptr;
                        }
                        stbi_image_free(imgRaw);
                    }
                }
            }
            
            if (data && data->tex) {
                LightLock_Lock(&mgr->queueLock);
                if (mgr->running && !mgr->suspended) {
                    mgr->readyImageQueue.push_back(data);
                } else {
                    freeImageData(data);
                }
                LightLock_Unlock(&mgr->queueLock);
            } else {
                if (data) freeImageData(data);
                LightLock_Lock(&mgr->queueLock);
                mgr->loadingImageStatus.erase(targetImage); // Failed
                LightLock_Unlock(&mgr->queueLock);
            }
        } else {
            mgr->isSleeping = true;
            // Sleep slightly if queue is empty to avoid CPU spinning
            svcSleepThread(10000000LL); // 10ms
            mgr->isSleeping = false;
        }
    }
    mgr->isSleeping = true;
}

void AsyncAssetManager::loadIconSync(const std::string& targetIcon) {
    if (targetIcon.empty()) return;
    LightLock_Lock(&queueLock);
    if (cachedIcons.find(targetIcon) != cachedIcons.end()) {
        LightLock_Unlock(&queueLock);
        return;
    }
    auto it = std::find(pendingIcons.begin(), pendingIcons.end(), targetIcon);
    if (it != pendingIcons.end()) pendingIcons.erase(it);
    loadingIconStatus[targetIcon] = true;
    LightLock_Unlock(&queueLock);

    std::string path = Paths::healthIcon(targetIcon);
    FILE* f = fopen(path.c_str(), "rb");
    ImageData* data = nullptr;
    if (f) {
        struct RawTexHeader {
            char magic[4];
            uint16_t width;
            uint16_t height;
            uint16_t origW;
            uint16_t origH;
        } header;

        if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
            size_t dataSize = (size_t)header.width * header.height * 4;
            void* buffer = linearAlloc(dataSize);
            if (buffer) {
                readChunked(f, buffer, dataSize);
                GSPGPU_FlushDataCache(buffer, dataSize);
                data = new ImageData();
                data->imgName = targetIcon;
                data->fileBuffer = buffer;
                data->fileSize = dataSize;
                data->isRawTex = true;
                data->rawWidth = header.width;
                data->rawHeight = header.height;
                data->origWidth = header.origW;
                data->origHeight = header.origH;
            }
        } else {
            fseek(f, 0, SEEK_END);
            size_t fileSize = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            void* tempBuf = linearAlloc(fileSize);
            if (tempBuf) {
                readChunked(f, tempBuf, fileSize);
                GSPGPU_FlushDataCache(tempBuf, fileSize);
                data = new ImageData();
                data->imgName = targetIcon;
                data->fileSize = fileSize;
                data->isRawTex = false;
                data->fileBuffer = tempBuf;
            }
        }
        fclose(f);
    }
    if (data) {
        LightLock_Lock(&queueLock);
        if (running && !suspended) {
            readyIconQueue.push_back(data);
        } else {
            freeImageData(data);
        }
        LightLock_Unlock(&queueLock);
    } else {
        LightLock_Lock(&queueLock);
        loadingIconStatus.erase(targetIcon); // Failed
        LightLock_Unlock(&queueLock);
    }
}

void AsyncAssetManager::update() {
    // --- ICONS FIRST: pop all, process outside lock ---
    std::vector<ImageData*> iconsToProcess;
    {
        LightLock_Lock(&queueLock);
        iconsToProcess.swap(readyIconQueue);
        LightLock_Unlock(&queueLock);
    }
    for (ImageData* data : iconsToProcess) {
        if (!data->isRawTex && data->fileBuffer && data->fileSize > 0) {
            data->sheet = C2D_SpriteSheetLoadFromMem(data->fileBuffer, data->fileSize);
            linearFree(data->fileBuffer);
            data->fileBuffer = nullptr;
        }
        LightLock_Lock(&queueLock);
        cachedIcons[data->imgName] = data;
        LightLock_Unlock(&queueLock);
    }

    // --- CHARACTERS: pop 1 item from queue while holding lock, then process outside ---
    CharacterData* charData = nullptr;
    {
        LightLock_Lock(&queueLock);
        if (!readyQueue.empty()) {
            charData = readyQueue.back();
            readyQueue.pop_back();
            // If already cached synchronously, discard immediately
            if (cachedCharacters.find(charData->charName) != cachedCharacters.end()) {
                delete charData;
                charData = nullptr;
            }
        }
        LightLock_Unlock(&queueLock);
    }
    if (charData) {
        // Heavy GPU work done WITHOUT holding the lock
        Character* c = new Character();
        c->instantiateFromData(charData);
        if (!c->hasValidTexture()) {
            c->isPlaceholder = true;
        }
        delete charData;

        LightLock_Lock(&queueLock);
        cachedCharacters[c->curCharacterName] = c;
        allCharacters.insert(c);
        LightLock_Unlock(&queueLock);
    }

    // --- IMAGES: pop 1 item, process outside lock ---
    ImageData* imgData = nullptr;
    {
        LightLock_Lock(&queueLock);
        if (!readyImageQueue.empty()) {
            imgData = readyImageQueue.back();
            readyImageQueue.pop_back();
        }
        LightLock_Unlock(&queueLock);
    }
    if (imgData) {
        if (imgData->tex) {
            LightLock_Lock(&queueLock);
            cachedImages[imgData->imgName] = imgData;
            LightLock_Unlock(&queueLock);
        } else {
            freeImageData(imgData);
        }
    }
}

bool AsyncAssetManager::isCharacterReady(const std::string& charName) {
    LightLock_Lock(&queueLock);
    bool ready = (cachedCharacters.find(charName) != cachedCharacters.end());
    LightLock_Unlock(&queueLock);
    return ready;
}

Character* AsyncAssetManager::getCharacter(const std::string& charName) {
    LightLock_Lock(&queueLock);
    Character* c = nullptr;
    if (cachedCharacters.find(charName) != cachedCharacters.end()) {
        c = cachedCharacters[charName];
    }
    LightLock_Unlock(&queueLock);
    return c;
}
