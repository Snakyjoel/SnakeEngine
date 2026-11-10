#include "EggDuelState.hpp"
#include "TitleState.hpp"
#include "backend/AudioEngine.hpp"
#include "attacks/EggAttack.hpp"
#include "attacks/AttackCrossSlash.hpp"
#include "attacks/AttackBlueSlam.hpp"
#include "attacks/AttackOrbitSpheres.hpp"
#include <cmath>
#include <algorithm>

extern C2D_Font globalVCRFont;
extern C2D_Font globalPixelFont;

static inline void setScissorBoxBottom(float x, float y, float w, float h) {
    C2D_Flush();
    if (w <= 0.0f || h <= 0.0f) {
        C3D_SetScissor(GPU_SCISSOR_NORMAL, 0, 0, 0, 0);
        return;
    }
    float sLeft = y;
    float sRight = y + h;
    float sTop = x;
    float sBottom = x + w;

    if (sLeft < 0.0f) sLeft = 0.0f;
    if (sRight > 240.0f) sRight = 240.0f;
    if (sTop < 0.0f) sTop = 0.0f;
    if (sBottom > 320.0f) sBottom = 320.0f;

    u32 phys_left = (u32)(240.0f - sRight);
    u32 phys_right = (u32)(240.0f - sLeft);
    u32 phys_top = (u32)(320.0f - sBottom);
    u32 phys_bottom = (u32)(320.0f - sTop);

    C3D_SetScissor(GPU_SCISSOR_NORMAL, phys_left, phys_top, phys_right, phys_bottom);
}

static void playSnd(int chn, int16_t* buf, uint32_t samples, ndspWaveBuf* wbuf, float vol = 1.0f) {
    if (!buf || samples == 0 || !wbuf) return;
    ndspChnSetPaused(chn, true);
    ndspChnReset(chn);
    ndspChnSetInterp(chn, NDSP_INTERP_LINEAR);
    ndspChnSetRate(chn, 44100);
    ndspChnSetFormat(chn, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = { 0 };
    mix[0] = vol;
    mix[1] = vol;
    ndspChnSetMix(chn, mix);

    memset(wbuf, 0, sizeof(ndspWaveBuf));
    wbuf->data_vaddr = buf;
    wbuf->nsamples = samples;
    wbuf->looping = false;
    wbuf->status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(chn, wbuf);
}

EggDuelState::EggDuelState() {}

EggDuelState::~EggDuelState() {
    freeAudio();

    if (soulSheet) {
        C2D_SpriteSheetFree(soulSheet);
        soulSheet = nullptr;
    }
    if (swordSheet) {
        C2D_SpriteSheetFree(swordSheet);
        swordSheet = nullptr;
    }
    if (spikeSheet) {
        C2D_SpriteSheetFree(spikeSheet);
        spikeSheet = nullptr;
    }
    if (skyboxSheet) {
        C2D_SpriteSheetFree(skyboxSheet);
        skyboxSheet = nullptr;
    }
    if (tpSheet) {
        C2D_SpriteSheetFree(tpSheet);
        tpSheet = nullptr;
    }
    if (grazeSheet) {
        C2D_SpriteSheetFree(grazeSheet);
        grazeSheet = nullptr;
    }
    if (optionsSheet) {
        C2D_SpriteSheetFree(optionsSheet);
        optionsSheet = nullptr;
    }
    optionFrames.clear();

    if (textBuf) {
        C2D_TextBufDelete(textBuf);
        textBuf = nullptr;
    }
}

static bool loadWav16Mono44k(const char* romfsPath, const char* altPath, int16_t*& outBuf, uint32_t& outSamples) {
    outBuf = nullptr;
    outSamples = 0;

    FILE* f = fopen(romfsPath, "rb");
    if (!f && altPath) f = fopen(altPath, "rb");
    if (!f) return false;

    char riff[4];
    if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) != 0) {
        fclose(f);
        return false;
    }
    fseek(f, 4, SEEK_CUR);
    char wave[4];
    if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) {
        fclose(f);
        return false;
    }

    uint32_t sampleRate = 44100;
    int channels = 1;
    int bitDepth = 16;
    uint32_t dataBytes = 0;
    bool foundFmt = false, foundData = false;

    while (true) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (fread(chunkId, 1, 4, f) != 4) break;
        if (fread(&chunkSize, 4, 1, f) != 1) break;

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat = 0;
            fread(&audioFormat, 2, 1, f);
            uint16_t chans = 0;
            fread(&chans, 2, 1, f);
            channels = chans;
            fread(&sampleRate, 4, 1, f);
            fseek(f, 6, SEEK_CUR);
            uint16_t bits = 0;
            fread(&bits, 2, 1, f);
            bitDepth = bits;
            if (bitDepth != 16) { fclose(f); return false; }
            if (chunkSize > 16) fseek(f, chunkSize - 16, SEEK_CUR);
            foundFmt = true;
        } else if (memcmp(chunkId, "data", 4) == 0) {
            dataBytes = chunkSize;
            foundData = true;
            break;
        } else {
            fseek(f, chunkSize, SEEK_CUR);
        }
    }

    if (!foundFmt || !foundData || dataBytes == 0) {
        fclose(f);
        return false;
    }

    int16_t* buf = (int16_t*)linearMemAlign(dataBytes, 0x80);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t bytesRead = fread(buf, 1, dataBytes, f);
    fclose(f);

    if (bytesRead < dataBytes) {
        linearFree(buf);
        return false;
    }

    DSP_FlushDataCache(buf, dataBytes);
    outBuf = buf;
    outSamples = (uint32_t)(dataBytes / (channels * sizeof(int16_t)));
    return true;
}

void EggDuelState::initAudio() {
    loadWav16Mono44k("romfs:/preload/sounds/fight/snd_knight_jump_quick.wav",
                     "assets/preload/sounds/fight/snd_knight_jump_quick.wav",
                     jumpQuickBuffer, jumpQuickSampleCount);

    loadWav16Mono44k("romfs:/preload/sounds/fight/snd_knight_cut.wav",
                     "assets/preload/sounds/fight/snd_knight_cut.wav",
                     cut1Buffer, cut1SampleCount);

    loadWav16Mono44k("romfs:/preload/sounds/fight/snd_knight_cut2.wav",
                     "assets/preload/sounds/fight/snd_knight_cut2.wav",
                     cut2Buffer, cut2SampleCount);
}

void EggDuelState::freeAudio() {
    ndspChnSetPaused(8, true);
    ndspChnReset(8);
    ndspChnSetPaused(10, true);
    ndspChnReset(10);
    svcSleepThread(10000000LL);

    if (jumpQuickBuffer) {
        linearFree(jumpQuickBuffer);
        jumpQuickBuffer = nullptr;
    }
    jumpQuickSampleCount = 0;

    if (cut1Buffer) {
        linearFree(cut1Buffer);
        cut1Buffer = nullptr;
    }
    cut1SampleCount = 0;

    if (cut2Buffer) {
        linearFree(cut2Buffer);
        cut2Buffer = nullptr;
    }
    cut2SampleCount = 0;
}

void EggDuelState::playCursorSound() {
    AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
}

void EggDuelState::playSwordAppearSound() {
    playSnd(10, jumpQuickBuffer, jumpQuickSampleCount, &jumpQuickWaveBuf, 0.90f);
}

void EggDuelState::playCutSound() {
    int v = rand() % 2;
    if (v == 0 && cut1Buffer && cut1SampleCount > 0) {
        playSnd(8, cut1Buffer, cut1SampleCount, &cut1WaveBuf, 0.95f);
    } else if (cut2Buffer && cut2SampleCount > 0) {
        playSnd(8, cut2Buffer, cut2SampleCount, &cut2WaveBuf, 0.95f);
    } else if (cut1Buffer && cut1SampleCount > 0) {
        playSnd(8, cut1Buffer, cut1SampleCount, &cut1WaveBuf, 0.95f);
    }
}

void EggDuelState::drawRotatedBox(float cx, float cy, float size, float angleRad, u32 borderColor, u32 fillColor, float depth, float borderThickness) {
    if (size < 2.0f) return;

    float half = size * 0.5f;
    float cosA = cosf(angleRad);
    float sinA = sinf(angleRad);

    float x0 = cx + (-half) * cosA - (-half) * sinA;
    float y0 = cy + (-half) * sinA + (-half) * cosA;
    float x1 = cx + (half) * cosA - (-half) * sinA;
    float y1 = cy + (half) * sinA + (-half) * cosA;
    float x2 = cx + (half) * cosA - (half) * sinA;
    float y2 = cy + (half) * sinA + (half) * cosA;
    float x3 = cx + (-half) * cosA - (half) * sinA;
    float y3 = cy + (-half) * sinA + (half) * cosA;

    u8 fillAlpha = (fillColor >> 24) & 0xFF;
    if (fillAlpha > 0) {
        C2D_DrawTriangle(x0, y0, fillColor, x1, y1, fillColor, x2, y2, fillColor, depth);
        C2D_DrawTriangle(x0, y0, fillColor, x2, y2, fillColor, x3, y3, fillColor, depth);
    }

    u8 borderAlpha = (borderColor >> 24) & 0xFF;
    if (borderAlpha > 0 && borderThickness > 0.0f) {
        float lineDepth = depth + 0.01f;
        C2D_DrawLine(x0, y0, borderColor, x1, y1, borderColor, borderThickness, lineDepth);
        C2D_DrawLine(x1, y1, borderColor, x2, y2, borderColor, borderThickness, lineDepth);
        C2D_DrawLine(x2, y2, borderColor, x3, y3, borderColor, borderThickness, lineDepth);
        C2D_DrawLine(x3, y3, borderColor, x0, y0, borderColor, borderThickness, lineDepth);
    }
}

void EggDuelState::spawnSlashSparks(float cx, float cy, float dirX, float dirY, int count, float speedMin, float speedMax) {
    for (int i = 0; i < count; i++) {
        SlashSpark s;
        s.x = cx;
        s.y = cy;
        float speed = speedMin + ((float)(rand() % 100) / 100.0f) * (speedMax - speedMin);
        float angle;
        if (dirX == 0.0f && dirY == 0.0f) {
            angle = ((float)(rand() % 360)) * (3.14159265f / 180.0f);
        } else {
            float baseAngle = atan2f(dirY, dirX);
            float spread = ((float)(rand() % 100) / 100.0f - 0.5f) * 1.570796f;
            angle = baseAngle + spread;
        }
        s.vx = cosf(angle) * speed;
        s.vy = sinf(angle) * speed;
        s.size = 2.0f + ((float)(rand() % 100) / 100.0f) * 3.5f;
        s.timer = 0.0f;
        s.maxLife = 0.18f + ((float)(rand() % 100) / 100.0f) * 0.10f;
        s.active = true;
        slashSparks.push_back(s);
    }
}

void EggDuelState::spawnSpatialCut(float x1, float y1, float x2, float y2, float duration, float thickness) {
    SpatialCut c;
    c.x1 = x1;
    c.y1 = y1;
    c.x2 = x2;
    c.y2 = y2;
    c.timer = 0.0f;
    c.maxDuration = duration;
    c.maxThickness = thickness;
    c.active = true;
    spatialCuts.push_back(c);
}

