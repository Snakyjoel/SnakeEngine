#pragma once
#include <citro2d.h>
#include <string>

class TextureLoader {
public:
    // Decodes a PNG file, swizzles into 3DS GPU tile order, and uploads to C3D_Tex.
    // Creates a matching Tex3DS_SubTexture with UV coordinates [0..w/pw, 1..(1 - h/ph)].
    // Returns true on success, false on failure.
    // Caller owns the allocated *outTex and *outSubtex.
    static bool loadPng(const std::string& path, C3D_Tex** outTex, Tex3DS_SubTexture** outSubtex);

    // Resolves an image path by trying:
    // 1. .t3x
    // 2. .rawtex
    // 3. .png
    // Returns the resolved path or empty string if none found.
    static std::string resolveImagePath(const std::string& key, const std::string& library = "");
};
