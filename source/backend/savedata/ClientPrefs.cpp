#include "ModHandler.hpp"
#include "OptionManager.hpp"
#include <jansson.h>
#include <3ds.h>
#include <sys/stat.h>

bool ClientPrefs::downscroll = false;
bool ClientPrefs::ghostTapping = true;
bool ClientPrefs::debugInfo = false;
bool ClientPrefs::extendedDebug = false;
bool ClientPrefs::middleScroll = false;
bool ClientPrefs::opponentStrums = true;
bool ClientPrefs::opponentNotes = true;
bool ClientPrefs::camZooms = true;
int ClientPrefs::timeBarType = 0;
bool ClientPrefs::healthBar = true;
bool ClientPrefs::scoreZoom = true;
bool ClientPrefs::lowQuality = false;
bool ClientPrefs::showRatings = true;
bool ClientPrefs::fastNotes = false;
bool ClientPrefs::globalAntialiasing = true;

bool ClientPrefs::flashing = true;
bool ClientPrefs::disableReset = false;
bool ClientPrefs::botPlay = false;
int ClientPrefs::noteOffset = 0;
bool ClientPrefs::noteColorsEnabled = false;

bool ClientPrefs::drawGrid = true;
float ClientPrefs::comboOffsetX = 170.0f;
float ClientPrefs::comboOffsetY = 120.0f;
float ClientPrefs::comboScale = 1.0f;
float ClientPrefs::comboAlpha = 1.0f;
bool ClientPrefs::alphabetPause = true;
bool ClientPrefs::checkForUpdates = true;

bool ClientPrefs::hitboxEnabled = false;
int ClientPrefs::hitboxMode = 0;
int ClientPrefs::hitboxStyle = 0;
int ClientPrefs::hitboxAlphaNormal = 10;
int ClientPrefs::hitboxAlphaTouch = 25;
float ClientPrefs::noteUnderlayAlpha = 0.0f;
bool ClientPrefs::opponentUnderlay = true;
bool ClientPrefs::acceptedEgg = false;
bool ClientPrefs::eggInteractionOccurred = false;
int ClientPrefs::fpsLimit = 60;

unsigned int ClientPrefs::noteKeys[4][2] = {
    {KEY_DLEFT, KEY_Y},
    {KEY_DDOWN, KEY_B},
    {KEY_DUP,   KEY_X},
    {KEY_DRIGHT, KEY_A}
};

unsigned char ClientPrefs::noteColors[4][3] = {
    {255, 100, 255}, // Left
    {100, 255, 255}, // Down
    {100, 255, 100}, // Up
    {255, 100, 100}  // Right
};

