#include "OptionManager.hpp"
#include "ClientPrefs.hpp"
#include "ModHandler.hpp"
#include "Paths.hpp"
#include <sys/stat.h>
#include <fstream>
#include <iostream>

void OptionManager::init() {
    if (initialized) return;
    initialized = true;
    loadBaseSchema();
    loadValues();
    refreshModSchemas();
    syncToClientPrefs();
}

void OptionManager::loadBaseSchema() {
    baseCategories.clear();

    std::string path = "romfs:/preload/data/options.json";
    if (!Paths::fileExists(path)) path = "romfs:/data/options.json";
    if (!Paths::fileExists(path)) return;

    json_t* root;
    json_error_t error;
    root = json_load_file(path.c_str(), 0, &error);
    if (!root) return;

    json_t* catsJson = json_object_get(root, "categories");
    if (catsJson && json_is_array(catsJson)) {
        size_t numCats = json_array_size(catsJson);
        for (size_t i = 0; i < numCats; i++) {
            json_t* catObj = json_array_get(catsJson, i);
            if (!catObj || !json_is_object(catObj)) continue;

            OptionCategory cat;
            json_t* idJ = json_object_get(catObj, "id");
            if (idJ && json_is_string(idJ)) cat.id = json_string_value(idJ);

            json_t* nameJ = json_object_get(catObj, "name");
            if (nameJ && json_is_string(nameJ)) cat.name = json_string_value(nameJ);

            json_t* descJ = json_object_get(catObj, "description");
            if (descJ && json_is_string(descJ)) cat.desc = json_string_value(descJ);

            json_t* typeJ = json_object_get(catObj, "type");
            if (typeJ && json_is_string(typeJ)) {
                std::string typeStr = json_string_value(typeJ);
                if (typeStr == "action_category") cat.type = OptionType::ACTION_CATEGORY;
            }

            json_t* actJ = json_object_get(catObj, "action");
            if (actJ && json_is_string(actJ)) cat.action = json_string_value(actJ);

            json_t* optsJ = json_object_get(catObj, "options");
            if (optsJ && json_is_array(optsJ)) {
                size_t numOpts = json_array_size(optsJ);
                for (size_t j = 0; j < numOpts; j++) {
                    json_t* optObj = json_array_get(optsJ, j);
                    if (!optObj || !json_is_object(optObj)) continue;
                    OptionItem item;
                    parseOptionItem(optObj, item, "", "", true);
                    cat.options.push_back(item);
                }
            }

            baseCategories.push_back(cat);
        }
    }

    json_decref(root);
    rebuildCategories();
}

