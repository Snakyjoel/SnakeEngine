#include "TextureLoader.hpp"
#include "ModHandler.hpp"
#include "Paths.hpp"
#include "stb_image.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

bool TextureLoader::loadPng(const std::string& path, C3D_Tex** outTex, Tex3DS_SubTexture** outSubtex) {
    if (!outTex || !outSubtex) return false;
    *outTex = nullptr;
    *outSubtex = nullptr;

    if (path.empty() || !Paths::fileExists(path)) return false;

    int w = 0, h = 0, c = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 4);
    if (!data || w <= 0 || h <= 0) {
        if (data) stbi_image_free(data);
        return false;
    }

    int pw = 1, ph = 1;
    while (pw < w) pw *= 2;
    while (ph < h) ph *= 2;

    C3D_Tex* tex = new C3D_Tex();
    memset(tex, 0, sizeof(C3D_Tex));
    if (!C3D_TexInit(tex, pw, ph, GPU_RGBA8)) {
        delete tex;
        stbi_image_free(data);
        return false;
    }

    size_t sz = (size_t)pw * ph * 4;
    uint32_t* swizzled = (uint32_t*)linearAlloc(sz);
    if (!swizzled) {
        C3D_TexDelete(tex);
        delete tex;
        stbi_image_free(data);
        return false;
    }

    memset(swizzled, 0, sz);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int src = (y * w + x) * 4;
            uint32_t px = ((uint32_t)data[src]   << 24) |
                          ((uint32_t)data[src+1] << 16) |
                          ((uint32_t)data[src+2] <<  8) |
                           (uint32_t)data[src+3];
            uint32_t i  = (x & 7) | ((y & 7) << 8);
            i = (i ^ (i << 2)) & 0x1313;
            i = (i ^ (i << 1)) & 0x1515;
            uint32_t tx = x >> 3, ty = y >> 3;
            uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
            uint32_t local_idx  = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
            swizzled[tile_start + local_idx] = px;
        }
    }
    stbi_image_free(data);

    C3D_TexUpload(tex, swizzled);
    C3D_TexFlush(tex);
    linearFree(swizzled);

    Tex3DS_SubTexture* sub = new Tex3DS_SubTexture();
    sub->width  = (u16)w;
    sub->height = (u16)h;
    sub->left   = 0.0f;
    sub->top    = 1.0f;
    sub->right  = (float)w / (float)pw;
    sub->bottom = 1.0f - ((float)h / (float)ph);

    *outTex = tex;
    *outSubtex = sub;
    return true;
}

std::string TextureLoader::resolveImagePath(const std::string& key, const std::string& library) {
    // 1. Try .t3x
    std::string pathT3x = Paths::image(key, library);
    if (Paths::fileExists(pathT3x)) return pathT3x;

    // 2. Try .rawtex
    std::string rawMod = ModHandler::get().getModPath("images/" + key + ".rawtex");
    if (!rawMod.empty() && Paths::fileExists(rawMod)) return rawMod;
    if (!library.empty()) {
        std::string rawRom = "romfs:/" + library + "/images/" + key + ".rawtex";
        if (Paths::fileExists(rawRom)) return rawRom;
    }
    if (Paths::fileExists("romfs:/shared/images/" + key + ".rawtex")) return "romfs:/shared/images/" + key + ".rawtex";
    if (Paths::fileExists("romfs:/preload/images/" + key + ".rawtex")) return "romfs:/preload/images/" + key + ".rawtex";

    // 3. Try .png
    std::string pngMod = ModHandler::get().getModPath("images/" + key + ".png");
    if (!pngMod.empty() && Paths::fileExists(pngMod)) return pngMod;
    if (!library.empty()) {
        std::string pngRom = "romfs:/" + library + "/images/" + key + ".png";
        if (Paths::fileExists(pngRom)) return pngRom;
    }
    if (Paths::fileExists("romfs:/shared/images/" + key + ".png")) return "romfs:/shared/images/" + key + ".png";
    if (Paths::fileExists("romfs:/preload/images/" + key + ".png")) return "romfs:/preload/images/" + key + ".png";

    return "";
}