void EggDuelState::spawnCrossFlare(float cx, float cy, float radius, float duration) {
    CrossFlare f;
    f.x = cx;
    f.y = cy;
    f.radius = radius;
    f.timer = 0.0f;
    f.maxDuration = duration;
    f.active = true;
    crossFlares.push_back(f);
}

void EggDuelState::updateSlashEffects(float dt) {
    if (slashImpactFlash > 0.0f) {
        slashImpactFlash = std::max(0.0f, slashImpactFlash - dt);
    }

    for (auto& s : slashSparks) {
        if (!s.active) continue;
        s.timer += dt;
        if (s.timer >= s.maxLife) {
            s.active = false;
            continue;
        }
        s.x += s.vx * dt;
        s.y += s.vy * dt;
        s.vx -= s.vx * std::min(1.0f, dt * 7.5f);
        s.vy -= s.vy * std::min(1.0f, dt * 7.5f);
    }
    slashSparks.erase(std::remove_if(slashSparks.begin(), slashSparks.end(),
        [](const SlashSpark& s) { return !s.active; }), slashSparks.end());

    for (auto& c : spatialCuts) {
        if (!c.active) continue;
        c.timer += dt;
        if (c.timer >= c.maxDuration) c.active = false;
    }
    spatialCuts.erase(std::remove_if(spatialCuts.begin(), spatialCuts.end(),
        [](const SpatialCut& c) { return !c.active; }), spatialCuts.end());

    for (auto& f : crossFlares) {
        if (!f.active) continue;
        f.timer += dt;
        if (f.timer >= f.maxDuration) f.active = false;
    }
    crossFlares.erase(std::remove_if(crossFlares.begin(), crossFlares.end(),
        [](const CrossFlare& f) { return !f.active; }), crossFlares.end());
}

void EggDuelState::drawIntersectionFlare(float cx, float cy, float radius, float progress) {
    if (progress >= 1.0f || radius <= 0.5f) return;

    float pFlare = (progress <= 0.20f) ? (progress / 0.20f) : (1.0f - (progress - 0.20f) / 0.80f);
    pFlare = std::max(0.0f, std::min(1.0f, pFlare));
    float r = radius * (0.4f + 0.6f * pFlare);
    u8 alpha = (u8)(pFlare * 255.0f);
    if (alpha < 4) return;

    u32 colCenter = C2D_Color32(255, 255, 255, alpha);
    u32 colMid    = C2D_Color32(255, 60, 40, (u8)(alpha * 0.8f));
    u32 colFade   = C2D_Color32(255, 20, 20, 0);

    float spikeLong = r * 1.5f;
    float spikeShort = r * 0.25f;

    C2D_DrawTriangle(cx - spikeLong, cy, colFade, cx, cy - spikeShort, colMid, cx, cy, colCenter, 0.45f);
    C2D_DrawTriangle(cx - spikeLong, cy, colFade, cx, cy, colCenter, cx, cy + spikeShort, colMid, 0.45f);
    C2D_DrawTriangle(cx + spikeLong, cy, colFade, cx, cy - spikeShort, colMid, cx, cy, colCenter, 0.45f);
    C2D_DrawTriangle(cx + spikeLong, cy, colFade, cx, cy, colCenter, cx, cy + spikeShort, colMid, 0.45f);

    C2D_DrawTriangle(cx, cy - spikeLong, colFade, cx - spikeShort, cy, colMid, cx, cy, colCenter, 0.45f);
    C2D_DrawTriangle(cx, cy - spikeLong, colFade, cx, cy, colCenter, cx + spikeShort, cy, colMid, 0.45f);
    C2D_DrawTriangle(cx, cy + spikeLong, colFade, cx - spikeShort, cy, colMid, cx, cy, colCenter, 0.45f);
    C2D_DrawTriangle(cx, cy + spikeLong, colFade, cx, cy, colCenter, cx + spikeShort, cy, colMid, 0.45f);

    float dLong = r * 0.75f * 0.7071f;
    float dShort = r * 0.18f * 0.7071f;
    C2D_DrawTriangle(cx - dLong, cy - dLong, colFade, cx - dShort, cy + dShort, colMid, cx, cy, colCenter, 0.45f);
    C2D_DrawTriangle(cx + dLong, cy + dLong, colFade, cx + dShort, cy - dShort, colMid, cx, cy, colCenter, 0.45f);
    C2D_DrawTriangle(cx - dLong, cy + dLong, colFade, cx - dShort, cy - dShort, colMid, cx, cy, colCenter, 0.45f);
    C2D_DrawTriangle(cx + dLong, cy - dLong, colFade, cx + dShort, cy + dShort, colMid, cx, cy, colCenter, 0.45f);
}

void EggDuelState::drawTravelingSlashWake(float x1, float y1, float x2, float y2, float maxThickness, float alphaRatio) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 2.0f || maxThickness <= 0.2f || alphaRatio <= 0.01f) return;

    float dirX = dx / len, dirY = dy / len;
    float perpX = -dirY, perpY = dirX;

    u8 aBase = (u8)(std::max(0.0f, std::min(1.0f, alphaRatio)) * 255.0f);
    if (aBase < 4) return;

    const int SEGMENTS = 6;
    float step = 1.0f / (float)SEGMENTS;

    struct WakeVert {
        float topX, topY;
        float botX, botY;
        float topGlowX, topGlowY;
        float botGlowX, botGlowY;
        float topCoreX, topCoreY;
        float botCoreX, botCoreY;
    };
    WakeVert verts[SEGMENTS + 1];

    for (int i = 0; i <= SEGMENTS; i++) {
        float u = (float)i * step;
        float px = x1 + dirX * (u * len);
        float py = y1 + dirY * (u * len);

        float profile = (u < 0.85f) ? (0.15f + 0.85f * (u / 0.85f)) : (1.0f - ((u - 0.85f) / 0.15f) * 0.70f);

        float hBlade = maxThickness * 0.50f * profile;
        float hGlow  = maxThickness * 1.30f * profile;
        float hCore  = maxThickness * 0.22f * profile;

        verts[i].topX = px + perpX * hBlade;
        verts[i].topY = py + perpY * hBlade;
        verts[i].botX = px - perpX * hBlade;
        verts[i].botY = py - perpY * hBlade;

        verts[i].topGlowX = px + perpX * hGlow;
        verts[i].topGlowY = py + perpY * hGlow;
        verts[i].botGlowX = px - perpX * hGlow;
        verts[i].botGlowY = py - perpY * hGlow;

        verts[i].topCoreX = px + perpX * hCore;
        verts[i].topCoreY = py + perpY * hCore;
        verts[i].botCoreX = px - perpX * hCore;
        verts[i].botCoreY = py - perpY * hCore;
    }

    u32 colGlowOuter = C2D_Color32(255, 10, 10, 0);
    u32 colBladeMid  = C2D_Color32(255, 55, 30, (u8)(aBase * 0.90f));
    u32 colCore      = C2D_Color32(255, 255, 255, aBase);
    u32 colCoreFade  = C2D_Color32(255, 255, 255, (u8)(aBase * 0.2f));

    for (int i = 0; i < SEGMENTS; i++) {
        const auto& v0 = verts[i];
        const auto& v1 = verts[i + 1];

        C2D_DrawTriangle(v0.topX, v0.topY, colBladeMid, v1.topX, v1.topY, colBladeMid, v1.topGlowX, v1.topGlowY, colGlowOuter, 0.43f);
        C2D_DrawTriangle(v0.topX, v0.topY, colBladeMid, v1.topGlowX, v1.topGlowY, colGlowOuter, v0.topGlowX, v0.topGlowY, colGlowOuter, 0.43f);
        C2D_DrawTriangle(v0.botX, v0.botY, colBladeMid, v1.botGlowX, v1.botGlowY, colGlowOuter, v1.botX, v1.botY, colBladeMid, 0.43f);
        C2D_DrawTriangle(v0.botX, v0.botY, colBladeMid, v0.botGlowX, v0.botGlowY, colGlowOuter, v1.botGlowX, v1.botGlowY, colGlowOuter, 0.43f);

        C2D_DrawTriangle(v0.topX, v0.topY, colBladeMid, v1.topX, v1.topY, colBladeMid, v1.botX, v1.botY, colBladeMid, 0.44f);
        C2D_DrawTriangle(v0.topX, v0.topY, colBladeMid, v1.botX, v1.botY, colBladeMid, v0.botX, v0.botY, colBladeMid, 0.44f);

        u32 c0 = (i == 0) ? colCoreFade : colCore;
        C2D_DrawTriangle(v0.topCoreX, v0.topCoreY, c0, v1.topCoreX, v1.topCoreY, colCore, v1.botCoreX, v1.botCoreY, colCore, 0.45f);
        C2D_DrawTriangle(v0.topCoreX, v0.topCoreY, c0, v1.botCoreX, v1.botCoreY, colCore, v0.botCoreX, v0.botCoreY, c0, 0.45f);
    }
}

void EggDuelState::drawSweepingCut(float startX, float startY, float endX, float endY, float maxThickness, float timer, float sweepDuration, float totalDuration) {
    if (timer < 0.0f || timer >= totalDuration || maxThickness <= 0.2f) return;

    if (timer <= sweepDuration) {
        float u = timer / sweepDuration;
        float sweepEased = 1.0f - powf(1.0f - u, 2.5f);
        float tipX = startX + (endX - startX) * sweepEased;
        float tipY = startY + (endY - startY) * sweepEased;
        drawTravelingSlashWake(startX, startY, tipX, tipY, maxThickness, 1.0f);
    } else {
        float progress = (timer - sweepDuration) / (totalDuration - sweepDuration);
        float midX = (startX + endX) * 0.5f;
        float midY = (startY + endY) * 0.5f;
        float dx = endX - startX;
        float dy = endY - startY;
        float fullLen = sqrtf(dx * dx + dy * dy);
        if (fullLen > 0.001f) {
            drawCurvedSlashIncision(midX, midY, dx / fullLen, dy / fullLen, fullLen * 0.5f, maxThickness, progress);
        }
    }
}