void OptionManager::parseOptionItem(json_t* optJson, OptionItem& item, const std::string& modFolder, const std::string& modName, bool runsGlobally) {
    item.modFolder = modFolder;

    json_t* idJ = json_object_get(optJson, "id");
    if (idJ && json_is_string(idJ)) item.id = json_string_value(idJ);

    json_t* nameJ = json_object_get(optJson, "name");
    if (nameJ && json_is_string(nameJ)) item.name = json_string_value(nameJ);

    json_t* descJ = json_object_get(optJson, "description");
    if (descJ && json_is_string(descJ)) item.desc = json_string_value(descJ);

    // If mod option is local only (runsGlobally == false), append note to description
    if (!modFolder.empty() && !runsGlobally) {
        std::string note = "\n(Note: This option only works with " + (modName.empty() ? modFolder : modName) + ")";
        if (item.desc.find("Note: This option only works with") == std::string::npos) {
            item.desc += note;
        }
    }

    json_t* sufJ = json_object_get(optJson, "suffix");
    if (sufJ && json_is_string(sufJ)) item.suffix = json_string_value(sufJ);

    json_t* actJ = json_object_get(optJson, "action");
    if (actJ && json_is_string(actJ)) item.action = json_string_value(actJ);

    json_t* minJ = json_object_get(optJson, "min");
    if (minJ && (json_is_number(minJ))) item.minVal = (float)json_number_value(minJ);

    json_t* maxJ = json_object_get(optJson, "max");
    if (maxJ && (json_is_number(maxJ))) item.maxVal = (float)json_number_value(maxJ);

    json_t* stepJ = json_object_get(optJson, "step");
    if (stepJ && (json_is_number(stepJ))) item.step = (float)json_number_value(stepJ);

    std::string typeStr = "bool";
    json_t* typeJ = json_object_get(optJson, "type");
    if (typeJ && json_is_string(typeJ)) typeStr = json_string_value(typeJ);

    json_t* defJ = json_object_get(optJson, "default");

    if (typeStr == "bool") {
        item.type = OptionType::BOOL;
        item.boolVal = (defJ && json_is_boolean(defJ)) ? json_is_true(defJ) : false;
        std::string key = modFolder.empty() ? item.id : (modFolder + ":" + item.id);
        if (boolMap.find(key) == boolMap.end()) boolMap[key] = item.boolVal;
        else item.boolVal = boolMap[key];
    } else if (typeStr == "int") {
        item.type = OptionType::INT;
        item.intVal = (defJ && json_is_integer(defJ)) ? (int)json_integer_value(defJ) : 0;
        std::string key = modFolder.empty() ? item.id : (modFolder + ":" + item.id);
        if (intMap.find(key) == intMap.end()) intMap[key] = item.intVal;
        else item.intVal = intMap[key];
    } else if (typeStr == "float") {
        item.type = OptionType::FLOAT;
        item.floatVal = (defJ && json_is_number(defJ)) ? (float)json_number_value(defJ) : 0.0f;
        std::string key = modFolder.empty() ? item.id : (modFolder + ":" + item.id);
        if (floatMap.find(key) == floatMap.end()) floatMap[key] = item.floatVal;
        else item.floatVal = floatMap[key];
    } else if (typeStr == "string_list") {
        item.type = OptionType::STRING_LIST;
        json_t* optArr = json_object_get(optJson, "options");
        if (optArr && json_is_array(optArr)) {
            size_t n = json_array_size(optArr);
            for (size_t k = 0; k < n; k++) {
                json_t* sJ = json_array_get(optArr, k);
                if (sJ && json_is_string(sJ)) item.stringOptions.push_back(json_string_value(sJ));
            }
        }
        item.stringVal = (defJ && json_is_string(defJ)) ? json_string_value(defJ) : (item.stringOptions.empty() ? "" : item.stringOptions[0]);
        std::string key = modFolder.empty() ? item.id : (modFolder + ":" + item.id);
        if (stringMap.find(key) == stringMap.end()) stringMap[key] = item.stringVal;
        else item.stringVal = stringMap[key];
    } else if (typeStr == "action") {
        item.type = OptionType::ACTION;
    }
}

void OptionManager::refreshModSchemas() {
    modCategories.clear();
    std::string base = ModHandler::getWorkingBase();

    for (const auto& mod : ModHandler::get().getMods()) {
        if (mod.folder.empty() || !mod.active) continue;

        std::string optPath = base + mod.folder + "/data/options.json";
        if (!Paths::fileExists(optPath)) {
            optPath = base + mod.folder + "/options.json";
        }
        if (!Paths::fileExists(optPath)) continue;

        json_t* root;
        json_error_t error;
        root = json_load_file(optPath.c_str(), 0, &error);
        if (!root) continue;

        OptionCategory cat;
        cat.id = mod.folder + "_options";
        cat.name = mod.name + " Options";
        cat.desc = "Ajustes personalizados de " + mod.name;
        cat.modFolder = mod.folder;
        cat.runsGlobally = mod.runsGlobally;

        json_t* catNameJ = json_object_get(root, "categoryName");
        if (catNameJ && json_is_string(catNameJ)) cat.name = json_string_value(catNameJ);

        json_t* catDescJ = json_object_get(root, "categoryDescription");
        if (catDescJ && json_is_string(catDescJ)) cat.desc = json_string_value(catDescJ);

        json_t* optsJ = json_object_get(root, "options");
        if (optsJ && json_is_array(optsJ)) {
            size_t numOpts = json_array_size(optsJ);
            for (size_t j = 0; j < numOpts; j++) {
                json_t* optObj = json_array_get(optsJ, j);
                if (!optObj || !json_is_object(optObj)) continue;
                OptionItem item;
                parseOptionItem(optObj, item, mod.folder, mod.name, mod.runsGlobally);
                cat.options.push_back(item);
            }
        }

        if (!cat.options.empty()) {
            modCategories.push_back(cat);
        }

        json_decref(root);
    }

    rebuildCategories();
}

