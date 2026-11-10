#ifndef MEMORYDEBUGSTATE_HPP
#define MEMORYDEBUGSTATE_HPP

#include <vector>
#include <string>
#include <citro2d.h>
#include <citro3d.h>

class MemoryDebugState {
public:
    enum class SortMode {
        DEFAULT = 0,
        SIZE_DESC,
        NAME_ASC,
        POOL
    };

    MemoryDebugState();
    ~MemoryDebugState();

    void update(float dt);
    void draw();

    bool active = true;

private:
    struct MemoryItem {
        std::string name;
        u32 size = 0;
        bool isVram = false;
        std::string category = "Other";
    };

    struct FormattedItem {
        std::string displayName;
        std::string sizeStr;
        std::string poolStr;
    };

    std::vector<MemoryItem> rawItems;
    std::vector<MemoryItem> memoryItems;
    std::vector<FormattedItem> formattedItems;

    int memorySelection = 0;
    float memoryScrollLerp = 0.0f;
    SortMode sortMode = SortMode::DEFAULT;

    C2D_TextBuf dynamicTextBuf = nullptr;

    void gatherAllMemoryItems();
    void applySort();
    void rebuildFormattedCache();
};

#endif