void EggDuelState::drawCurvedSlashIncision(float cx, float cy, float dirX, float dirY, float length, float maxThickness, float progress) {
    if (progress >= 1.0f || maxThickness <= 0.2f) return;

    float swell = (progress <= 0.20f) ? (sinf(progress / 0.20f * 1.570796f)) : powf(1.0f - (progress - 0.20f) / 0.80f, 1.4f);
    float thick = maxThickness * swell;
    float alphaF = (progress <= 0.15f) ? 1.0f : (1.0f - (progress - 0.15f) / 0.85f);
    u8 aBase = (u8)(std::max(0.0f, std::min(1.0f, alphaF)) * 255.0f);
    if (aBase < 4) return;

    float perpX = -dirY, perpY = dirX;
    const int SEGMENTS = 6;
    float step = 2.0f / (float)SEGMENTS;

    struct RibbonVert {
        float topX, topY, botX, botY;
        float topGlowX, topGlowY, botGlowX, botGlowY;
        float topCoreX, topCoreY, botCoreX, botCoreY;
    };
    RibbonVert verts[SEGMENTS + 1];

    for (int i = 0; i <= SEGMENTS; i++) {
        float s = -1.0f + (float)i * step;
        float profile = std::max(0.0f, 1.0f - (s * s));

        float distAlong = s * length;
        float px = cx + dirX * distAlong;
        float py = cy + dirY * distAlong;

        float hBlade = thick * 0.5f * profile;
        float hGlow  = thick * 1.35f * profile;
        float hCore  = thick * 0.22f * profile;

        verts[i].topX = px + perpX * hBlade;
        verts[i].topY = py + perpY * hBlade;
        verts[i].botX = px - perpX * hBlade;
        verts[i].botY = py - perpY * hBlade;

        verts[i].topGlowX = px + perpX * hGlow;
        verts[i].topGlowY = py + perpY * hGlow;
        verts[i].botGlowX = px - perpX * hGlow;
        verts[i].botGlowY = py - perpY * hGlow;

        verts[i].topCoreX = px + perpX * hCore;
        verts[i].topCoreY = py + perpY * hCore;
        verts[i].botCoreX = px - perpX * hCore;
        verts[i].botCoreY = py - perpY * hCore;
    }

    u32 colGlowOuter = C2D_Color32(255, 10, 10, 0);
    u32 colBladeMid  = C2D_Color32(255, 55, 30, (u8)(aBase * 0.90f));
    u32 colCore      = C2D_Color32(255, 255, 255, aBase);
    u32 colCoreFade  = C2D_Color32(255, 255, 255, 0);

    for (int i = 0; i < SEGMENTS; i++) {
        const auto& v0 = verts[i];
        const auto& v1 = verts[i + 1];

        C2D_DrawTriangle(v0.topX, v0.topY, colBladeMid, v1.topX, v1.topY, colBladeMid, v1.topGlowX, v1.topGlowY, colGlowOuter, 0.43f);
        C2D_DrawTriangle(v0.topX, v0.topY, colBladeMid, v1.topGlowX, v1.topGlowY, colGlowOuter, v0.topGlowX, v0.topGlowY, colGlowOuter, 0.43f);
        C2D_DrawTriangle(v0.botX, v0.botY, colBladeMid, v1.botGlowX, v1.botGlowY, colGlowOuter, v1.botX, v1.botY, colBladeMid, 0.43f);
        C2D_DrawTriangle(v0.botX, v0.botY, colBladeMid, v0.botGlowX, v0.botGlowY, colGlowOuter, v1.botGlowX, v1.botGlowY, colGlowOuter, 0.43f);

        C2D_DrawTriangle(v0.topX, v0.topY, colBladeMid, v1.topX, v1.topY, colBladeMid, v1.botX, v1.botY, colBladeMid, 0.44f);
        C2D_DrawTriangle(v0.topX, v0.topY, colBladeMid, v1.botX, v1.botY, colBladeMid, v0.botX, v0.botY, colBladeMid, 0.44f);

        u32 c0 = (i == 0) ? colCoreFade : colCore;
        u32 c1 = (i == SEGMENTS - 1) ? colCoreFade : colCore;
        C2D_DrawTriangle(v0.topCoreX, v0.topCoreY, c0, v1.topCoreX, v1.topCoreY, c1, v1.botCoreX, v1.botCoreY, c1, 0.45f);
        C2D_DrawTriangle(v0.topCoreX, v0.topCoreY, c0, v1.botCoreX, v1.botCoreY, c1, v0.botCoreX, v0.botCoreY, c0, 0.45f);
    }
}

void EggDuelState::drawSlashEffects() {
    for (const auto& f : crossFlares) {
        if (!f.active) continue;
        float progress = f.timer / f.maxDuration;
        drawIntersectionFlare(f.x, f.y, f.radius, progress);
    }

    for (const auto& c : spatialCuts) {
        if (!c.active) continue;
        float progress = c.timer / c.maxDuration;
        float midX = (c.x1 + c.x2) * 0.5f;
        float midY = (c.y1 + c.y2) * 0.5f;
        float dx = c.x2 - c.x1;
        float dy = c.y2 - c.y1;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.001f) {
            drawCurvedSlashIncision(midX, midY, dx / len, dy / len, len * 0.5f, c.maxThickness, progress);
        }
    }

    for (const auto& s : slashSparks) {
        if (!s.active) continue;
        float progress = s.timer / s.maxLife;
        u8 a = (u8)((1.0f - progress) * 255.0f);
        if (a < 5) continue;
        float curSize = s.size * (1.0f - progress * 0.45f);
        u32 col = (progress < 0.4f) ? C2D_Color32(255, 255, 255, a) : C2D_Color32(255, 60, 40, a);
        C2D_DrawRectSolid(s.x - curSize * 0.5f, s.y - curSize * 0.5f, 0.46f, curSize, curSize, col);
    }
}

void EggDuelState::spawnBullet(const Bullet& b) {
    bullets.push_back(b);
}

void EggDuelState::applyDamage(int directDmg, int karmaDmg, float iFrameDur) {
    if (iFrameTimer > 0.0f) return;

    if (isDefending) {
        directDmg = (int)((float)directDmg * 0.65f);
        karmaDmg = (int)((float)karmaDmg * 0.65f);
    }

    curHp -= directDmg;
    if (curHp < 0) curHp = 0;

    curKarma += karmaDmg;
    if (curKarma > curHp) curKarma = curHp;

    iFrameTimer = iFrameDur;
    screenShakeTimer = 0.20f;
    screenShakeMag = 6.0f;
}

void EggDuelState::startEnemyAttack() {
    duelPhase = DuelPhase::BOX_TRANSITION_IN;
    boxTransitionTimer = 0.0f;
    boxTrailCount = 0;
    soulTrailCount = 0;
    curArenaSize = 0.0f;
    curArenaAngle = 3.14159265f;
    soulX = 85.0f;
    soulY = 112.0f;
    bullets.clear();
    slashSparks.clear();
    spatialCuts.clear();
    crossFlares.clear();
    slashImpactFlash = 0.0f;
    attackPatternTimer = 0.0f;
    waveTimer = 0.0f;
    waveCount = 0;
    if (curAttack >= 0 && curAttack < (int)attackList.size()) {
        attackList[curAttack]->start(*this);
        soulIsBlue = attackList[curAttack]->isSoulBlue();
    }
    enemyOffsetX = 0.0f;
    enemyOffsetY = 0.0f;
    enemyScaleX = 1.0f;
    enemyScaleY = 1.0f;
    enemyTilt = 0.0f;
    enemyGlowAlpha = 0.0f;
    enemyColor = C2D_Color32(255, 255, 255, 255);
    iFrameTimer = 0.0f;
    screenShakeTimer = 0.0f;

    initAttacks();
}

void EggDuelState::initAttacks() {
    attackList.clear();
    attackList.push_back(std::unique_ptr<EggAttack>(new AttackCrossSlash()));
    attackList.push_back(std::unique_ptr<EggAttack>(new AttackBlueSlam()));
    attackList.push_back(std::unique_ptr<EggAttack>(new AttackOrbitSpheres()));
}

bool EggDuelState::isPointInsideGrazeHeart(float worldX, float worldY, float soulCX, float soulCY, float soulAngle) {
    float dx = worldX - soulCX;
    float dy = worldY - soulCY;
    float cosA = cosf(-soulAngle);
    float sinA = sinf(-soulAngle);
    float lx = dx * cosA - dy * sinA;
    float ly = dx * sinA + dy * cosA;

    float gx = (lx * 0.5f) + 13.0f;
    float gy = (ly * 0.5f) + 13.0f;

    if (gx < 0.0f || gx >= 26.0f || gy < 0.0f || gy >= 26.0f) return false;

    int row = (int)gy;
    int col = (int)gx;
    if (row < 0 || row >= 26 || col < 0 || col >= 26) return false;

    static const uint32_t s_grazeMask[26] = {
        0x00000000, 0x00000000, 0x00000000, 0x007C3E00, 0x00FE7F00,
        0x01FFFF80, 0x01FFFF80, 0x03FFFFC0, 0x03FFFFC0, 0x03FFFFC0,
        0x03FFFFC0, 0x03FFFFC0, 0x03FFFFC0, 0x01FFFF80, 0x01FFFF80,
        0x00FFFF00, 0x007FE000, 0x007FE000, 0x003FC000, 0x001F8000,
        0x000F0000, 0x00060000, 0x00000000, 0x00000000, 0x00000000,
        0x00000000
    };

    return (s_grazeMask[row] & (1u << (25 - col))) != 0;
}

void EggDuelState::triggerGraze(float tpGain, bool isContinuous) {
    if (tpGain <= 0.0f) return;

    if (targetTP < 100.0f && (targetTP + tpGain) >= 100.0f) {
        AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.8f);
    }

    targetTP = std::min(100.0f, targetTP + tpGain);
    if (!isContinuous) {
        AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.65f);
        grazePulseTimer = 0.22f;
    } else {
        grazePulseTimer = 0.08f;
    }
}

void EggDuelState::updateTP(float dt) {
    if (grazePulseTimer > 0.0f) {
        grazePulseTimer = std::max(0.0f, grazePulseTimer - dt);
    }

    if (curTP < targetTP) {
        float step = std::max(20.0f, (targetTP - curTP) * 12.0f);
        curTP = std::min(targetTP, curTP + step * dt);
    } else if (curTP > targetTP) {
        curTP = targetTP;
    }

    if (drainDelayTimer > 0.0f) {
        drainDelayTimer = std::max(0.0f, drainDelayTimer - dt);
    } else if (drainTP > targetTP) {
        float diff = drainTP - targetTP;
        drainTP -= (diff * std::min(1.0f, dt * 10.0f)) + (dt * 25.0f);
        if (drainTP < targetTP) drainTP = targetTP;
    } else if (drainTP < targetTP) {
        drainTP = targetTP;
    }
}