void OptionManager::rebuildCategories() {
    categories.clear();

    // Insert standard categories
    for (const auto& cat : baseCategories) {
        // Insert mod categories right before the final action categories (Controls, Colors, Reset, Erase)
        if (cat.type == OptionType::ACTION_CATEGORY && !modCategories.empty()) {
            for (const auto& mCat : modCategories) {
                categories.push_back(mCat);
            }
            modCategories.clear(); // Flushed into categories
        }
        categories.push_back(cat);
    }
    // If any mod categories remain
    for (const auto& mCat : modCategories) {
        categories.push_back(mCat);
    }

    // Refresh option values in items from memory maps
    for (auto& cat : categories) {
        for (auto& opt : cat.options) {
            std::string key = opt.modFolder.empty() ? opt.id : (opt.modFolder + ":" + opt.id);
            if (opt.type == OptionType::BOOL && boolMap.find(key) != boolMap.end()) opt.boolVal = boolMap[key];
            if (opt.type == OptionType::INT && intMap.find(key) != intMap.end()) opt.intVal = intMap[key];
            if (opt.type == OptionType::FLOAT && floatMap.find(key) != floatMap.end()) opt.floatVal = floatMap[key];
            if (opt.type == OptionType::STRING_LIST && stringMap.find(key) != stringMap.end()) opt.stringVal = stringMap[key];
        }
    }
}

OptionCategory* OptionManager::getCategory(int index) {
    if (index >= 0 && index < (int)categories.size()) return &categories[index];
    return nullptr;
}

OptionCategory* OptionManager::getCategoryById(const std::string& id) {
    for (auto& cat : categories) {
        if (cat.id == id) return &cat;
    }
    return nullptr;
}

void OptionManager::loadValues() {
    std::string path = ModHandler::getWorkingBase() + "options.json";
    if (!Paths::fileExists(path)) return;

    json_t* root;
    json_error_t error;
    root = json_load_file(path.c_str(), 0, &error);
    if (!root) return;

    // 1. Check if structured with "engine" and "modOptions"
    json_t* engineJ = json_object_get(root, "engine");
    if (engineJ && json_is_object(engineJ)) {
        const char* key;
        json_t* val;
        json_object_foreach(engineJ, key, val) {
            if (json_is_boolean(val)) boolMap[key] = json_is_true(val);
            else if (json_is_integer(val)) intMap[key] = (int)json_integer_value(val);
            else if (json_is_number(val)) floatMap[key] = (float)json_number_value(val);
            else if (json_is_string(val)) stringMap[key] = json_string_value(val);
        }
    }

    json_t* modOptsJ = json_object_get(root, "modOptions");
    if (modOptsJ && json_is_object(modOptsJ)) {
        const char* modKey;
        json_t* modValObj;
        json_object_foreach(modOptsJ, modKey, modValObj) {
            if (!json_is_object(modValObj)) continue;
            const char* optKey;
            json_t* optVal;
            json_object_foreach(modValObj, optKey, optVal) {
                std::string compositeKey = std::string(modKey) + ":" + optKey;
                if (json_is_boolean(optVal)) boolMap[compositeKey] = json_is_true(optVal);
                else if (json_is_integer(optVal)) intMap[compositeKey] = (int)json_integer_value(optVal);
                else if (json_is_number(optVal)) floatMap[compositeKey] = (float)json_number_value(optVal);
                else if (json_is_string(optVal)) stringMap[compositeKey] = json_string_value(optVal);
            }
        }
    }

    // 2. Backward compatibility for flat old options.json
    const char* rootKey;
    json_t* rootVal;
    json_object_foreach(root, rootKey, rootVal) {
        if (strcmp(rootKey, "engine") == 0 || strcmp(rootKey, "modOptions") == 0) continue;
        if (json_is_boolean(rootVal)) boolMap[rootKey] = json_is_true(rootVal);
        else if (json_is_integer(rootVal)) intMap[rootKey] = (int)json_integer_value(rootVal);
        else if (json_is_number(rootVal)) floatMap[rootKey] = (float)json_number_value(rootVal);
        else if (json_is_string(rootVal)) stringMap[rootKey] = json_string_value(rootVal);
    }

    json_decref(root);
}

