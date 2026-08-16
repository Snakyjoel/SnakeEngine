#pragma once
#include <string>
#include <vector>
#include <map>
#include <jansson.h>

enum class OptionType {
    BOOL,
    INT,
    FLOAT,
    STRING_LIST,
    ACTION,
    ACTION_CATEGORY
};

struct OptionItem {
    std::string id;
    std::string name;
    OptionType type = OptionType::BOOL;
    std::string desc;
    std::string action;
    std::string suffix;
    
    // Numeric bounds
    float minVal = 0.0f;
    float maxVal = 100.0f;
    float step = 1.0f;
    
    // String list options
    std::vector<std::string> stringOptions;
    
    // Current value
    bool boolVal = false;
    int intVal = 0;
    float floatVal = 0.0f;
    std::string stringVal = "";
    
    // Mod context
    std::string modFolder = ""; // Empty for base engine options
};

struct OptionCategory {
    std::string id;
    std::string name;
    std::string desc;
    OptionType type = OptionType::BOOL; // ACTION_CATEGORY if direct action
    std::string action;
    std::string modFolder = ""; // Non-empty if from a mod
    bool runsGlobally = true;
    std::vector<OptionItem> options;
};

class OptionManager {
public:
    static OptionManager& get() {
        static OptionManager instance;
        return instance;
    }

    void init();
    void loadBaseSchema();
    void refreshModSchemas();
    void loadValues();
    void saveValues();
    void resetToDefaults();

    // Engine base access
    bool getBool(const std::string& id, bool def = false);
    void setBool(const std::string& id, bool val);

    int getInt(const std::string& id, int def = 0);
    void setInt(const std::string& id, int val);

    float getFloat(const std::string& id, float def = 0.0f);
    void setFloat(const std::string& id, float val);

    std::string getString(const std::string& id, const std::string& def = "");
    void setString(const std::string& id, const std::string& val);

    // Mod-Specific access
    bool getModBool(const std::string& modFolder, const std::string& id, bool def = false);
    void setModBool(const std::string& modFolder, const std::string& id, bool val);

    int getModInt(const std::string& modFolder, const std::string& id, int def = 0);
    void setModInt(const std::string& modFolder, const std::string& id, int val);

    float getModFloat(const std::string& modFolder, const std::string& id, float def = 0.0f);
    void setModFloat(const std::string& modFolder, const std::string& id, float val);

    std::string getModString(const std::string& modFolder, const std::string& id, const std::string& def = "");
    void setModString(const std::string& modFolder, const std::string& id, const std::string& val);

    // Category list for UI
    std::vector<OptionCategory>& getCategories() { return categories; }
    OptionCategory* getCategory(int index);
    OptionCategory* getCategoryById(const std::string& id);

    // Synchronization with ClientPrefs
    void syncToClientPrefs();
    void syncFromClientPrefs();

private:
    bool initialized = false;
    std::vector<OptionCategory> baseCategories;
    std::vector<OptionCategory> modCategories;
    std::vector<OptionCategory> categories; // Active combined categories

    std::map<std::string, bool> boolMap;
    std::map<std::string, int> intMap;
    std::map<std::string, float> floatMap;
    std::map<std::string, std::string> stringMap;

    void parseOptionItem(json_t* optJson, OptionItem& item, const std::string& modFolder = "", const std::string& modName = "", bool runsGlobally = true);
    void rebuildCategories();
};