void ClientPrefs::loadSettings() {
    json_t *root;
    json_error_t error;
    
    std::string path = ModHandler::getWorkingBase() + "options.json";
    root = json_load_file(path.c_str(), 0, &error);
    if (!root) return;
    
    json_t *val;
    
    val = json_object_get(root, "downscroll");
    if (val && json_is_boolean(val)) downscroll = json_is_true(val);

    val = json_object_get(root, "middleScroll");
    if (val && json_is_boolean(val)) middleScroll = json_is_true(val);

    val = json_object_get(root, "opponentStrums");
    if (val && json_is_boolean(val)) opponentStrums = json_is_true(val);

    val = json_object_get(root, "opponentNotes");
    if (val && json_is_boolean(val)) opponentNotes = json_is_true(val);

    val = json_object_get(root, "ghostTapping");
    if (val && json_is_boolean(val)) ghostTapping = json_is_true(val);

    val = json_object_get(root, "debugInfo");
    if (val && json_is_boolean(val)) debugInfo = json_is_true(val);

    val = json_object_get(root, "extendedDebug");
    if (val && json_is_boolean(val)) extendedDebug = json_is_true(val);

    val = json_object_get(root, "camZooms");
    if (val && json_is_boolean(val)) camZooms = json_is_true(val);

    val = json_object_get(root, "timeBarType");
    if (val && json_is_integer(val)) timeBarType = (int)json_integer_value(val);

    val = json_object_get(root, "healthBar");
    if (val && json_is_boolean(val)) healthBar = json_is_true(val);

    val = json_object_get(root, "scoreZoom");
    if (val && json_is_boolean(val)) scoreZoom = json_is_true(val);

    val = json_object_get(root, "lowQuality");
    if (val && json_is_boolean(val)) lowQuality = json_is_true(val);

    val = json_object_get(root, "showRatings");
    if (val && json_is_boolean(val)) showRatings = json_is_true(val);

    val = json_object_get(root, "fastNotes");
    if (val && json_is_boolean(val)) fastNotes = json_is_true(val);

    val = json_object_get(root, "globalAntialiasing");
    if (val && json_is_boolean(val)) globalAntialiasing = json_is_true(val);

    val = json_object_get(root, "flashing");
    if (val && json_is_boolean(val)) flashing = json_is_true(val);

    val = json_object_get(root, "disableReset");
    if (val && json_is_boolean(val)) disableReset = json_is_true(val);

    val = json_object_get(root, "botPlay");
    if (val && json_is_boolean(val)) botPlay = json_is_true(val);

    val = json_object_get(root, "noteColorsEnabled");
    if (val && json_is_boolean(val)) noteColorsEnabled = json_is_true(val);

    val = json_object_get(root, "noteOffset");
    if (val && json_is_integer(val)) noteOffset = (int)json_integer_value(val);
    
    val = json_object_get(root, "drawGrid");
    if (val && json_is_boolean(val)) drawGrid = json_is_true(val);

    val = json_object_get(root, "comboOffsetX");
    if (val && json_is_number(val)) comboOffsetX = json_number_value(val);

    val = json_object_get(root, "comboOffsetY");
    if (val && json_is_number(val)) comboOffsetY = json_number_value(val);

    val = json_object_get(root, "comboScale");
    if (val && json_is_number(val)) comboScale = json_number_value(val);

    val = json_object_get(root, "comboAlpha");
    if (val && json_is_number(val)) comboAlpha = json_number_value(val);
    
    val = json_object_get(root, "noteKeys");
    if (val && json_is_array(val)) {
        for (int i = 0; i < 4; i++) {
            json_t *sub = json_array_get(val, i);
            if (sub && json_is_array(sub)) {
                for (int j = 0; j < 2; j++) {
                    json_t *kVal = json_array_get(sub, j);
                    if (kVal && json_is_integer(kVal)) {
                        noteKeys[i][j] = (unsigned int)json_integer_value(kVal);
                    }
                }
            }
        }
    }
    
    val = json_object_get(root, "noteColors");
    if (val && json_is_array(val)) {
        for (int i = 0; i < 4; i++) {
            json_t *sub = json_array_get(val, i);
            if (sub && json_is_array(sub)) {
                for (int j = 0; j < 3; j++) {
                    json_t *cVal = json_array_get(sub, j);
                    if (cVal && json_is_integer(cVal)) {
                        noteColors[i][j] = (unsigned char)json_integer_value(cVal);
                    }
                }
            }
        }
    }
    
    val = json_object_get(root, "alphabetPause");
    if (val && json_is_boolean(val)) alphabetPause = json_is_true(val);
    
    val = json_object_get(root, "checkForUpdates");
    if (val && json_is_boolean(val)) checkForUpdates = json_is_true(val);
    
    val = json_object_get(root, "hitboxEnabled");
    if (val && json_is_boolean(val)) hitboxEnabled = json_is_true(val);
    
    val = json_object_get(root, "hitboxMode");
    if (val && json_is_integer(val)) hitboxMode = (int)json_integer_value(val);
    
    val = json_object_get(root, "hitboxStyle");
    if (val && json_is_integer(val)) hitboxStyle = (int)json_integer_value(val);
    
    val = json_object_get(root, "hitboxAlphaNormal");
    if (val && json_is_integer(val)) hitboxAlphaNormal = (int)json_integer_value(val);
    
    val = json_object_get(root, "hitboxAlphaTouch");
    if (val && json_is_integer(val)) hitboxAlphaTouch = (int)json_integer_value(val);

    val = json_object_get(root, "noteUnderlayAlpha");
    if (val && json_is_number(val)) noteUnderlayAlpha = (float)json_number_value(val);

    val = json_object_get(root, "opponentUnderlay");
    if (val && json_is_boolean(val)) opponentUnderlay = json_is_true(val);

    val = json_object_get(root, "acceptedEgg");
    if (val && json_is_boolean(val)) acceptedEgg = json_is_true(val);

    val = json_object_get(root, "eggInteractionOccurred");
    if (val && json_is_boolean(val)) eggInteractionOccurred = json_is_true(val);

    val = json_object_get(root, "fpsLimit");
    if (val && json_is_integer(val)) fpsLimit = (int)json_integer_value(val);
    
    json_decref(root);


}

