#include "UpdateChecker.hpp"
#include "../backend/savedata/ClientPrefs.hpp"
#include "../states/MainMenuState.hpp"
#include <3ds.h>
#include <jansson.h>
#include <sstream>
#include <sys/stat.h>
#include <3ds/services/sslc.h>
#include <malloc.h>
#include <cstring>

static bool s_updateCheckDone = false;
static bool s_updateCheckInProgress = false;
static std::string s_onlineVersion = "";
static std::string s_updateSource = "gamebanana";

static Thread s_updateThread = nullptr;
static u32* s_socBuffer = nullptr;
#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000 // 1MB buffer

static int safeParseInt(const std::string& s) {
    int val = 0;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            val = val * 10 + (c - '0');
        }
    }
    return val;
}

int UpdateChecker::compareVersions(const std::string& v1, const std::string& v2) {
    if (v1 == v2) return 0;
    
    std::vector<int> parts1, parts2;
    std::stringstream ss1(v1), ss2(v2);
    std::string item;
    while (std::getline(ss1, item, '.')) {
        parts1.push_back(safeParseInt(item));
    }
    while (std::getline(ss2, item, '.')) {
        parts2.push_back(safeParseInt(item));
    }
    
    while (parts1.size() < parts2.size()) parts1.push_back(0);
    while (parts2.size() < parts1.size()) parts2.push_back(0);
    
    for (size_t i = 0; i < parts1.size(); i++) {
        if (parts1[i] < parts2[i]) return -1;
        if (parts1[i] > parts2[i]) return 1;
    }
    return 0;
}

static std::string cleanVersionString(std::string ver) {
    if (!ver.empty() && (ver[0] == 'v' || ver[0] == 'V')) {
        ver = ver.substr(1);
    }
    size_t start = ver.find_first_not_of(" \t\r\n");
    if (start != std::string::npos) ver = ver.substr(start);
    
    size_t spacePos = ver.find_first_of(" \t\r\n");
    if (spacePos != std::string::npos) {
        ver = ver.substr(0, spacePos);
    }
    return ver;
}

static std::string httpFetch(const std::string& url, FILE* logFile) {
    httpcContext context;
    std::string currentUrl = url;
    int redirectCount = 0;
    bool success = false;
    u32 status = 0;
    Result ret = 0;

    while (redirectCount < 5) {
        if (logFile) {
            fprintf(logFile, "Opening HTTP context to: %s\n", currentUrl.c_str());
            fflush(logFile);
        }

        ret = httpcOpenContext(&context, HTTPC_METHOD_GET, currentUrl.c_str(), 1);
        if (R_FAILED(ret)) {
            if (logFile) fprintf(logFile, "Failed to open context. Ret: 0x%08X\n", (unsigned int)ret);
            return "";
        }

        httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
        httpcAddRequestHeaderField(&context, "User-Agent", "SnakeEngine-3DS");
        httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);

        ret = httpcBeginRequest(&context);
        if (R_FAILED(ret)) {
            if (logFile) fprintf(logFile, "Failed to begin request. Ret: 0x%08X\n", (unsigned int)ret);
            httpcCloseContext(&context);
            return "";
        }

        status = 0;
        ret = httpcGetResponseStatusCode(&context, &status);
        if (R_FAILED(ret)) {
            if (logFile) fprintf(logFile, "Failed to get status code. Ret: 0x%08X\n", (unsigned int)ret);
            httpcCloseContext(&context);
            return "";
        }

        if (logFile) fprintf(logFile, "HTTP Status Code: %u (Redirect count: %d)\n", (unsigned int)status, redirectCount);

        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            char location[1024];
            std::memset(location, 0, sizeof(location));
            ret = httpcGetResponseHeader(&context, "Location", location, sizeof(location));
            httpcCloseContext(&context);
            if (R_FAILED(ret) || std::strlen(location) == 0) {
                if (logFile) fprintf(logFile, "Failed to get Location header. Ret: 0x%08X\n", (unsigned int)ret);
                return "";
            }
            currentUrl = location;
            redirectCount++;
        } else {
            success = (status == 200);
            break;
        }
    }

    if (!success) {
        if (logFile) fprintf(logFile, "No success status code (%u)\n", (unsigned int)status);
        httpcCloseContext(&context);
        return "";
    }

    u32 contentsize = 0;
    httpcGetDownloadSizeState(&context, NULL, &contentsize);
    std::string result;
    u32 readsize = 0;
    if (contentsize > 0) {
        result.resize(contentsize);
        ret = httpcDownloadData(&context, (u8*)&result[0], contentsize, &readsize);
        if (R_SUCCEEDED(ret)) {
            result.resize(readsize);
        }
    } else {
        char chunk[4096];
        while (true) {
            u32 bytesRead = 0;
            Result downloadRet = httpcDownloadData(&context, (u8*)chunk, sizeof(chunk), &bytesRead);
            if (bytesRead > 0) {
                result.append(chunk, bytesRead);
            }
            if (downloadRet == (Result)HTTPC_RESULTCODE_DOWNLOADPENDING) {
                continue;
            }
            if (downloadRet == 0 || bytesRead == 0) {
                break;
            }
            if (R_FAILED(downloadRet)) {
                break;
            }
        }
    }

    httpcCloseContext(&context);
    return result;
}

