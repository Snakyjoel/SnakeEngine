#pragma once

struct Note {
    float strumTime;
    int noteData;
    float sustainLength;
    bool isPlayer;
    bool hit; // Internal hit flag
    bool wasGoodHit; // Successfully hit
    bool sustainActive; // Actively being held
    bool gfNote; // GF sings this note
    bool ignoreNote; // Ignore after miss
    
    // Custom note properties
    std::string noteType;
    std::string texture;
    bool noAnimation;
    float scaleX;
    float scaleY;
    float offsetX;
    float offsetY;
    float multAlpha;
    float angle;
    bool visible;
    u32 color;
    bool antialiasing;
    bool flipX;
    bool flipY;

    Note() : strumTime(0), noteData(0), sustainLength(0), isPlayer(false), hit(false), 
             wasGoodHit(false), sustainActive(false), gfNote(false), ignoreNote(false),
             noteType(""), texture(""), noAnimation(false), scaleX(1.0f), scaleY(1.0f), 
             offsetX(0.0f), offsetY(0.0f), multAlpha(1.0f), angle(0.0f), visible(true),
             color(0xFFFFFFFF), antialiasing(true), flipX(false), flipY(false) {}
};
