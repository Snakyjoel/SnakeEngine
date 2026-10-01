#include "LegacyCompatState.hpp"
#include "../backend/AudioEngine.hpp"
#include <citro2d.h>
#include <cstddef>
#include <string>

extern C2D_Font globalVCRFont;

static const uint8_t _ui_xk = 0x37;

static const uint8_t _msg_0[] = { 0x64, 0x63, 0x78, 0x67, 0x37 };

static const uint8_t _msg_1[] = {
    0x5f, 0x56, 0x44, 0x17, 0x55, 0x52, 0x52, 0x59, 0x17, 0x53,
    0x52, 0x43, 0x52, 0x54, 0x43, 0x52, 0x53, 0x19, 0x37
};

static const uint8_t _msg_2[] = {
    0x64, 0x59, 0x56, 0x5c, 0x52, 0x17, 0x72, 0x59, 0x50, 0x5e,
    0x59, 0x52, 0x17, 0x45, 0x52, 0x5d, 0x52, 0x54, 0x43, 0x44,
    0x17, 0x43, 0x5f, 0x5e, 0x44, 0x17, 0x5a, 0x58, 0x53, 0x19, 0x37
};

static const uint8_t _msg_3[] = {
    0x7e, 0x43, 0x44, 0x17, 0x54, 0x58, 0x59, 0x43, 0x52, 0x59,
    0x43, 0x17, 0x5e, 0x44, 0x17, 0x53, 0x5e, 0x44, 0x45, 0x52,
    0x44, 0x47, 0x52, 0x54, 0x43, 0x51, 0x42, 0x5b, 0x37
};

static const uint8_t _msg_4[] = {
    0x43, 0x58, 0x17, 0x45, 0x52, 0x56, 0x5b, 0x17, 0x47, 0x52,
    0x58, 0x47, 0x5b, 0x52, 0x17, 0x56, 0x59, 0x53, 0x17, 0x41,
    0x5e, 0x58, 0x5b, 0x56, 0x43, 0x52, 0x44, 0x37
};

static const uint8_t _msg_5[] = {
    0x43, 0x5f, 0x52, 0x17, 0x64, 0x59, 0x56, 0x5c, 0x52, 0x17,
    0x72, 0x59, 0x50, 0x5e, 0x59, 0x52, 0x17, 0x5b, 0x5e, 0x54,
    0x52, 0x59, 0x44, 0x52, 0x19, 0x37
};

static const uint8_t _msg_6[] = {
    0x63, 0x5f, 0x5e, 0x44, 0x17, 0x5e, 0x44, 0x17, 0x5f, 0x56,
    0x45, 0x5a, 0x51, 0x42, 0x5b, 0x17, 0x43, 0x58, 0x17, 0x4e,
    0x58, 0x42, 0x19, 0x37
};

static const uint8_t _msg_7[] = {
    0x6e, 0x58, 0x42, 0x17, 0x56, 0x45, 0x52, 0x17, 0x47, 0x52,
    0x45, 0x5a, 0x56, 0x59, 0x52, 0x59, 0x43, 0x5b, 0x4e, 0x17,
    0x55, 0x56, 0x59, 0x59, 0x52, 0x53, 0x19, 0x37
};

static const uint8_t _msg_8[] = {
    0x17, 0x17, 0x1a, 0x17, 0x64, 0x59, 0x56, 0x5c, 0x4e,
    0x5d, 0x58, 0x52, 0x5b, 0x37
};

static std::string resolveStr(const uint8_t* enc) {
    std::string out;
    for (size_t i = 0; enc[i] != _ui_xk; i++) {
        out += (char)(enc[i] ^ _ui_xk);
    }
    return out;
}

LegacyCompatState::LegacyCompatState(const std::string& modName)
    : _mod(modName.size() > 24 ? modName.substr(0, 21) + "..." : modName) {}

void LegacyCompatState::init() {
    MusicPlayer::stop();
    AudioEngine::pause();

    msgBuf = C2D_TextBufNew(512);

    auto parse = [&](C2D_Text& t, const std::string& s) {
        C2D_TextFontParse(&t, globalVCRFont, msgBuf, s.c_str());
        C2D_TextOptimize(&t);
    };

    parse(txtStop,    resolveStr(_msg_0));
    parse(txtModLine, "\"" + _mod + "\" " + resolveStr(_msg_1));
    parse(txtLine1,   resolveStr(_msg_2));
    parse(txtLine2,   resolveStr(_msg_3));
    parse(txtLine3,   resolveStr(_msg_4));
    parse(txtLine4,   resolveStr(_msg_5));
    parse(txtLine5,   resolveStr(_msg_6));
    parse(txtLine6,   resolveStr(_msg_7));
    parse(txtSig,     resolveStr(_msg_8));
}

void LegacyCompatState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(top);

    if (globalVCRFont) {
        const float stopScale = 1.4f;
        const u32   red       = C2D_Color32(220, 40, 40, 255);
        float sw = 0, sh = 0;
        C2D_TextGetDimensions(&txtStop, stopScale, stopScale, &sw, &sh);
        C2D_DrawText(&txtStop, C2D_WithColor,
                     200.0f - sw * 0.5f,
                     120.0f - sh * 0.5f,
                     0.5f, stopScale, stopScale, red);
    }

    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(bottom);

    if (!globalVCRFont || !msgBuf) return;

    const float cx   = 160.0f;
    const float maxW = 300.0f;
    const float sc   = 0.8f;
    const float scSm = 0.65f;
    const u32   red  = C2D_Color32(220, 60,  60,  255);
    const u32   wht  = C2D_Color32(220, 220, 220, 255);
    const u32   dim  = C2D_Color32(110, 110, 110, 255);

    // Draws centered text, auto-scales down if wider than maxW.
    auto draw = [&](C2D_Text& t, float y, float scale, u32 color) -> float {
        float w = 0, h = 0;
        C2D_TextGetDimensions(&t, scale, scale, &w, &h);
        if (w > maxW) {
            scale = scale * (maxW / w);
            C2D_TextGetDimensions(&t, scale, scale, &w, &h);
        }
        C2D_DrawText(&t, C2D_WithColor, cx - w * 0.5f, y, 0.5f, scale, scale, color);
        return h;
    };

    float y   = 5.0f;
    float gap = 3.0f;
    float h;

    h = draw(txtModLine, y, sc,   red);   y += h + gap;
    C2D_DrawRectSolid(10.0f, y + 1.5f, 0.5f, 300.0f, 1.0f, C2D_Color32(70, 70, 70, 255));
    y += 6.0f;

    h = draw(txtLine1, y, sc,   wht); y += h + gap;
    h = draw(txtLine2, y, scSm, wht); y += h + gap;
    h = draw(txtLine3, y, scSm, wht); y += h + gap;
    h = draw(txtLine4, y, scSm, wht); y += h + gap * 2.5f;
    h = draw(txtLine5, y, scSm, dim); y += h + gap * 2.5f;
        draw(txtLine6, y, sc,   wht);

    float sw = 0, sh = 0;
    C2D_TextGetDimensions(&txtSig, scSm, scSm, &sw, &sh);
    C2D_DrawText(&txtSig, C2D_WithColor,
                 320.0f - sw - 8.0f,
                 240.0f - sh - 5.0f,
                 0.5f, scSm, scSm, dim);
}

void LegacyCompatState::exitState() {
    if (msgBuf) {
        C2D_TextBufDelete(msgBuf);
        msgBuf = nullptr;
    }
}