static std::string parseGameBananaVersion(const std::string& jsonStr, FILE* logFile) {
    json_error_t jerror;
    json_t* root = json_loads(jsonStr.c_str(), 0, &jerror);
    if (!root) {
        if (logFile) fprintf(logFile, "GameBanana JSON parse error: %s on line %d\n", jerror.text, jerror.line);
        return "";
    }

    std::string foundVer = "";
    if (json_is_array(root) && json_array_size(root) >= 2) {
        json_t* filesObj = json_array_get(root, 1);
        if (filesObj && json_is_object(filesObj)) {
            const char* key;
            json_t* fileVal;
            json_object_foreach(filesObj, key, fileVal) {
                if (fileVal && json_is_object(fileVal)) {
                    json_t* verObj = json_object_get(fileVal, "_sVersion");
                    if (verObj && json_is_string(verObj)) {
                        std::string rawVer = json_string_value(verObj);
                        std::string cleaned = cleanVersionString(rawVer);
                        if (!cleaned.empty()) {
                            if (foundVer.empty() || UpdateChecker::compareVersions(cleaned, foundVer) > 0) {
                                foundVer = cleaned;
                            }
                        }
                    }
                }
            }
        }
    }

    json_decref(root);
    return foundVer;
}



static void updateCheckThreadFunc(void* arg) {
    s_socBuffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    
    mkdir("sdmc:/SnakeEngine", 0777);
    FILE* logFile = fopen("sdmc:/SnakeEngine/update_log.txt", "w");
    if (logFile) {
        fprintf(logFile, "Update Check Thread Started.\n");
        fflush(logFile);
    }

    if (!s_socBuffer) {
        if (logFile) {
            fprintf(logFile, "Error: Failed to allocate aligned SOC buffer memory.\n");
            fclose(logFile);
        }
        s_updateCheckDone = true;
        s_updateCheckInProgress = false;
        return;
    }

    Result socRes = socInit(s_socBuffer, SOC_BUFFERSIZE);
    if (R_FAILED(socRes)) {
        if (logFile) {
            fprintf(logFile, "socInit failed with Result: 0x%08X\n", (unsigned int)socRes);
            fclose(logFile);
        }
        free(s_socBuffer);
        s_socBuffer = nullptr;
        s_updateCheckDone = true;
        s_updateCheckInProgress = false;
        return;
    }

    if (logFile) {
        fprintf(logFile, "socInit initialized successfully. Starting httpc...\n");
        fflush(logFile);
    }

    Result ret = httpcInit(0);
    if (R_FAILED(ret)) {
        if (logFile) fprintf(logFile, "httpcInit failed. Ret: 0x%08X\n", (unsigned int)ret);
    } else {
        // Fetch GameBanana API (Tool 23841)
        if (logFile) fprintf(logFile, "Fetching GameBanana API...\n");
        std::string gbJson = httpFetch("https://api.gamebanana.com/Core/Item/Data?itemtype=Tool&itemid=23841&fields=name,Files().aFiles()", logFile);
        if (!gbJson.empty()) {
            std::string gbVer = parseGameBananaVersion(gbJson, logFile);
            if (!gbVer.empty()) {
                s_onlineVersion = gbVer;
                s_updateSource = "gamebanana";
                if (logFile) fprintf(logFile, "GameBanana Version parsed: %s\n", s_onlineVersion.c_str());
            }
        }

        if (logFile) fprintf(logFile, "Final online version selected: %s\n", s_onlineVersion.c_str());

        httpcExit();
    }

    socExit();
    if (s_socBuffer) {
        free(s_socBuffer);
        s_socBuffer = nullptr;
    }

    s_updateCheckDone = true;
    s_updateCheckInProgress = false;
    
    if (logFile) {
        fprintf(logFile, "Thread exited cleanly. Check finished!\n");
        fclose(logFile);
    }
}

void UpdateChecker::startCheck() {
    if (ClientPrefs::checkForUpdates && !s_updateCheckDone && !s_updateCheckInProgress) {
        s_updateCheckInProgress = true;
        s_updateThread = threadCreate(updateCheckThreadFunc, nullptr, 32 * 1024, 0x3F, -2, true);
    }
}

bool UpdateChecker::isFinished() {
    return s_updateCheckDone;
}

bool UpdateChecker::isChecking() {
    return s_updateCheckInProgress;
}

std::string UpdateChecker::getOnlineVersion() {
    return s_onlineVersion;
}

std::string UpdateChecker::getUpdateSource() {
    return s_updateSource;
}

std::string UpdateChecker::getCurrentVersion() {
    return MainMenuState::version;
}