void OptionManager::saveValues() {
    syncFromClientPrefs();

    json_t* root = json_object();
    json_t* engineJ = json_object();
    json_t* modOptsJ = json_object();

    for (const auto& kv : boolMap) {
        size_t col = kv.first.find(':');
        if (col == std::string::npos) {
            json_object_set_new(engineJ, kv.first.c_str(), json_boolean(kv.second));
            // Also store flat in root for legacy compatibility
            json_object_set_new(root, kv.first.c_str(), json_boolean(kv.second));
        } else {
            std::string mod = kv.first.substr(0, col);
            std::string opt = kv.first.substr(col + 1);
            json_t* mObj = json_object_get(modOptsJ, mod.c_str());
            if (!mObj) {
                mObj = json_object();
                json_object_set_new(modOptsJ, mod.c_str(), mObj);
            }
            json_object_set_new(mObj, opt.c_str(), json_boolean(kv.second));
        }
    }

    for (const auto& kv : intMap) {
        size_t col = kv.first.find(':');
        if (col == std::string::npos) {
            json_object_set_new(engineJ, kv.first.c_str(), json_integer(kv.second));
            json_object_set_new(root, kv.first.c_str(), json_integer(kv.second));
        } else {
            std::string mod = kv.first.substr(0, col);
            std::string opt = kv.first.substr(col + 1);
            json_t* mObj = json_object_get(modOptsJ, mod.c_str());
            if (!mObj) {
                mObj = json_object();
                json_object_set_new(modOptsJ, mod.c_str(), mObj);
            }
            json_object_set_new(mObj, opt.c_str(), json_integer(kv.second));
        }
    }

    for (const auto& kv : floatMap) {
        size_t col = kv.first.find(':');
        if (col == std::string::npos) {
            json_object_set_new(engineJ, kv.first.c_str(), json_real(kv.second));
            json_object_set_new(root, kv.first.c_str(), json_real(kv.second));
        } else {
            std::string mod = kv.first.substr(0, col);
            std::string opt = kv.first.substr(col + 1);
            json_t* mObj = json_object_get(modOptsJ, mod.c_str());
            if (!mObj) {
                mObj = json_object();
                json_object_set_new(modOptsJ, mod.c_str(), mObj);
            }
            json_object_set_new(mObj, opt.c_str(), json_real(kv.second));
        }
    }

    for (const auto& kv : stringMap) {
        size_t col = kv.first.find(':');
        if (col == std::string::npos) {
            json_object_set_new(engineJ, kv.first.c_str(), json_string(kv.second.c_str()));
            json_object_set_new(root, kv.first.c_str(), json_string(kv.second.c_str()));
        } else {
            std::string mod = kv.first.substr(0, col);
            std::string opt = kv.first.substr(col + 1);
            json_t* mObj = json_object_get(modOptsJ, mod.c_str());
            if (!mObj) {
                mObj = json_object();
                json_object_set_new(modOptsJ, mod.c_str(), mObj);
            }
            json_object_set_new(mObj, opt.c_str(), json_string(kv.second.c_str()));
        }
    }

    // Save note keys and note colors for ClientPrefs compatibility
    json_t* keyArr = json_array();
    for (int i = 0; i < 4; i++) {
        json_t* subArr = json_array();
        json_array_append_new(subArr, json_integer(ClientPrefs::noteKeys[i][0]));
        json_array_append_new(subArr, json_integer(ClientPrefs::noteKeys[i][1]));
        json_array_append_new(keyArr, subArr);
    }
    json_object_set_new(root, "noteKeys", keyArr);

    json_t* colArr = json_array();
    for (int i = 0; i < 4; i++) {
        json_t* subArr = json_array();
        json_array_append_new(subArr, json_integer(ClientPrefs::noteColors[i][0]));
        json_array_append_new(subArr, json_integer(ClientPrefs::noteColors[i][1]));
        json_array_append_new(subArr, json_integer(ClientPrefs::noteColors[i][2]));
        json_array_append_new(colArr, subArr);
    }
    json_object_set_new(root, "noteColors", colArr);

    json_object_set_new(root, "engine", engineJ);
    json_object_set_new(root, "modOptions", modOptsJ);

    std::string path = ModHandler::getWorkingBase() + "options.json";
    json_dump_file(root, path.c_str(), JSON_INDENT(2));
    json_decref(root);
}

