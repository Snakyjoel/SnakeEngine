#include "UpdateChecker.hpp"
#include "../backend/savedata/ClientPrefs.hpp"
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
static const std::string s_currentVersion = "2.6.7";

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
        std::string currentUrl = "https://gamejolt.com/site-api/web/discover/games/packages/1074690";
        httpcContext context;
        int redirectCount = 0;
        bool success = false;
        u32 status = 0;
        
        while (redirectCount < 5) {
            if (logFile) {
                fprintf(logFile, "Opening HTTP context to: %s\n", currentUrl.c_str());
                fflush(logFile);
            }
            
            ret = httpcOpenContext(&context, HTTPC_METHOD_GET, currentUrl.c_str(), 1);
            if (R_FAILED(ret)) {
                if (logFile) fprintf(logFile, "Failed to open context. Ret: 0x%08X\n", (unsigned int)ret);
                break;
            }
            
            httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
            httpcAddRequestHeaderField(&context, "User-Agent", "SnakeEngine-3DS");
            httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
            
            if (logFile) {
                fprintf(logFile, "Beginning HTTP Request...\n");
                fflush(logFile);
            }
            
            ret = httpcBeginRequest(&context);
            if (R_FAILED(ret)) {
                if (logFile) fprintf(logFile, "Failed to begin request. Ret: 0x%08X\n", (unsigned int)ret);
                httpcCloseContext(&context);
                break;
            }
            
            status = 0;
            ret = httpcGetResponseStatusCode(&context, &status);
            if (R_FAILED(ret)) {
                if (logFile) fprintf(logFile, "Failed to get status code. Ret: 0x%08X\n", (unsigned int)ret);
                httpcCloseContext(&context);
                break;
            }
            
            if (logFile) fprintf(logFile, "HTTP Status Code: %u (Redirect count: %d)\n", (unsigned int)status, redirectCount);
            
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                char location[1024];
                std::memset(location, 0, sizeof(location));
                ret = httpcGetResponseHeader(&context, "Location", location, sizeof(location));
                httpcCloseContext(&context);
                if (R_FAILED(ret) || std::strlen(location) == 0) {
                    if (logFile) fprintf(logFile, "Failed to get Location header. Ret: 0x%08X\n", (unsigned int)ret);
                    break;
                }
                currentUrl = location;
                redirectCount++;
            } else {
                success = (status == 200);
                break;
            }
        }
        
        if (success) {
            u32 contentsize = 0;
            httpcGetDownloadSizeState(&context, NULL, &contentsize);
            if (logFile) fprintf(logFile, "Content size: %u\n", (unsigned int)contentsize);
            
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
            
            if (!result.empty()) {
                if (logFile) fprintf(logFile, "Downloaded JSON: %s\n", result.c_str());
                
                json_error_t jerror;
                json_t* root = json_loads(result.c_str(), 0, &jerror);
                if (root) {
                    json_t* payload = json_object_get(root, "payload");
                    if (payload) {
                        json_t* releases = json_object_get(payload, "releases");
                        if (releases && json_is_array(releases) && json_array_size(releases) > 0) {
                            json_t* firstRelease = json_array_get(releases, 0);
                            if (firstRelease) {
                                json_t* ver = json_object_get(firstRelease, "version_number");
                                if (ver && json_is_string(ver)) {
                                    std::string tagStr = json_string_value(ver);
                                    if (!tagStr.empty() && tagStr[0] == 'v') {
                                        tagStr = tagStr.substr(1);
                                    }
                                    s_onlineVersion = tagStr;
                                    if (logFile) fprintf(logFile, "Success! Version parsed: %s\n", s_onlineVersion.c_str());
                                }
                            }
                        }
                    }
                    json_decref(root);
                } else {
                    if (logFile) fprintf(logFile, "JSON parsing error: %s on line %d\n", jerror.text, jerror.line);
                }
            } else {
                if (logFile) fprintf(logFile, "Failed to download data. Ret: 0x%08X\n", (unsigned int)ret);
            }
            httpcCloseContext(&context);
        } else {
            if (logFile) fprintf(logFile, "No success status (or too many redirects). Last status: %u\n", (unsigned int)status);
        }
        
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

std::string UpdateChecker::getCurrentVersion() {
    return s_currentVersion;
}