void EggDuelState::drawTPBar(float x, float y, float w) {
    if (!tpLoaded || w < 50.0f) {
        C2D_DrawRectSolid(x, y, 0.42f, w, 25.0f, C2D_Color32(128, 0, 0, 255));
        if (curTP > 0.0f) {
            u32 c = (curTP >= 99.9f) ? C2D_Color32(255, 207, 34, 255) : C2D_Color32(245, 154, 66, 255);
            C2D_DrawRectSolid(x, y, 0.44f, w * (curTP / 100.0f), 25.0f, c);
        }
        return;
    }

    C2D_Flush();
    C2D_SetTintMode(C2D_TintMult);

    auto renderSlice = [this](float px, float py, float pw, u32 col, float depth) {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, col, 1.0f);

        C2D_DrawImageAt(tpLeftImg, px, py, depth, &tint, 1.0f, 1.0f);
        float midW = pw - 50.0f;
        if (midW > 0.0f) {
            C2D_DrawImageAt(tpMidImg, px + 25.0f, py, depth, &tint, midW / 12.0f, 1.0f);
        }
        C2D_DrawImageAt(tpRightImg, px + pw - 25.0f, py, depth, &tint, 1.0f, 1.0f);
    };

    const u32 COLOR_EMPTY  = C2D_Color32(128, 0, 0, 255);
    const u32 COLOR_WHITE  = C2D_Color32(255, 255, 255, 255);
    const u32 COLOR_ORANGE = C2D_Color32(245, 154, 66, 255);
    const u32 COLOR_FULL   = C2D_Color32(255, 207, 34, 255);

    u32 curFillColor = (curTP >= 99.9f) ? COLOR_FULL : COLOR_ORANGE;

    renderSlice(x, y, w, COLOR_EMPTY, 0.42f);

    float yTop = y + 3.0f;
    float yBot = y + 22.0f;
    float xTopLeft = x + 5.0f;
    float xBotLeft = x + 23.0f;
    float totalInnerW = w - 28.0f;

    float curRatio = std::max(0.0f, std::min(1.0f, curTP / 100.0f));
    float fillW = totalInnerW * curRatio;

    float borderPct = (targetTP > 0.0f && targetTP < 98.5f) ? 2.5f : 0.0f;
    float activeWhiteTP = std::max(0.0f, std::min(100.0f, std::max({targetTP + borderPct, drainTP, curTP + borderPct})));
    float whiteW = totalInnerW * (activeWhiteTP / 100.0f);

    if (whiteW > 0.5f && curTP < 99.9f) {
        float xTopRight = xTopLeft + whiteW;
        float xBotRight = xBotLeft + whiteW;
        C2D_DrawTriangle(xTopLeft, yTop, COLOR_WHITE, xTopRight, yTop, COLOR_WHITE, xBotRight, yBot, COLOR_WHITE, 0.43f);
        C2D_DrawTriangle(xTopLeft, yTop, COLOR_WHITE, xBotRight, yBot, COLOR_WHITE, xBotLeft, yBot, COLOR_WHITE, 0.43f);
    }

    if (fillW > 0.5f) {
        float xTopRight = xTopLeft + fillW;
        float xBotRight = xBotLeft + fillW;
        C2D_DrawTriangle(xTopLeft, yTop, curFillColor, xTopRight, yTop, curFillColor, xBotRight, yBot, curFillColor, 0.44f);
        C2D_DrawTriangle(xTopLeft, yTop, curFillColor, xBotRight, yBot, curFillColor, xBotLeft, yBot, curFillColor, 0.44f);
    }

    u32 colBlack = C2D_Color32(0, 0, 0, 255);
    C2D_DrawRectSolid(x, y, 0.45f, w - 22.0f, 3.0f, colBlack);
    C2D_DrawRectSolid(x + 22.0f, y + 22.0f, 0.45f, w - 22.0f, 3.0f, colBlack);
    C2D_DrawLine(x + 1.5f, y + 1.0f, colBlack, x + 23.5f, y + 23.0f, colBlack, 3.5f, 0.45f);
    C2D_DrawLine(x + w - 23.5f, y + 1.0f, colBlack, x + w - 1.5f, y + 23.0f, colBlack, 3.5f, 0.45f);

    C2D_Flush();
    C2D_SetTintMode(C2D_TintMult);
}

void EggDuelState::updateAttackPatterns(float dt) {
    enemyOffsetX += (0.0f - enemyOffsetX) * std::min(1.0f, dt * 8.0f);
    enemyOffsetY += (0.0f - enemyOffsetY) * std::min(1.0f, dt * 8.0f);
    enemyScaleX += (1.0f - enemyScaleX) * std::min(1.0f, dt * 8.0f);
    enemyScaleY += (1.0f - enemyScaleY) * std::min(1.0f, dt * 8.0f);
    enemyTilt += (0.0f - enemyTilt) * std::min(1.0f, dt * 8.0f);
    enemyGlowAlpha += (0.0f - enemyGlowAlpha) * std::min(1.0f, dt * 6.0f);
    enemyColor = C2D_Color32(255, 255, 255, 255);

    if (curAttack >= 0 && curAttack < (int)attackList.size()) {
        attackList[curAttack]->update(*this, dt);
    }
}

void EggDuelState::updateBullets(float dt) {
    float soulCX = soulX + 8.0f;
    float soulCY = soulY + 8.0f;
    const float soulRadius = 4.0f;

    for (size_t i = 0; i < bullets.size(); i++) {
        auto& b = bullets[i];
        if (!b.active) continue;

        if (b.type == BulletType::THRUST_SWORD) {
            if (!b.wallHit && curHp > 0) {
                float backDirX = -cosf(b.angle), backDirY = -sinf(b.angle);
                spawnSlashSparks(b.x, b.y, backDirX, backDirY, 1, 80.0f, 200.0f);
                if ((rand() % 3) == 0) {
                    float side = ((rand() % 2) == 0) ? 1.0f : -1.0f;
                    spawnSlashSparks(b.x, b.y, -backDirY * side, backDirX * side, 1, 60.0f, 150.0f);
                }
            }

            float distFromStart = sqrtf((b.x - b.startX) * (b.x - b.startX) + (b.y - b.startY) * (b.y - b.startY));
            if (!b.wallHit && distFromStart >= 170.0f) {
                b.wallHit = true;
                spawnSlashSparks(b.x, b.y, -cosf(b.angle), -sinf(b.angle), 14, 160.0f, 380.0f);
                slashImpactFlash = 0.05f;
                screenShakeTimer = 0.12f;
                screenShakeMag = 4.5f;
                spawnSpatialCut(b.startX, b.startY, b.x, b.y, 0.24f, 10.5f);
            }

            for (int t = MAX_BULLET_TRAIL - 1; t > 0; t--) {
                b.trail[t] = b.trail[t - 1];
            }
            b.trail[0] = { b.x, b.y };
            if (b.trailCount < MAX_BULLET_TRAIL) b.trailCount++;
        }

        b.timer += dt;
        b.x += b.vx * dt;
        b.y += b.vy * dt;

        if (b.timer > b.maxLifetime || b.x < -60.0f || b.x > 460.0f || b.y < -60.0f || b.y > 300.0f) {
            b.active = false;
            continue;
        }

        if (b.type == BulletType::THRUST_SWORD && curHp > 0) {
            float halfLen = b.length * 0.5f;
            float cosA = cosf(b.angle), sinA = sinf(b.angle);
            float p1x = b.x - halfLen * cosA, p1y = b.y - halfLen * sinA;
            float p2x = b.x + halfLen * cosA, p2y = b.y + halfLen * sinA;

            float segDx = p2x - p1x, segDy = p2y - p1y;
            float segLenSq = segDx * segDx + segDy * segDy;
            float t = (segLenSq > 0.001f) ? std::max(0.0f, std::min(1.0f, ((soulCX - p1x) * segDx + (soulCY - p1y) * segDy) / segLenSq)) : 0.0f;

            float closestX = p1x + t * segDx;
            float closestY = p1y + t * segDy;
            float distSq = (soulCX - closestX) * (soulCX - closestX) + (soulCY - closestY) * (soulCY - closestY);
            float maxDist = soulRadius + b.width * 0.5f;

            if (!b.isFake && iFrameTimer <= 0.0f && distSq <= (maxDist * maxDist)) {
                applyDamage(48, 22, 0.45f);
                b.active = false;
            } else if (!b.grazed) {
                bool grazed = isPointInsideGrazeHeart(b.x, b.y, soulCX, soulCY, curSoulAngle);
                if (!grazed) {
                    grazed = isPointInsideGrazeHeart(b.x - cosf(b.angle) * 8.0f, b.y - sinf(b.angle) * 8.0f, soulCX, soulCY, curSoulAngle);
                }
                if (grazed) {
                    triggerGraze(3.5f, false);
                    b.grazed = true;
                }
            }
        }
    }

    bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) {
        return !b.active;
    }), bullets.end());
}

void EggDuelState::drawBullets() {
    for (const auto& b : bullets) {
        if (!b.active) continue;

        if (b.type == BulletType::THRUST_SWORD) {
            float flightProgress = std::min(1.0f, b.timer / 0.35f);
            float alphaRatio = 1.0f - (flightProgress * 0.6f);
            if (!b.wallHit) {
                drawTravelingSlashWake(b.startX, b.startY, b.x, b.y, 10.0f, alphaRatio);
            }

            if (swordSheet && swordImg.tex) {
                float rot = b.angle - 1.570796f;
                float moveScaleX = 1.6f, moveScaleY = 2.7f;

                for (int t = 0; t < b.trailCount; t++) {
                    float tRatio = (float)(t + 1) / (float)(MAX_BULLET_TRAIL + 1);
                    float alpha = (1.0f - tRatio) * 0.65f;
                    if (alpha > 0.03f) {
                        C2D_ImageTint tTint;
                        C2D_PlainImageTint(&tTint, C2D_Color32(255, 50, 50, (u8)(alpha * 255.0f)), 0.85f);
                        C2D_DrawImageAtRotated(swordImg, b.trail[t].x, b.trail[t].y, 0.44f, rot, &tTint, moveScaleX * (1.0f - tRatio * 0.25f), moveScaleY);
                    }
                }

                C2D_ImageTint bTint;
                C2D_ImageTint* tintPtr = nullptr;
                if (b.isFake) {
                    C2D_PlainImageTint(&bTint, C2D_Color32(255, 220, 220, 210), 0.7f);
                    tintPtr = &bTint;
                }
                C2D_DrawImageAtRotated(swordImg, b.x, b.y, 0.46f, rot, tintPtr, moveScaleX, moveScaleY);
            }
        }
    }
}