void OptionManager::resetToDefaults() {
    boolMap.clear();
    intMap.clear();
    floatMap.clear();
    stringMap.clear();
    loadBaseSchema();
    refreshModSchemas();
    syncToClientPrefs();
    saveValues();
}

bool OptionManager::getBool(const std::string& id, bool def) {
    if (boolMap.find(id) != boolMap.end()) return boolMap[id];
    return def;
}

void OptionManager::setBool(const std::string& id, bool val) {
    boolMap[id] = val;
}

int OptionManager::getInt(const std::string& id, int def) {
    if (intMap.find(id) != intMap.end()) return intMap[id];
    return def;
}

void OptionManager::setInt(const std::string& id, int val) {
    intMap[id] = val;
}

float OptionManager::getFloat(const std::string& id, float def) {
    if (floatMap.find(id) != floatMap.end()) return floatMap[id];
    return def;
}

void OptionManager::setFloat(const std::string& id, float val) {
    floatMap[id] = val;
}

std::string OptionManager::getString(const std::string& id, const std::string& def) {
    if (stringMap.find(id) != stringMap.end()) return stringMap[id];
    return def;
}

void OptionManager::setString(const std::string& id, const std::string& val) {
    stringMap[id] = val;
}

bool OptionManager::getModBool(const std::string& modFolder, const std::string& id, bool def) {
    std::string key = modFolder + ":" + id;
    if (boolMap.find(key) != boolMap.end()) return boolMap[key];
    return def;
}

void OptionManager::setModBool(const std::string& modFolder, const std::string& id, bool val) {
    boolMap[modFolder + ":" + id] = val;
}

int OptionManager::getModInt(const std::string& modFolder, const std::string& id, int def) {
    std::string key = modFolder + ":" + id;
    if (intMap.find(key) != intMap.end()) return intMap[key];
    return def;
}

void OptionManager::setModInt(const std::string& modFolder, const std::string& id, int val) {
    intMap[modFolder + ":" + id] = val;
}

float OptionManager::getModFloat(const std::string& modFolder, const std::string& id, float def) {
    std::string key = modFolder + ":" + id;
    if (floatMap.find(key) != floatMap.end()) return floatMap[key];
    return def;
}

void OptionManager::setModFloat(const std::string& modFolder, const std::string& id, float val) {
    floatMap[modFolder + ":" + id] = val;
}

std::string OptionManager::getModString(const std::string& modFolder, const std::string& id, const std::string& def) {
    std::string key = modFolder + ":" + id;
    if (stringMap.find(key) != stringMap.end()) return stringMap[key];
    return def;
}

void OptionManager::setModString(const std::string& modFolder, const std::string& id, const std::string& val) {
    stringMap[modFolder + ":" + id] = val;
}