void ClientPrefs::saveSettings() {
    std::string basePath = ModHandler::getWorkingBase();
    mkdir(basePath.c_str(), 0777);
    json_t *root = json_object();
    
    json_object_set_new(root, "downscroll", downscroll ? json_true() : json_false());
    json_object_set_new(root, "middleScroll", middleScroll ? json_true() : json_false());
    json_object_set_new(root, "opponentStrums", opponentStrums ? json_true() : json_false());
    json_object_set_new(root, "opponentNotes", opponentNotes ? json_true() : json_false());
    json_object_set_new(root, "ghostTapping", ghostTapping ? json_true() : json_false());
    json_object_set_new(root, "debugInfo", debugInfo ? json_true() : json_false());
    json_object_set_new(root, "extendedDebug", extendedDebug ? json_true() : json_false());
    json_object_set_new(root, "camZooms", camZooms ? json_true() : json_false());
    json_object_set_new(root, "timeBarType", json_integer(timeBarType));
    json_object_set_new(root, "healthBar", healthBar ? json_true() : json_false());
    json_object_set_new(root, "scoreZoom", scoreZoom ? json_true() : json_false());
    json_object_set_new(root, "lowQuality", lowQuality ? json_true() : json_false());
    json_object_set_new(root, "showRatings", showRatings ? json_true() : json_false());
    json_object_set_new(root, "fastNotes", fastNotes ? json_true() : json_false());
    json_object_set_new(root, "globalAntialiasing", globalAntialiasing ? json_true() : json_false());
    json_object_set_new(root, "flashing", flashing ? json_true() : json_false());
    json_object_set_new(root, "disableReset", disableReset ? json_true() : json_false());
    json_object_set_new(root, "botPlay", botPlay ? json_true() : json_false());
    json_object_set_new(root, "noteColorsEnabled", noteColorsEnabled ? json_true() : json_false());
    json_object_set_new(root, "noteOffset", json_integer(noteOffset));
    json_object_set_new(root, "drawGrid", drawGrid ? json_true() : json_false());
    json_object_set_new(root, "comboOffsetX", json_real(comboOffsetX));
    json_object_set_new(root, "comboOffsetY", json_real(comboOffsetY));
    json_object_set_new(root, "comboScale", json_real(comboScale));
    json_object_set_new(root, "comboAlpha", json_real(comboAlpha));
    json_object_set_new(root, "alphabetPause", alphabetPause ? json_true() : json_false());
    json_object_set_new(root, "checkForUpdates", checkForUpdates ? json_true() : json_false());
    json_object_set_new(root, "hitboxEnabled", hitboxEnabled ? json_true() : json_false());
    json_object_set_new(root, "hitboxMode", json_integer(hitboxMode));
    json_object_set_new(root, "hitboxStyle", json_integer(hitboxStyle));
    json_object_set_new(root, "hitboxAlphaNormal", json_integer(hitboxAlphaNormal));
    json_object_set_new(root, "hitboxAlphaTouch", json_integer(hitboxAlphaTouch));
    json_object_set_new(root, "noteUnderlayAlpha", json_real(noteUnderlayAlpha));
    json_object_set_new(root, "opponentUnderlay", opponentUnderlay ? json_true() : json_false());
    json_object_set_new(root, "acceptedEgg", acceptedEgg ? json_true() : json_false());
    json_object_set_new(root, "eggInteractionOccurred", eggInteractionOccurred ? json_true() : json_false());
    json_object_set_new(root, "fpsLimit", json_integer(fpsLimit));

    
    json_t *keysArr = json_array();
    for (int i = 0; i < 4; i++) {
        json_t *sub = json_array();
        json_array_append_new(sub, json_integer(noteKeys[i][0]));
        json_array_append_new(sub, json_integer(noteKeys[i][1]));
        json_array_append_new(keysArr, sub);
    }
    json_object_set_new(root, "noteKeys", keysArr);
    
    json_t *colorsArr = json_array();
    for (int i = 0; i < 4; i++) {
        json_t *sub = json_array();
        json_array_append_new(sub, json_integer(noteColors[i][0]));
        json_array_append_new(sub, json_integer(noteColors[i][1]));
        json_array_append_new(sub, json_integer(noteColors[i][2]));
        json_array_append_new(colorsArr, sub);
    }
    json_object_set_new(root, "noteColors", colorsArr);
    
    std::string path = basePath + "options.json";
    json_dump_file(root, path.c_str(), 0);
    json_decref(root);
}