void EggDuelState::init() {
    MusicBeatState::transPhase = TransitionPhase::NONE;
    MusicBeatState::skipTransition = true;
    fadeTimer = 0.0f;
    curHp = 160;
    maxHp = 160;
    curKarma = 0;
    karmaDrainTimer = 0.0f;
    curAction = 0;
    curAttack = 0;
    animTimer = 0.0f;
    scrollDownTimer = 0.0f;
    scrollUpTimer = 0.0f;

    duelPhase = DuelPhase::ACTION_SELECT;
    boxTransitionTimer = 0.0f;
    curArenaSize = 0.0f;
    curArenaAngle = 0.0f;
    boxTrailCount = 0;
    soulTrailCount = 0;
    soulX = 85.0f;
    soulY = 112.0f;
    attackTimer = 0.0f;

    curTP = 0.0f;
    targetTP = 0.0f;
    drainTP = 0.0f;
    drainDelayTimer = 0.0f;
    grazePulseTimer = 0.0f;

    bullets.clear();
    slashSparks.clear();
    spatialCuts.clear();
    crossFlares.clear();
    slashImpactFlash = 0.0f;
    attackPatternTimer = 0.0f;
    enemyOffsetX = 0.0f;
    enemyOffsetY = 0.0f;
    enemyScaleX = 1.0f;
    enemyScaleY = 1.0f;
    enemyTilt = 0.0f;
    enemyGlowAlpha = 0.0f;
    enemyColor = C2D_Color32(255, 255, 255, 255);
    iFrameTimer = 0.0f;
    screenShakeTimer = 0.0f;
    screenShakeMag = 0.0f;

    initAudio();

    soulSheet = C2D_SpriteSheetLoad("romfs:/preload/images/theend/soul.t3x");
    if (soulSheet) {
        soulImg = C2D_SpriteSheetGetImage(soulSheet, 0);
        if (soulImg.tex) C3D_TexSetFilter(soulImg.tex, GPU_NEAREST, GPU_NEAREST);
    }

    swordSheet = C2D_SpriteSheetLoad("romfs:/preload/images/theend/swordkey.t3x");
    if (!swordSheet) swordSheet = C2D_SpriteSheetLoad("assets/preload/images/theend/swordkey.t3x");
    if (swordSheet) {
        swordImg = C2D_SpriteSheetGetImage(swordSheet, 0);
        if (swordImg.tex) C3D_TexSetFilter(swordImg.tex, GPU_NEAREST, GPU_NEAREST);
        if (swordImg.subtex && swordImg.subtex->height > 4) {
            swordBladeSubtex = *swordImg.subtex;
            float vPix = (swordImg.subtex->bottom - swordImg.subtex->top) / (float)swordImg.subtex->height;
            swordBladeSubtex.top += vPix * 4.0f;
            swordBladeSubtex.height -= 4;
            swordBladeImg.tex = swordImg.tex;
            swordBladeImg.subtex = &swordBladeSubtex;
        } else {
            swordBladeImg = swordImg;
        }
    }

    spikeSheet = C2D_SpriteSheetLoad("romfs:/preload/images/theend/spike.t3x");
    if (spikeSheet) {
        spikeImg = C2D_SpriteSheetGetImage(spikeSheet, 0);
        if (spikeImg.tex) C3D_TexSetFilter(spikeImg.tex, GPU_NEAREST, GPU_NEAREST);
    }

    MusicPlayer::play("romfs:/preload/music/fight.adp", 0.75f);

    optionsSheet = C2D_SpriteSheetLoad("romfs:/preload/images/theend/options.t3x");
    if (optionsSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(optionsSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_NEAREST, GPU_NEAREST);

        int atlasW = 24, atlasH = 20;
        SparrowParser::parseXml("romfs:/preload/images/theend/options.xml", optionFrames, &atlasW, &atlasH);

        float rw = mainImg.subtex->right - mainImg.subtex->left;
        float rh = mainImg.subtex->bottom - mainImg.subtex->top;

        for (auto& f : optionFrames) {
            f.tex = mainImg.tex;
            f.uv.width = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left = mainImg.subtex->left + ((float)f.x * rw / (float)atlasW);
            f.uv.top = mainImg.subtex->top + ((float)f.y * rh / (float)atlasH);
            f.uv.right = mainImg.subtex->left + ((float)(f.x + f.w) * rw / (float)atlasW);
            f.uv.bottom = mainImg.subtex->top + ((float)(f.y + f.h) * rh / (float)atlasH);

            if (f.name == "attack") frameAttack = &f;
            else if (f.name == "act") frameAct = &f;
            else if (f.name == "item") frameItem = &f;
            else if (f.name == "defense") frameDefense = &f;
        }
    }

    skyboxSheet = C2D_SpriteSheetLoad("romfs:/preload/images/theend/skybox.t3x");
    if (!skyboxSheet) skyboxSheet = C2D_SpriteSheetLoad("assets/preload/images/theend/skybox.t3x");
    if (skyboxSheet) {
        skyboxImg = C2D_SpriteSheetGetImage(skyboxSheet, 0);
        if (skyboxImg.tex) {
            C3D_TexSetFilter(skyboxImg.tex, GPU_LINEAR, GPU_LINEAR);
            C3D_TexSetWrap(skyboxImg.tex, GPU_REPEAT, GPU_REPEAT);
        }
    }

    tpSheet = C2D_SpriteSheetLoad("romfs:/preload/images/theend/tp.t3x");
    if (!tpSheet) tpSheet = C2D_SpriteSheetLoad("assets/preload/images/theend/tp.t3x");
    if (tpSheet) {
        tpImg = C2D_SpriteSheetGetImage(tpSheet, 0);
        if (tpImg.tex) C3D_TexSetFilter(tpImg.tex, GPU_NEAREST, GPU_NEAREST);
        if (tpImg.subtex) {
            float uPix = (tpImg.subtex->right - tpImg.subtex->left) / (float)tpImg.subtex->width;

            tpLeftSubtex = *tpImg.subtex;
            tpLeftSubtex.width = 25;
            tpLeftSubtex.right = tpImg.subtex->left + 25.0f * uPix;

            tpMidSubtex = *tpImg.subtex;
            tpMidSubtex.width = 12;
            tpMidSubtex.left = tpImg.subtex->left + 25.0f * uPix;
            tpMidSubtex.right = tpImg.subtex->left + 37.0f * uPix;

            tpRightSubtex = *tpImg.subtex;
            tpRightSubtex.width = 25;
            tpRightSubtex.left = tpImg.subtex->left + 37.0f * uPix;
            tpRightSubtex.right = tpImg.subtex->right;

            tpLeftImg.tex = tpImg.tex;
            tpLeftImg.subtex = &tpLeftSubtex;
            tpMidImg.tex = tpImg.tex;
            tpMidImg.subtex = &tpMidSubtex;
            tpRightImg.tex = tpImg.tex;
            tpRightImg.subtex = &tpRightSubtex;
            tpLoaded = true;
        }
    }

    grazeSheet = C2D_SpriteSheetLoad("romfs:/preload/images/theend/graze.t3x");
    if (!grazeSheet) grazeSheet = C2D_SpriteSheetLoad("assets/preload/images/theend/graze.t3x");
    if (grazeSheet) {
        grazeImg = C2D_SpriteSheetGetImage(grazeSheet, 0);
        if (grazeImg.tex) C3D_TexSetFilter(grazeImg.tex, GPU_NEAREST, GPU_NEAREST);
    }

    textBuf = C2D_TextBufNew(1024);
}