void OptionManager::syncToClientPrefs() {
    ClientPrefs::downscroll        = getBool("downscroll", false);
    ClientPrefs::middleScroll      = getBool("middleScroll", false);
    ClientPrefs::opponentStrums    = getBool("opponentStrums", true);
    ClientPrefs::opponentNotes     = getBool("opponentNotes", true);
    ClientPrefs::ghostTapping      = getBool("ghostTapping", true);
    ClientPrefs::disableReset      = getBool("disableReset", false);
    ClientPrefs::hitboxEnabled     = getBool("hitboxEnabled", true);
    ClientPrefs::botPlay           = getBool("botPlay", false);
    ClientPrefs::noteUnderlayAlpha = getFloat("noteUnderlayAlpha", 0.0f);
    ClientPrefs::opponentUnderlay  = getBool("opponentUnderlay", true);

    ClientPrefs::noteColorsEnabled = getBool("noteColorsEnabled", false);
    ClientPrefs::healthBar         = getBool("healthBar", true);
    ClientPrefs::showRatings       = getBool("showRatings", true);
    ClientPrefs::camZooms          = getBool("camZooms", true);
    ClientPrefs::scoreZoom         = getBool("scoreZoom", true);
    ClientPrefs::flashing          = getBool("flashing", true);
    ClientPrefs::alphabetPause     = getBool("alphabetPause", true);
    ClientPrefs::checkForUpdates   = getBool("checkForUpdates", true);
    ClientPrefs::buttonPrompts     = getBool("buttonPrompts", true);

    ClientPrefs::lowQuality        = getBool("lowQuality", false);
    ClientPrefs::globalAntialiasing= getBool("globalAntialiasing", true);
    ClientPrefs::fastNotes         = getBool("fastNotes", false);
    ClientPrefs::drawGrid          = getBool("drawGrid", true);
    ClientPrefs::debugInfo         = getBool("debugInfo", false);
    ClientPrefs::extendedDebug     = getBool("extendedDebug", false);

    std::string tb = getString("timeBarType", "Time Left");
    if (tb == "Time Left") ClientPrefs::timeBarType = 0;
    else if (tb == "Time Elapsed") ClientPrefs::timeBarType = 1;
    else if (tb == "Song Name") ClientPrefs::timeBarType = 2;
    else if (tb == "Disabled") ClientPrefs::timeBarType = 3;

    std::string hbVis = getString("hitboxMode", "Always");
    if (hbVis == "Always") ClientPrefs::hitboxMode = 0;
    else if (hbVis == "On Touch") ClientPrefs::hitboxMode = 1;
    else if (hbVis == "Hidden") ClientPrefs::hitboxMode = 2;

    std::string hbSty = getString("hitboxStyle", "Full Bars");
    if (hbSty == "Full Bars") ClientPrefs::hitboxStyle = 0;
    else if (hbSty == "Borders Only") ClientPrefs::hitboxStyle = 1;

    std::string fps = getString("fpsLimit", "60 FPS");
    ClientPrefs::fpsLimit = (fps == "30 FPS") ? 30 : 60;
}

void OptionManager::syncFromClientPrefs() {
    setBool("downscroll", ClientPrefs::downscroll);
    setBool("middleScroll", ClientPrefs::middleScroll);
    setBool("opponentStrums", ClientPrefs::opponentStrums);
    setBool("opponentNotes", ClientPrefs::opponentNotes);
    setBool("ghostTapping", ClientPrefs::ghostTapping);
    setBool("disableReset", ClientPrefs::disableReset);
    setBool("hitboxEnabled", ClientPrefs::hitboxEnabled);
    setBool("botPlay", ClientPrefs::botPlay);
    setFloat("noteUnderlayAlpha", ClientPrefs::noteUnderlayAlpha);
    setBool("opponentUnderlay", ClientPrefs::opponentUnderlay);

    setBool("noteColorsEnabled", ClientPrefs::noteColorsEnabled);
    setBool("healthBar", ClientPrefs::healthBar);
    setBool("showRatings", ClientPrefs::showRatings);
    setBool("camZooms", ClientPrefs::camZooms);
    setBool("scoreZoom", ClientPrefs::scoreZoom);
    setBool("flashing", ClientPrefs::flashing);
    setBool("alphabetPause", ClientPrefs::alphabetPause);
    setBool("checkForUpdates", ClientPrefs::checkForUpdates);
    setBool("buttonPrompts", ClientPrefs::buttonPrompts);

    setBool("lowQuality", ClientPrefs::lowQuality);
    setBool("globalAntialiasing", ClientPrefs::globalAntialiasing);
    setBool("fastNotes", ClientPrefs::fastNotes);
    setBool("drawGrid", ClientPrefs::drawGrid);
    setBool("debugInfo", ClientPrefs::debugInfo);
    setBool("extendedDebug", ClientPrefs::extendedDebug);
}