void EggDuelState::update(float dt) {
    MusicPlayer::update();

    if (fadeTimer < FADE_DURATION) {
        fadeTimer = std::min(FADE_DURATION, fadeTimer + dt);
    }

    animTimer += dt;
    scrollDownTimer += dt * 32.0f;
    scrollUpTimer += dt * 32.0f;

    updateTP(dt);
    updateSlashEffects(dt);

    if (screenShakeTimer > 0.0f) {
        screenShakeTimer = std::max(0.0f, screenShakeTimer - dt);
        if (screenShakeTimer <= 0.0f) screenShakeMag = 0.0f;
    }

    if (duelPhase != DuelPhase::ENEMY_ATTACK || !soulIsBlue) {
        targetSoulAngle = 0.0f;
    }
    float angleDiff = targetSoulAngle - curSoulAngle;
    while (angleDiff > 3.14159265f) angleDiff -= 6.2831853f;
    while (angleDiff < -3.14159265f) angleDiff += 6.2831853f;
    curSoulAngle += angleDiff * std::min(1.0f, dt * 45.0f);

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    touchPosition touch;
    hidTouchRead(&touch);

    // Debug TP adjust
    if (kDown & KEY_Y) {
        drainTP = std::max(drainTP, targetTP);
        targetTP = std::max(0.0f, targetTP - 20.0f);
        curTP = targetTP;
        drainDelayTimer = 0.40f;
    }
    if (kDown & KEY_X) {
        triggerGraze(20.0f, false);
    }

    switch (duelPhase) {
        case DuelPhase::ACTION_SELECT: {
            int oldAction = curAction;

            if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
                curAction = (curAction + ACTION_COUNT - 1) % ACTION_COUNT;
            } else if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
                curAction = (curAction + 1) % ACTION_COUNT;
            }

            if (curAction == 4) {
                if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
                    curAttack = (curAttack + 1) % TOTAL_ATTACKS;
                    playCursorSound();
                } else if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
                    curAttack = (curAttack + TOTAL_ATTACKS - 1) % TOTAL_ATTACKS;
                    playCursorSound();
                }
            }

            if (kDown & KEY_TOUCH) {
                const float startX = 55.0f, boxW = 34.0f, gap = 10.0f, boxY = 50.0f, boxH = 42.0f;
                for (int i = 0; i < ACTION_COUNT; i++) {
                    float bx = startX + i * (boxW + gap);
                    if (touch.px >= (bx - 4.0f) && touch.px <= (bx + boxW + 4.0f) &&
                        touch.py >= (boxY - 4.0f) && touch.py <= (boxY + boxH + 4.0f)) {
                        curAction = i;
                        break;
                    }
                }
            }

            if (curAction != oldAction) playCursorSound();

            if ((kDown & (KEY_A | KEY_START)) && curAction == 3) {
                if (curHp <= 0) curHp = maxHp;
                playCursorSound();
                triggerGraze(16.0f, false);
                isDefending = true;
                startEnemyAttack();
            }

            if ((kDown & (KEY_A | KEY_START)) && curAction == 4) {
                if (curHp <= 0) curHp = maxHp;
                playCursorSound();
                isDefending = false;
                startEnemyAttack();
            }
            break;
        }

        case DuelPhase::BOX_TRANSITION_IN: {
            boxTransitionTimer += dt;
            float progress = std::min(1.0f, boxTransitionTimer / BOX_TRANS_DURATION);
            float ease = 1.0f - powf(1.0f - progress, 3.0f);

            float targetSize = (curAttack == 2) ? ARENA_SIZE_ATTACK2 : ARENA_SIZE;
            curArenaSize = targetSize * ease;
            curArenaAngle = (1.0f - ease) * 3.14159265f;

            soulX = 85.0f + (192.0f - 85.0f) * ease;
            soulY = 112.0f;

            for (int i = MAX_BOX_TRAILS - 1; i > 0; i--) boxTrails[i] = boxTrails[i - 1];
            boxTrails[0] = { curArenaSize, curArenaAngle };
            if (boxTrailCount < MAX_BOX_TRAILS) boxTrailCount++;

            for (int i = MAX_SOUL_TRAILS - 1; i > 0; i--) soulTrails[i] = soulTrails[i - 1];
            soulTrails[0] = { soulX, soulY };
            if (soulTrailCount < MAX_SOUL_TRAILS) soulTrailCount++;

            if (progress >= 1.0f) {
                curArenaSize = targetSize;
                curArenaAngle = 0.0f;
                soulX = 192.0f;
                soulY = 112.0f;

                duelPhase = DuelPhase::ENEMY_ATTACK;
                attackTimer = 30.0f;
                attackPatternTimer = 0.0f;
                bullets.clear();

                if (curAttack >= 0 && curAttack < (int)attackList.size()) {
                    attackList[curAttack]->start(*this);
                    soulIsBlue = attackList[curAttack]->isSoulBlue();
                }
            }
            break;
        }

        case DuelPhase::ENEMY_ATTACK: {
            if (kDown & KEY_SELECT) attackTimer = 0.0f;
            if ((kDown & KEY_A) && curHp <= 0) {
                curHp = maxHp;
                curKarma = 0;
                karmaDrainTimer = 0.0f;
                playCursorSound();
            }

            if (curKarma > 0) {
                karmaDrainTimer += dt;
                float drainInterval = (curKarma >= 30) ? 0.035f : (curKarma >= 15 ? 0.065f : 0.11f);
                if (karmaDrainTimer >= drainInterval) {
                    karmaDrainTimer = 0.0f;
                    curKarma--;
                    curHp--;
                    if (curHp < 1) curHp = 1;
                }
            }

            if (iFrameTimer > 0.0f) {
                iFrameTimer = std::max(0.0f, iFrameTimer - dt);
            }

            circlePosition circle;
            hidCircleRead(&circle);

            float moveX = 0.0f, moveY = 0.0f;
            if (kHeld & (KEY_DLEFT | KEY_CPAD_LEFT)) moveX -= 1.0f;
            if (kHeld & (KEY_DRIGHT | KEY_CPAD_RIGHT)) moveX += 1.0f;
            if (kHeld & (KEY_DUP | KEY_CPAD_UP)) moveY -= 1.0f;
            if (kHeld & (KEY_DDOWN | KEY_CPAD_DOWN)) moveY += 1.0f;

            if (std::abs(circle.dx) > 35) moveX = (float)circle.dx / 154.0f;
            if (std::abs(circle.dy) > 35) moveY = -(float)circle.dy / 154.0f;

            float len = sqrtf(moveX * moveX + moveY * moveY);
            if (len > 1.0f) {
                moveX /= len;
                moveY /= len;
            }

            float soulSpeed = 125.0f;
            if (kHeld & (KEY_B | KEY_X | KEY_L | KEY_R)) soulSpeed = 62.5f;

            const float wallPadding = 2.0f;
            const float soulSize = 16.0f;
            float curBoxSize = (curAttack == 2) ? ARENA_SIZE_ATTACK2 : ARENA_SIZE;
            float curBoxX = ARENA_CENTER_X - (curBoxSize * 0.5f);
            float curBoxY = ARENA_CENTER_Y - (curBoxSize * 0.5f);
            const float minX = curBoxX + wallPadding;
            const float maxX = curBoxX + curBoxSize - wallPadding - soulSize;
            const float minY = curBoxY + wallPadding;
            const float maxY = curBoxY + curBoxSize - wallPadding - soulSize;

            if (!soulIsBlue) {
                soulX = std::max(minX, std::min(maxX, soulX + moveX * soulSpeed * dt));
                soulY = std::max(minY, std::min(maxY, soulY + moveY * soulSpeed * dt));
            } else {
                float lateralSpeed = (kHeld & (KEY_B | KEY_X | KEY_L | KEY_R)) ? 75.0f : 150.0f;

                if (soulFlinging) {
                    soulX += soulVelX * dt;
                    soulY += soulVelY * dt;
                } else {
                    bool isVert = (soulGravity == SoulGravity::DOWN || soulGravity == SoulGravity::UP);
                    float latMove = 0.0f;
                    if (isVert) {
                        if (kHeld & (KEY_DLEFT | KEY_CPAD_LEFT)) latMove -= 1.0f;
                        if (kHeld & (KEY_DRIGHT | KEY_CPAD_RIGHT)) latMove += 1.0f;
                        if (latMove == 0.0f && std::abs(circle.dx) > 35) latMove = (circle.dx > 0) ? 1.0f : -1.0f;
                        soulX += latMove * lateralSpeed * dt;
                    } else {
                        if (kHeld & (KEY_DUP | KEY_CPAD_UP)) latMove -= 1.0f;
                        if (kHeld & (KEY_DDOWN | KEY_CPAD_DOWN)) latMove += 1.0f;
                        if (latMove == 0.0f && std::abs(circle.dy) > 35) latMove = (circle.dy > 0) ? -1.0f : 1.0f;
                        soulY += latMove * lateralSpeed * dt;
                    }

                    bool jumpHeld = false;
                    if (soulGravity == SoulGravity::DOWN) jumpHeld = (kHeld & (KEY_DUP | KEY_CPAD_UP)) || (circle.dy > 35);
                    else if (soulGravity == SoulGravity::UP) jumpHeld = (kHeld & (KEY_DDOWN | KEY_CPAD_DOWN)) || (circle.dy < -35);
                    else if (soulGravity == SoulGravity::LEFT) jumpHeld = (kHeld & (KEY_DRIGHT | KEY_CPAD_RIGHT)) || (circle.dx > 35);
                    else if (soulGravity == SoulGravity::RIGHT) jumpHeld = (kHeld & (KEY_DLEFT | KEY_CPAD_LEFT)) || (circle.dx < -35);

                    float& vel = isVert ? soulVelY : soulVelX;
                    float gravDir = (soulGravity == SoulGravity::DOWN || soulGravity == SoulGravity::RIGHT) ? 1.0f : -1.0f;

                    if (soulOnGround && jumpHeld) {
                        vel = -180.0f * gravDir;
                        soulOnGround = false;
                    }

                    if (!jumpHeld && (vel * gravDir) < -30.0f) {
                        vel = -30.0f * gravDir;
                    }

                    float currentSpeedAlongGrav = vel * gravDir;
                    float grav = 540.0f;
                    if (currentSpeedAlongGrav <= -120.0f) grav = 180.0f;
                    else if (currentSpeedAlongGrav <= -30.0f) grav = 450.0f;
                    else if (currentSpeedAlongGrav <= 15.0f)  grav = 180.0f;

                    vel += grav * gravDir * dt;
                    if (currentSpeedAlongGrav > 240.0f) vel = 240.0f * gravDir;

                    if (isVert) soulY += soulVelY * dt;
                    else soulX += soulVelX * dt;
                }

                soulOnGround = false;
                if (soulX <= minX) {
                    soulX = minX;
                    if (soulGravity == SoulGravity::LEFT) soulOnGround = true;
                    if (soulVelX < 0.0f) soulVelX = 0.0f;
                }
                if (soulX >= maxX) {
                    soulX = maxX;
                    if (soulGravity == SoulGravity::RIGHT) soulOnGround = true;
                    if (soulVelX > 0.0f) soulVelX = 0.0f;
                }
                if (soulY <= minY) {
                    soulY = minY;
                    if (soulGravity == SoulGravity::UP) soulOnGround = true;
                    if (soulVelY < 0.0f) soulVelY = 0.0f;
                }
                if (soulY >= maxY) {
                    soulY = maxY;
                    if (soulGravity == SoulGravity::DOWN) soulOnGround = true;
                    if (soulVelY > 0.0f) soulVelY = 0.0f;
                }

                for (int i = MAX_SOUL_TRAILS - 1; i > 0; i--) soulTrails[i] = soulTrails[i - 1];
                soulTrails[0] = { soulX, soulY };
                if (soulTrailCount < MAX_SOUL_TRAILS) soulTrailCount++;
            }

            attackPatternTimer += dt;
            updateAttackPatterns(dt);
            updateBullets(dt);

            attackTimer -= dt;
            if (attackTimer <= 0.0f) {
                attackTimer = 0.0f;
                duelPhase = DuelPhase::BOX_TRANSITION_OUT;
                screenShakeTimer = 0.0f;
                screenShakeMag = 0.0f;
                boxTransitionTimer = 0.0f;
                boxTrailCount = 0;
                soulTrailCount = 0;
                soulExitStartX = soulX;
                soulExitStartY = soulY;
            }
            break;
        }

        case DuelPhase::BOX_TRANSITION_OUT: {
            isDefending = false;
            boxTransitionTimer += dt;
            float progress = std::min(1.0f, boxTransitionTimer / BOX_TRANS_DURATION);
            float ease = progress * progress * (3.0f - 2.0f * progress);
            float reverseEase = 1.0f - ease;

            float targetSize = (curAttack == 2) ? ARENA_SIZE_ATTACK2 : ARENA_SIZE;
            curArenaSize = targetSize * reverseEase;
            curArenaAngle = ease * 3.14159265f;

            soulX = soulExitStartX + (85.0f - soulExitStartX) * ease;
            soulY = soulExitStartY + (112.0f - soulExitStartY) * ease;

            for (int i = MAX_BOX_TRAILS - 1; i > 0; i--) boxTrails[i] = boxTrails[i - 1];
            boxTrails[0] = { curArenaSize, curArenaAngle };
            if (boxTrailCount < MAX_BOX_TRAILS) boxTrailCount++;

            for (int i = MAX_SOUL_TRAILS - 1; i > 0; i--) soulTrails[i] = soulTrails[i - 1];
            soulTrails[0] = { soulX, soulY };
            if (soulTrailCount < MAX_SOUL_TRAILS) soulTrailCount++;

            updateBullets(dt);

            if (progress >= 1.0f) {
                curArenaSize = 0.0f;
                curArenaAngle = 0.0f;
                boxTrailCount = 0;
                soulTrailCount = 0;
                bullets.clear();
                if (curAttack >= 0 && curAttack < (int)attackList.size() && attackList[curAttack]) {
                    attackList[curAttack]->stop(*this);
                }
                duelPhase = DuelPhase::ACTION_SELECT;
            }
            break;
        }
    }

    if (kDown & KEY_START) {
        MusicBeatState::skipTransition = true;
        switchState(new TitleState());
    }
}

void EggDuelState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_Font fontToUse = globalPixelFont ? globalPixelFont : globalVCRFont;

    C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(top);

    float shakeX = 0.0f, shakeY = 0.0f;
    if (screenShakeTimer > 0.0f) {
        shakeX = ((float)(rand() % 100) / 50.0f - 1.0f) * screenShakeMag;
        shakeY = ((float)(rand() % 100) / 50.0f - 1.0f) * screenShakeMag;
    }

    // Skybox render
    if (skyboxSheet && skyboxImg.tex) {
        float origLeft = 0.0f, origTop = 1.0f, origRight = 1.0f, origBottom = 0.0f;
        if (skyboxImg.subtex) {
            origLeft = skyboxImg.subtex->left;
            origTop = skyboxImg.subtex->top;
            origRight = skyboxImg.subtex->right;
            origBottom = skyboxImg.subtex->bottom;
        }

        float uvW = origRight - origLeft;
        float uvH = origBottom - origTop;

        const float cellW = 400.0f / (float)SKYBOX_COLS;
        const float cellH = 240.0f / (float)SKYBOX_ROWS;
        const float focalLength = 0.65f;
        const float scrollU = fmodf(animTimer * 0.065f, 1.0f);

        for (int r = 0; r < SKYBOX_ROWS; r++) {
            float y0 = (float)r * cellH;
            float y1 = (float)(r + 1) * cellH;
            float normY0 = ((y0 - shakeY * 0.25f) - 120.0f) / 120.0f;
            float normY1 = ((y1 - shakeY * 0.25f) - 120.0f) / 120.0f;

            for (int c = 0; c < SKYBOX_COLS; c++) {
                int idx = r * SKYBOX_COLS + c;
                float x0 = (float)c * cellW;
                float x1 = (float)(c + 1) * cellW;
                float normX0 = ((x0 - shakeX * 0.25f) - 200.0f) / 200.0f;
                float normX1 = ((x1 - shakeX * 0.25f) - 200.0f) / 200.0f;

                float vy0 = normY0 * 0.60f;
                float dist0 = sqrtf(normX0 * normX0 + vy0 * vy0 + focalLength * focalLength);
                float phi0 = atan2f(normX0, focalLength);
                float theta0 = asinf(std::max(-0.99f, std::min(0.99f, vy0 / dist0)));

                float vy1 = normY1 * 0.60f;
                float dist1 = sqrtf(normX1 * normX1 + vy1 * vy1 + focalLength * focalLength);
                float phi1 = atan2f(normX1, focalLength);
                float theta1 = asinf(std::max(-0.99f, std::min(0.99f, vy1 / dist1)));

                float u0 = (phi0 / (2.0f * 3.14159265f)) + scrollU;
                float u1 = (phi1 / (2.0f * 3.14159265f)) + scrollU;
                float v0 = 0.5f + (theta0 / 3.14159265f) * 1.45f;
                float v1 = 0.5f + (theta1 / 3.14159265f) * 1.45f;

                float waveU = sinf(normY0 * 3.5f + animTimer * 0.70f) * 0.022f +
                              sinf(normY0 * 1.8f - animTimer * 0.35f) * 0.012f;
                float waveV = cosf(normX0 * 3.0f + animTimer * 0.55f) * 0.015f;

                u0 += waveU;
                u1 += waveU;
                v0 = std::max(0.02f, std::min(0.98f, v0 + waveV));
                v1 = std::max(0.02f, std::min(0.98f, v1 + waveV));

                skyboxSubtexs[idx].left = origLeft + u0 * uvW;
                skyboxSubtexs[idx].right = origLeft + u1 * uvW;
                skyboxSubtexs[idx].top = origTop + v0 * uvH;
                skyboxSubtexs[idx].bottom = origTop + v1 * uvH;
                skyboxSubtexs[idx].width = (u16)cellW;
                skyboxSubtexs[idx].height = (u16)cellH;

                C2D_Image cellImg;
                cellImg.tex = skyboxImg.tex;
                cellImg.subtex = &skyboxSubtexs[idx];
                C2D_DrawImageAt(cellImg, x0, y0, 0.10f, nullptr, 1.0f, 1.0f);
            }
        }
    }

    const float cubeSize = 36.0f;
    const float playerX = 75.0f + shakeX;
    const float playerY = (240.0f - cubeSize) * 0.5f + shakeY;
    const float baseEnemyX = 400.0f - 75.0f - cubeSize;
    const float baseEnemyY = (240.0f - cubeSize) * 0.5f;

    C2D_DrawRectSolid(playerX, playerY, 0.20f, cubeSize, cubeSize, C2D_Color32(255, 255, 255, 255));

    float finalEnemyX = baseEnemyX + enemyOffsetX + shakeX;
    float finalEnemyY = baseEnemyY + enemyOffsetY + shakeY;
    float enemyCenterWorldX = finalEnemyX + (cubeSize * 0.5f);
    float enemyCenterWorldY = finalEnemyY + (cubeSize * 0.5f);

    float baseHalf = cubeSize * 0.5f;
    float halfW = baseHalf * enemyScaleX;
    float halfH = baseHalf * enemyScaleY;
    float cosT = cosf(enemyTilt);
    float sinT = sinf(enemyTilt);

    float ex0 = enemyCenterWorldX + (-halfW) * cosT - (-halfH) * sinT;
    float ey0 = enemyCenterWorldY + (-halfW) * sinT + (-halfH) * cosT;
    float ex1 = enemyCenterWorldX + (halfW) * cosT - (-halfH) * sinT;
    float ey1 = enemyCenterWorldY + (halfW) * sinT + (-halfH) * cosT;
    float ex2 = enemyCenterWorldX + (halfW) * cosT - (halfH) * sinT;
    float ey2 = enemyCenterWorldY + (halfW) * sinT + (halfH) * cosT;
    float ex3 = enemyCenterWorldX + (-halfW) * cosT - (halfH) * sinT;
    float ey3 = enemyCenterWorldY + (-halfW) * sinT + (halfH) * cosT;

    C2D_DrawTriangle(ex0, ey0, enemyColor, ex1, ey1, enemyColor, ex2, ey2, enemyColor, 0.20f);
    C2D_DrawTriangle(ex0, ey0, enemyColor, ex2, ey2, enemyColor, ex3, ey3, enemyColor, 0.20f);

    if (enemyGlowAlpha > 0.01f) {
        u8 ga = (u8)(enemyGlowAlpha * 255.0f);
        u32 gCol = C2D_Color32(255, 255, 255, ga);
        float glowH = halfW + 4.0f;
        float gx0 = enemyCenterWorldX + (-glowH) * cosT - (-glowH) * sinT;
        float gy0 = enemyCenterWorldY + (-glowH) * sinT + (-glowH) * cosT;
        float gx1 = enemyCenterWorldX + (glowH) * cosT - (-glowH) * sinT;
        float gy1 = enemyCenterWorldY + (glowH) * sinT + (-glowH) * cosT;
        float gx2 = enemyCenterWorldX + (glowH) * cosT - (glowH) * sinT;
        float gy2 = enemyCenterWorldY + (glowH) * sinT + (glowH) * cosT;
        float gx3 = enemyCenterWorldX + (-glowH) * cosT - (glowH) * sinT;
        float gy3 = enemyCenterWorldY + (-glowH) * sinT + (glowH) * cosT;

        C2D_DrawLine(gx0, gy0, gCol, gx1, gy1, gCol, 2.0f, 0.21f);
        C2D_DrawLine(gx1, gy1, gCol, gx2, gy2, gCol, 2.0f, 0.21f);
        C2D_DrawLine(gx2, gy2, gCol, gx3, gy3, gCol, 2.0f, 0.21f);
        C2D_DrawLine(gx3, gy3, gCol, gx0, gy0, gCol, 2.0f, 0.21f);
    }

    if (duelPhase == DuelPhase::BOX_TRANSITION_IN || duelPhase == DuelPhase::BOX_TRANSITION_OUT) {
        for (int i = boxTrailCount - 1; i >= 1; i--) {
            float ratio = (float)(boxTrailCount - i) / (float)boxTrailCount;
            u8 a = (u8)(ratio * 80.0f);
            if (a > 5) {
                u32 trailBorder = C2D_Color32(255, 0, 0, a);
                drawRotatedBox(ARENA_CENTER_X + shakeX, ARENA_CENTER_Y + shakeY, boxTrails[i].size, boxTrails[i].angle, trailBorder, 0x00000000, 0.38f, 2.0f);
            }
        }
    }

    if (curArenaSize > 1.0f) {
        drawRotatedBox(ARENA_CENTER_X + shakeX, ARENA_CENTER_Y + shakeY, curArenaSize, curArenaAngle,
                       C2D_Color32(235, 55, 55, 255), C2D_Color32(0, 0, 0, 255), 0.40f, 4.0f);
    }

    if (curAttack >= 0 && curAttack < (int)attackList.size()) {
        attackList[curAttack]->draw(*this, shakeX, shakeY);
    }

    drawBullets();
    drawSlashEffects();

    if (soulSheet && soulImg.tex) {
        float soulDrawCX = std::round(soulX + shakeX + 8.0f);
        float soulDrawCY = std::round(soulY + shakeY + 8.0f);

        if (soulIsBlue && (soulFlinging || !soulOnGround)) {
            C2D_Flush();
            C2D_SetTintMode(C2D_TintSolid);
            for (int i = soulTrailCount - 1; i >= 1; i--) {
                float trailRatio = (float)(soulTrailCount - i) / (float)soulTrailCount;
                u8 sa = (u8)(trailRatio * 140.0f);
                if (sa > 10) {
                    C2D_ImageTint tTint;
                    C2D_PlainImageTint(&tTint, C2D_Color32(0, 70, 255, sa), 1.0f);
                    C2D_DrawImageAtRotated(soulImg, std::round(soulTrails[i].x + shakeX + 8.0f),
                                           std::round(soulTrails[i].y + shakeY + 8.0f), 0.58f, curSoulAngle, &tTint, 2.0f, 2.0f);
                }
            }
            C2D_Flush();
            C2D_SetTintMode(C2D_TintMult);
        }

        if (soulIsBlue) {
            C2D_Flush();
            C2D_SetTintMode(C2D_TintSolid);
            if (iFrameTimer > 0.0f) {
                if (((int)(iFrameTimer * 26.0f) % 2) == 0) {
                    C2D_ImageTint bTint;
                    C2D_PlainImageTint(&bTint, C2D_Color32(0, 70, 255, 130), 1.0f);
                    C2D_DrawImageAtRotated(soulImg, soulDrawCX, soulDrawCY, 0.60f, curSoulAngle, &bTint, 2.0f, 2.0f);
                }
            } else {
                C2D_ImageTint bTint;
                C2D_PlainImageTint(&bTint, C2D_Color32(0, 70, 255, 255), 1.0f);
                C2D_DrawImageAtRotated(soulImg, soulDrawCX, soulDrawCY, 0.60f, curSoulAngle, &bTint, 2.0f, 2.0f);
            }
            C2D_Flush();
            C2D_SetTintMode(C2D_TintMult);
        } else {
            if (iFrameTimer > 0.0f) {
                if (((int)(iFrameTimer * 26.0f) % 2) == 0) {
                    C2D_ImageTint iTint;
                    C2D_PlainImageTint(&iTint, C2D_Color32(255, 200, 200, 130), 1.0f);
                    C2D_DrawImageAtRotated(soulImg, soulDrawCX, soulDrawCY, 0.60f, curSoulAngle, &iTint, 2.0f, 2.0f);
                }
            } else {
                C2D_DrawImageAtRotated(soulImg, soulDrawCX, soulDrawCY, 0.60f, curSoulAngle, nullptr, 2.0f, 2.0f);
            }
        }

        if (grazeSheet && grazeImg.tex && grazePulseTimer > 0.0f) {
            float p = std::max(0.0f, std::min(1.0f, grazePulseTimer / 0.22f));
            float pulseScale = 2.0f * (1.0f + p * 0.06f);
            u8 a = (u8)(std::min(1.0f, p * 1.8f) * 255.0f);
            C2D_ImageTint gTint;
            C2D_PlainImageTint(&gTint, C2D_Color32(255, 255, 255, a), 1.0f);
            C2D_DrawImageAtRotated(grazeImg, soulDrawCX, soulDrawCY, 0.61f, curSoulAngle, &gTint, pulseScale, pulseScale);
        }
    }

    float fadeAlpha = 1.0f - (fadeTimer / FADE_DURATION);
    if (fadeAlpha > 0.0f) {
        C2D_DrawRectSolid(0.0f, 0.0f, 0.99f, 400.0f, 240.0f, C2D_Color32(255, 255, 255, (u8)(fadeAlpha * 255.0f)));
    }

    // Bottom screen HUD
    if (!g_isRightEye && bottom) {
        C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(bottom);

        if (textBuf) C2D_TextBufClear(textBuf);

        const float topBoxY = 0.0f;
        const float topBoxH = 42.0f;
        const float topBoxBottom = topBoxY + topBoxH;
        const float gapCenterY = (topBoxBottom + DIALOG_BOX_Y) * 0.5f;

        // Animated red scanlines
        const float lineSpacing = 8.0f;
        float offsetDown = fmodf(scrollDownTimer, lineSpacing);
        for (float y = topBoxBottom + offsetDown; y < gapCenterY; y += lineSpacing) {
            float progress = (y - topBoxBottom) / (gapCenterY - topBoxBottom);
            float a = (1.0f - progress) * 190.0f;
            if (a > 0.0f) C2D_DrawRectSolid(0.0f, y, 0.05f, 320.0f, 1.5f, C2D_Color32(235, 45, 45, (u8)a));
        }

        float offsetUp = fmodf(scrollUpTimer, lineSpacing);
        for (float y = DIALOG_BOX_Y - offsetUp; y > gapCenterY; y -= lineSpacing) {
            float progress = (DIALOG_BOX_Y - y) / (DIALOG_BOX_Y - gapCenterY);
            float a = (1.0f - progress) * 190.0f;
            if (a > 0.0f) C2D_DrawRectSolid(0.0f, y, 0.05f, 320.0f, 1.5f, C2D_Color32(235, 45, 45, (u8)a));
        }

        // Top HUD box
        C2D_DrawRectSolid(0.0f, topBoxY, 0.30f, 320.0f, topBoxH, C2D_Color32(235, 55, 55, 255));
        C2D_DrawRectSolid(0.0f, topBoxY + 2.0f, 0.31f, 320.0f, topBoxH - 4.0f, C2D_Color32(0, 0, 0, 255));

        if (fontToUse && textBuf) {
            C2D_TextFontParse(&nameText, fontToUse, textBuf, "YOU");
            C2D_TextOptimize(&nameText);
            C2D_DrawText(&nameText, C2D_WithColor, 12.0f, topBoxY + 12.0f, 0.40f, 0.56f, 0.56f, C2D_Color32(255, 255, 255, 255));

            std::string hpStr = std::to_string(curHp) + "/ " + std::to_string(maxHp);
            u32 hpTextColor = (curKarma > 0) ? C2D_Color32(0xFF, 0x01, 0xFF, 255) : C2D_Color32(255, 255, 255, 255);
            C2D_TextFontParse(&hpNumText, fontToUse, textBuf, hpStr.c_str());
            C2D_TextOptimize(&hpNumText);
            C2D_DrawText(&hpNumText, C2D_WithColor, 216.0f, topBoxY + 8.0f, 0.40f, 0.46f, 0.46f, hpTextColor);

            C2D_TextFontParse(&krLabelText, fontToUse, textBuf, "KR");
            C2D_TextOptimize(&krLabelText);
            C2D_DrawText(&krLabelText, C2D_WithColor, 284.0f, topBoxY + 8.0f, 0.40f, 0.46f, 0.46f, C2D_Color32(0xFF, 0x01, 0xFF, 255));

            C2D_TextFontParse(&hpLabelText, fontToUse, textBuf, "HP");
            C2D_TextOptimize(&hpLabelText);
            C2D_DrawText(&hpLabelText, C2D_WithColor, 186.0f, topBoxY + 23.0f, 0.40f, 0.40f, 0.40f, C2D_Color32(255, 255, 255, 255));
        }

        const float barX = 208.0f, barY = topBoxY + 24.0f, barW = 100.0f, barH = 9.0f;
        C2D_DrawRectSolid(barX, barY, 0.35f, barW, barH, C2D_Color32(0xC1, 0x04, 0x04, 255));

        int safeHp = std::max(0, curHp - curKarma);
        float safeRatio = (maxHp > 0) ? std::min(1.0f, (float)safeHp / (float)maxHp) : 0.0f;
        float totalRatio = (maxHp > 0) ? std::min(1.0f, (float)curHp / (float)maxHp) : 0.0f;

        if (safeRatio > 0.0f) {
            C2D_DrawRectSolid(barX, barY, 0.36f, barW * safeRatio, barH, C2D_Color32(255, 105, 105, 255));
        }
        if (totalRatio > safeRatio) {
            float krX = barX + barW * safeRatio;
            float krW = barW * (totalRatio - safeRatio);
            C2D_DrawRectSolid(krX, barY, 0.37f, krW, barH, C2D_Color32(0xFF, 0x01, 0xFF, 255));
        }

        // Action buttons
        const float btnStartX = 55.0f, btnW = 34.0f, btnH = 26.0f, btnGap = 10.0f, btnBaseY = 50.0f;
        const char* actionNames[ACTION_COUNT] = {"FIGHT", "ACT", "ITEM", "DEFEND", "TEST"};
        Frame* actionIcons[4] = {frameAttack, frameAct, frameItem, frameDefense};

        for (int i = 0; i < ACTION_COUNT; i++) {
            bool isSel = (i == curAction && duelPhase == DuelPhase::ACTION_SELECT);
            float bx = btnStartX + i * (btnW + btnGap);
            float by = btnBaseY + (isSel ? -2.0f : 0.0f);

            u32 borderCol = isSel ? C2D_Color32(255, 220, 40, 255) : C2D_Color32(235, 55, 55, 255);
            C2D_DrawRectSolid(bx - 1.0f, by - 1.0f, 0.25f, btnW + 2.0f, btnH + 2.0f, borderCol);
            C2D_DrawRectSolid(bx, by, 0.26f, btnW, btnH, C2D_Color32(0, 0, 0, 255));

            if (i < 4 && actionIcons[i] && actionIcons[i]->tex) {
                Frame* f = actionIcons[i];
                Tex3DS_SubTexture sub;
                sub.width = f->uv.width;
                sub.height = f->uv.height;
                sub.left = f->uv.left;
                sub.top = f->uv.top;
                sub.right = f->uv.right;
                sub.bottom = f->uv.bottom;
                C2D_Image img;
                img.tex = f->tex;
                img.subtex = &sub;

                float ix = bx + (btnW - (float)f->w) * 0.5f;
                float iy = by + (btnH - (float)f->h) * 0.5f;
                C2D_ImageTint t;
                u32 iconCol = isSel ? C2D_Color32(255, 220, 40, 255) : C2D_Color32(255, 255, 255, 255);
                C2D_PlainImageTint(&t, iconCol, 1.0f);
                C2D_DrawImageAt(img, ix, iy, 0.28f, &t, 1.0f, 1.0f);
            } else if (i == 4 && fontToUse && textBuf) {
                std::string numStr = "<0" + std::to_string(curAttack) + ">";
                C2D_Text numText;
                C2D_TextFontParse(&numText, fontToUse, textBuf, numStr.c_str());
                C2D_TextOptimize(&numText);
                float nw = 0.0f, nh = 0.0f;
                C2D_TextGetDimensions(&numText, 0.35f, 0.35f, &nw, &nh);
                float tx = bx + (btnW - nw) * 0.5f;
                float ty = by + (btnH - nh) * 0.5f;
                u32 tCol = isSel ? C2D_Color32(255, 220, 40, 255) : C2D_Color32(255, 255, 255, 255);
                C2D_DrawText(&numText, C2D_WithColor, tx, ty, 0.28f, 0.35f, 0.35f, tCol);
            }

            if (fontToUse && textBuf) {
                C2D_TextFontParse(&actionLabels[i], fontToUse, textBuf, actionNames[i]);
                C2D_TextOptimize(&actionLabels[i]);
                float tw = 0.0f, th = 0.0f;
                C2D_TextGetDimensions(&actionLabels[i], 0.38f, 0.38f, &tw, &th);
                float tx = bx + (btnW - tw) * 0.5f;
                float ty = by + btnH + 3.0f;
                u32 labelColor = isSel ? C2D_Color32(255, 220, 40, 255) : C2D_Color32(180, 180, 180, 255);
                C2D_DrawText(&actionLabels[i], C2D_WithColor, tx, ty, 0.28f, 0.38f, 0.38f, labelColor);
            }
        }

        // Dialogue box
        C2D_DrawRectSolid(DIALOG_BOX_X, DIALOG_BOX_Y, 0.30f, DIALOG_BOX_W, DIALOG_BOX_H, C2D_Color32(235, 55, 55, 255));
        C2D_DrawRectSolid(DIALOG_BOX_X + 2.0f, DIALOG_BOX_Y + 2.0f, 0.31f, DIALOG_BOX_W - 4.0f, DIALOG_BOX_H - 4.0f, C2D_Color32(0, 0, 0, 255));

        setScissorBoxBottom(DIALOG_BOX_X + 2.0f, DIALOG_BOX_Y + 2.0f, DIALOG_BOX_W - 4.0f, DIALOG_BOX_H - 4.0f);

        if (fontToUse && textBuf) {
            float scale = 0.55f;
            float lineH = 15.0f;
            float textX = DIALOG_BOX_X + 16.0f;
            float textY = DIALOG_BOX_Y + 14.0f;

            if (curHp <= 0) {
                const char* deadMsg1 = "* You feel your determination";
                const char* deadMsg2 = "  slipping away...";
                const char* deadMsg3 = "* Press A to revive.";
                C2D_Text t1, t2, t3;
                C2D_TextFontParse(&t1, fontToUse, textBuf, deadMsg1);
                C2D_TextFontParse(&t2, fontToUse, textBuf, deadMsg2);
                C2D_TextFontParse(&t3, fontToUse, textBuf, deadMsg3);
                C2D_TextOptimize(&t1);
                C2D_TextOptimize(&t2);
                C2D_TextOptimize(&t3);
                u32 dCol = C2D_Color32(255, 80, 80, 255);
                C2D_DrawText(&t1, C2D_WithColor, textX, textY, 0.35f, scale, scale, dCol);
                C2D_DrawText(&t2, C2D_WithColor, textX, textY + lineH, 0.35f, scale, scale, dCol);
                C2D_DrawText(&t3, C2D_WithColor, textX, textY + lineH * 2.0f, 0.35f, scale, scale, C2D_Color32(255, 255, 60, 255));
            } else if (duelPhase == DuelPhase::ACTION_SELECT) {
                const char* selectMsg1 = "* The wind roars violently";
                const char* selectMsg2 = "  across the open abyss.";
                const char* selectMsg3 = "* Stand your ground.";
                C2D_Text t1, t2, t3;
                C2D_TextFontParse(&t1, fontToUse, textBuf, selectMsg1);
                C2D_TextFontParse(&t2, fontToUse, textBuf, selectMsg2);
                C2D_TextFontParse(&t3, fontToUse, textBuf, selectMsg3);
                C2D_TextOptimize(&t1);
                C2D_TextOptimize(&t2);
                C2D_TextOptimize(&t3);
                C2D_DrawText(&t1, C2D_WithColor, textX, textY, 0.35f, scale, scale, C2D_Color32(255, 255, 255, 255));
                C2D_DrawText(&t2, C2D_WithColor, textX, textY + lineH, 0.35f, scale, scale, C2D_Color32(255, 255, 255, 255));
                C2D_DrawText(&t3, C2D_WithColor, textX, textY + lineH * 2.0f, 0.35f, scale, scale, C2D_Color32(255, 255, 255, 255));
            } else {
                const char* dodgeMsg1 = "* Dodging the barrage...";
                C2D_Text d1;
                C2D_TextFontParse(&d1, fontToUse, textBuf, dodgeMsg1);
                C2D_TextOptimize(&d1);
                C2D_DrawText(&d1, C2D_WithColor, textX, textY, 0.35f, scale, scale, C2D_Color32(200, 200, 200, 255));
            }
        }

        disableScissor();

        // TP Gauge
        drawTPBar(45.0f, 100.0f, 230.0f);

        if (fontToUse && textBuf) {
            C2D_TextFontParse(&tpLabelText, fontToUse, textBuf, "T\nP");
            C2D_TextOptimize(&tpLabelText);
            C2D_DrawText(&tpLabelText, C2D_WithColor | C2D_AlignCenter, 25.0f, 98.0f, 0.46f, 0.44f, 0.44f, C2D_Color32(255, 255, 255, 255));

            int tpDisplayInt = (int)curTP;
            if (tpDisplayInt > 100) tpDisplayInt = 100;
            std::string pctStr = std::to_string(tpDisplayInt) + "%";
            C2D_TextFontParse(&tpPercentText, fontToUse, textBuf, pctStr.c_str());
            C2D_TextOptimize(&tpPercentText);
            u32 pctColor = (curTP >= 99.9f) ? C2D_Color32(255, 207, 34, 255) : C2D_Color32(255, 255, 255, 255);
            C2D_DrawText(&tpPercentText, C2D_WithColor, 282.0f, 106.0f, 0.46f, 0.40f, 0.40f, pctColor);
        }
    }
}
