#include "StoryMenuState.hpp"
#include "PlayState.hpp"
#include "MainMenuState.hpp"
#include "VideoState.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/AudioEngine.hpp"
#include "../objects/ButtonPrompt.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>

static std::string lastDifficultyName = "Normal";

static void enforceLRUCache(std::unordered_map<std::string, StoryMenuState::StoryCacheEntry>& cache, size_t maxSize) {
    if (cache.size() > maxSize) {
        auto oldest = cache.begin();
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.lastAccessFrame < oldest->second.lastAccessFrame) {
                oldest = it;
            }
        }
        if (oldest->second.sheet) {
            C2D_SpriteSheetFree(oldest->second.sheet);
        }
        cache.erase(oldest);
    }
}

void StoryMenuState::init() {
    ModHandler::get().currentModFolder = "";

    MusicPlayer::playMenuMusic();

    VCRFontFix();

    curSelected = 0;
    WeekData::reloadWeekFiles();
    
    selectableWeeks.clear();
    for (const auto& weekName : WeekData::weeksList) {
        if (WeekData::weeksLoaded.find(weekName) != WeekData::weeksLoaded.end()) {
            WeekData& data = WeekData::weeksLoaded[weekName];
            if (!data.hideStoryMode) {
                selectableWeeks.push_back(weekName);
            }
        }
    }

    if (!selectableWeeks.empty()) {
        updateDifficulties();
    }
 
    uiSheet = C2D_SpriteSheetLoad("romfs:/preload/images/campaign_menu_UI_assets.t3x");
    uiFrames.clear();
    if (uiSheet) {
        C2D_Image mainImg = C2D_SpriteSheetGetImage(uiSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);

        SparrowParser::parseXml("romfs:/preload/images/campaign_menu_UI_assets.xml", uiFrames);
        float rw = mainImg.subtex->right - mainImg.subtex->left;
        float rh = mainImg.subtex->bottom - mainImg.subtex->top;

        for (auto& f : uiFrames) {
            f.tex = mainImg.tex;
            f.uv.width = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left = mainImg.subtex->left + ((float)f.x * rw / (float)mainImg.subtex->width);
            f.uv.top = mainImg.subtex->top + ((float)f.y * rh / (float)mainImg.subtex->height);
            f.uv.right = mainImg.subtex->left + ((float)(f.x + f.w) * rw / (float)mainImg.subtex->width);
            f.uv.bottom = mainImg.subtex->top + ((float)(f.y + f.h) * rh / (float)mainImg.subtex->height);
        }

        auto getUIFrame = [&](const std::string& name) -> Frame* {
            for (size_t i = 0; i < uiFrames.size(); i++) {
                if (uiFrames[i].name.find(name) == 0) {
                    return &uiFrames[i];
                }
            }
            return nullptr;
        };
        arrowLeftFrame = getUIFrame("arrow left");
        arrowPushLeftFrame = getUIFrame("arrow push left");
        arrowRightFrame = getUIFrame("arrow right");
        arrowPushRightFrame = getUIFrame("arrow push right");
        lockFrame = getUIFrame("lock");
    }

    weekCache.clear();
    diffCache.clear();
}

void StoryMenuState::updateDifficulties() {
    curWeekDiffs.clear();
    if (selectableWeeks.empty()) return;
    WeekData& data = WeekData::weeksLoaded[selectableWeeks[curSelected]];
    std::string diffs = data.difficulties;
    
    if (diffs.empty()) {
        curWeekDiffs = {"Easy", "Normal", "Hard"};
    } else {
        std::stringstream ss(diffs);
        std::string d;
        while (std::getline(ss, d, ',')) {
            size_t first = d.find_first_not_of(' ');
            if (first == std::string::npos) continue;
            size_t last = d.find_last_not_of(' ');
            curWeekDiffs.push_back(d.substr(first, last - first + 1));
        }
    }
    
    int normalIndex = -1;
    int lastDiffIndex = -1;
    for (int i = 0; i < (int)curWeekDiffs.size(); ++i) {
        if (curWeekDiffs[i] == lastDifficultyName) {
            lastDiffIndex = i;
        }
        if (curWeekDiffs[i] == "Normal") {
            normalIndex = i;
        }
    }

    if (lastDiffIndex != -1) {
        curDifficulty = lastDiffIndex;
    } else if (normalIndex != -1) {
        curDifficulty = normalIndex;
    } else {
        curDifficulty = 0;
    }
}

void StoryMenuState::update(float dt) {
    cacheFrameCount++;
    u32 kDown = hidKeysDown();
    touchPosition touch;
    hidTouchRead(&touch);

    if (!selectableWeeks.empty()) {
        int prevIdx = curSelected - 1;
        if (prevIdx < 0) prevIdx = selectableWeeks.size() - 1;
        int nextIdx = curSelected + 1;
        if (nextIdx >= (int)selectableWeeks.size()) nextIdx = 0;
        
        getWeekImage(selectableWeeks[prevIdx]);
        getWeekImage(selectableWeeks[nextIdx]);

        if ((keyJustPressed(KEY_DUP) || keyJustPressed(KEY_CPAD_UP))) {
            curSelected--;
            if (curSelected < 0) curSelected = (int)selectableWeeks.size() - 1;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            updateDifficulties();
        }
        if ((keyJustPressed(KEY_DDOWN) || keyJustPressed(KEY_CPAD_DOWN))) {
            curSelected++;
            if (curSelected >= (int)selectableWeeks.size()) curSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
            updateDifficulties();
        }

        if ((keyJustPressed(KEY_DLEFT) || keyJustPressed(KEY_CPAD_LEFT))) {
            curDifficulty--;
            if (curDifficulty < 0) curDifficulty = (int)curWeekDiffs.size() - 1;
            lastDifficultyName = curWeekDiffs[curDifficulty];
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if ((keyJustPressed(KEY_DRIGHT) || keyJustPressed(KEY_CPAD_RIGHT))) {
            curDifficulty++;
            if (curDifficulty >= (int)curWeekDiffs.size()) curDifficulty = 0;
            lastDifficultyName = curWeekDiffs[curDifficulty];
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }

        if (keyJustPressed(KEY_A | KEY_START)) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            std::string weekName = selectableWeeks[curSelected];
            WeekData& data = WeekData::weeksLoaded[weekName];
            
            if (!data.songs.empty()) {
                std::string diff = curWeekDiffs[curDifficulty];
                std::string suffix = "";
                if (diff == "Easy") suffix = "easy";
                else if (diff == "Hard") suffix = "hard";
                else if (diff != "Normal") {
                    suffix = diff;
                    std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
                }
                if (data.isMod) {
                    ModHandler::get().currentModFolder = data.modFolder;
                }
                MusicPlayer::stop();
                if (!data.songs[0].introVideo.empty()) {
                    switchState(new VideoState(data.songs[0].introVideo, new PlayState(data, 0, suffix)));
                } else {
                    switchState(new PlayState(data, 0, suffix));
                }
            }
        }
    }

    if (keyJustPressed(KEY_B)) {
        switchState(new MainMenuState());
    }


    u32 kHeld = hidKeysHeld();
    u32 kUp = hidKeysUp();
    if (kDown & KEY_TOUCH) {
        touchActive = true;
        touchStartY = touch.py;
        lastTouchY = touch.py;
        touchStartLerp = lerpSelected;
        isDragging = false;
        scrollVelocity = 0.0f;
        touchHoldTime = 0.0f;
    } else if (kHeld & KEY_TOUCH && touchActive) {
        float deltaY = touch.py - lastTouchY;
        scrollVelocity = deltaY;
        lastTouchY = touch.py;
        
        float diffY = touch.py - touchStartY;
        if (std::abs(diffY) > 10.0f) {
            isDragging = true;
        }
        
        if (isDragging) {
            float listScroll = diffY / 80.0f; 
            lerpSelected = touchStartLerp - listScroll;
            
            int maxItems = (int)selectableWeeks.size() - 1;
            if (lerpSelected < -0.5f) lerpSelected = -0.5f;
            if (lerpSelected > maxItems + 0.5f) lerpSelected = maxItems + 0.5f;
            
            int oldSelected = curSelected;
            curSelected = (int)(lerpSelected + 0.5f);
            if (curSelected < 0) curSelected = 0;
            if (curSelected > maxItems) curSelected = maxItems;
            
            if (oldSelected != curSelected) {
                updateDifficulties();
            }
        } else {
            touchHoldTime += dt;
        }
    } else if (kUp & KEY_TOUCH && touchActive) {
        touchActive = false;
        if (isDragging) {
            if (std::abs(scrollVelocity) > 5.0f) {
                float projectedLerp = lerpSelected - (scrollVelocity / 15.0f);
                curSelected = (int)(projectedLerp + 0.5f);
                int maxItems = (int)selectableWeeks.size() - 1;
                if (curSelected < 0) curSelected = 0;
                if (curSelected > maxItems) curSelected = maxItems;
                updateDifficulties();
            }
            isDragging = false;
        }
    }

    if (!isDragging) {
        lerpSelected += (curSelected - lerpSelected) * (1.0f - exp2f(-12.0f * dt));
    }
}

void StoryMenuState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    ClearTextBuf();
    C2D_SceneBegin(top);
    C2D_TargetClear(top, C2D_Color32(0xF9, 0xCF, 0x51, 0xFF)); // New color: #f9cf51
 
    if (!selectableWeeks.empty()) {
        std::string weekName = selectableWeeks[curSelected];
        WeekData& data = WeekData::weeksLoaded[weekName];
        std::string bgName = data.weekBackground;
        if (!bgName.empty()) {
            C2D_Image bgImg = getWeekBackgroundImage(bgName);
            if (bgImg.tex) {
                float w = bgImg.subtex->width;
                float h = bgImg.subtex->height;
                drawImage(bgImg, 200.0f - (w / 2.0f), 120.0f - (h / 2.0f), 0.5f);
            }
        }
    }

    C2D_DrawRectSolid(0, 0, 0.8f, 400, 45, C2D_Color32(0, 0, 0, 255));
    C2D_DrawRectSolid(0, 195, 0.8f, 400, 45, C2D_Color32(0, 0, 0, 255));
 
    if (!selectableWeeks.empty()) {
        std::string weekName = selectableWeeks[curSelected];
        WeekData& data = WeekData::weeksLoaded[weekName];
        
        std::string storyText = data.storyName.empty() ? data.weekName : data.storyName;
        std::transform(storyText.begin(), storyText.end(), storyText.begin(), ::toupper);
        
        AddText(storyText, 200, 12, 0.45f, true, 0.0f, CGray, 0.0f);
        AddText("SCORE: 0", 200, 31, 0.45f, true, 0.0f, CWhite, 0.0f);
    }
 
    if (!selectableWeeks.empty()) {
        u32 kHeld = hidKeysHeld();
        Frame* aLeftFrame = (kHeld & (KEY_DLEFT | KEY_CPAD_LEFT)) ? arrowPushLeftFrame : arrowLeftFrame;
        Frame* aRightFrame = (kHeld & (KEY_DRIGHT | KEY_CPAD_RIGHT)) ? arrowPushRightFrame : arrowRightFrame;

        std::string curDiffStr = curWeekDiffs[curDifficulty];
        std::string lowerDiff = curDiffStr;
        std::transform(lowerDiff.begin(), lowerDiff.end(), lowerDiff.begin(), ::tolower);

        C2D_Image dImg = getDiffImage(lowerDiff);
        float diffY = 217.0f;
        if (dImg.tex) {
            float dW = dImg.subtex->width;
            float dH = dImg.subtex->height;
            drawImage(dImg, 200 - (dW / 2.0f), diffY - (dH / 2.0f), 0.85f);
            
            if (aLeftFrame && aLeftFrame->tex) {
                float aH = frameLogicalH(*aLeftFrame);
                drawFrameAt(*aLeftFrame, 200 - (dW / 2.0f) - 35, diffY - (aH / 2.0f), 0.85f);
            }
            if (aRightFrame && aRightFrame->tex) {
                float aW = frameLogicalW(*aRightFrame);
                float aH = frameLogicalH(*aRightFrame);
                drawFrameAt(*aRightFrame, 200 + (dW / 2.0f) + 35 - aW, diffY - (aH / 2.0f), 0.85f);
            }
        } else {
            AddText("< " + curDiffStr + " >", 200, diffY, 0.6f, true, 0.0f, CWhite, 0.0f);
        }
    }
 
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
 
    if (!selectableWeeks.empty()) {
        std::string weekName = selectableWeeks[curSelected];
        WeekData& data = WeekData::weeksLoaded[weekName];
 
        u32 tracksColor = C2D_Color32(200, 200, 200, 255);
        AddText("TRACKS", 30, 30, 0.65f, false, 0.0f, tracksColor, 0.0f);
 
        float songY = 70.0f;
        for (const auto& song : data.songs) {
            songY = drawWrappedText(song.name, 30, songY, 0.45f, 130.0f, tracksColor);
            songY += 10.0f;
        }
 
        float listX = 220.0f;
        float listCenterY = 120.0f;
        for (int i = 0; i < (int)selectableWeeks.size(); i++) {
            float dist = (i - lerpSelected);
            float itemY = listCenterY + dist * 50.0f;
            if (itemY < -60 || itemY > 300) continue;
 
            bool isSelected = (i == curSelected);
            bool isLocked = !WeekData::weeksLoaded[selectableWeeks[i]].startUnlocked;
            
            C2D_Image img = getWeekImage(selectableWeeks[i]);
            if (img.tex) {
                float scale = isSelected ? 0.7f : 0.5f;
                float imgW = img.subtex->width * scale;
                float imgH = img.subtex->height * scale;
                
                C2D_ImageTint tint;
                C2D_ImageTint* tintPtr = &tint;
                if (isLocked) {
                    C2D_PlainImageTint(&tint, C2D_Color32(50, 50, 50, 255), 1.0f);
                } else if (!isSelected) {
                    C2D_PlainImageTint(&tint, C2D_Color32(180, 180, 180, 255), 1.0f);
                } else {
                    if (img.tex && (img.tex->fmt == GPU_A8 || img.tex->fmt == GPU_A4)) {
                        C2D_PlainImageTint(&tint, C2D_Color32(255, 255, 255, 255), 1.0f);
                    } else {
                        C2D_AlphaImageTint(&tint, 1.0f);
                    }
                }
 
                drawImageScaledTinted(img, listX - (imgW / 2.0f), itemY - (imgH / 2.0f), 0.5f, scale, scale, tintPtr);
                
                if (isLocked && lockFrame && lockFrame->tex) {
                    float lW = frameLogicalW(*lockFrame) * scale;
                    float lH = frameLogicalH(*lockFrame) * scale;
                    drawFrameAt(*lockFrame, listX - (lW / 2.0f), itemY - (lH / 2.0f), 0.51f, nullptr, scale, scale);
                }
            } else {
                std::string weekDisplayName = WeekData::weeksLoaded[selectableWeeks[i]].weekName;
                AddText(weekDisplayName, listX, itemY, isSelected ? 0.65f : 0.45f, true, isSelected ? 2.0f : 0.0f, isSelected ? CWhite : C2D_Color32(110, 110, 110, 255), 0.0f);
            }
        }
    }
    ButtonPrompt::drawPrompt("b", "Back", 8.0f, 205.0f, 0.70f, 1.0f);
}


float StoryMenuState::drawWrappedText(const std::string& text, float x, float y, float scale, float wrapWidth, u32 color) {
    std::stringstream ss(text);
    std::string word;
    std::string line = "";
    float currentY = y;
    float lineHeight = 24.0f * scale;
    
    while (ss >> word) {
        std::string testLine = line.empty() ? word : line + " " + word;
        
        C2D_Text gText;
        C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, testLine.c_str());
        
        float tw, th;
        C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
        
        if (tw > wrapWidth && !line.empty()) {
        AddText(line, x, currentY, scale, false, 0.0f, color, 0.0f);
            line = word;
            currentY += lineHeight;
        } else {
            line = testLine;
        }
    }
    if (!line.empty()) {
        AddText(line, x, currentY, scale, false, 0.0f, color, 0.0f);
    }
    currentY += lineHeight;
    return currentY;
}

void StoryMenuState::exitState() {
    for (auto const& pair : weekCache) { if(pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet); }
    for (auto const& pair : diffCache) { if(pair.second.sheet) C2D_SpriteSheetFree(pair.second.sheet); }
    if (uiSheet) C2D_SpriteSheetFree(uiSheet);
    if (backgroundSheet) C2D_SpriteSheetFree(backgroundSheet);
    backgroundSheet = nullptr;

    weekCache.clear();
    diffCache.clear();
    
    C2D_TextBufDelete(vcrFontBuf);
}

C2D_Image StoryMenuState::getWeekBackgroundImage(const std::string& name) {
    if (!selectableWeeks.empty() && curSelected >= 0 && curSelected < (int)selectableWeeks.size()) {
        WeekData& data = WeekData::weeksLoaded[selectableWeeks[curSelected]];
        ModHandler::get().currentModFolder = data.isMod ? data.modFolder : "";
    } else {
        ModHandler::get().currentModFolder = "";
    }

    if (backgroundSheet && currentBackground == name) return C2D_SpriteSheetGetImage(backgroundSheet, 0);
    
    if (backgroundSheet) {
        C2D_SpriteSheetFree(backgroundSheet);
        backgroundSheet = nullptr;
    }

    std::string path = Paths::image("menubackgrounds/" + name);
    if (!Paths::fileExists(path)) {
        path = Paths::image("menubackgrounds/placeholder");
    }

    if (Paths::fileExists(path)) {
        backgroundSheet = C2D_SpriteSheetLoad(path.c_str());
        if (backgroundSheet) {
            currentBackground = name;
            C2D_Image img = C2D_SpriteSheetGetImage(backgroundSheet, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            return img;
        }
    }
    return {nullptr, nullptr};
}

C2D_Image StoryMenuState::getWeekImage(const std::string& name) {
    if (WeekData::weeksLoaded.count(name)) {
        WeekData& data = WeekData::weeksLoaded[name];
        ModHandler::get().currentModFolder = data.isMod ? data.modFolder : "";
    } else {
        ModHandler::get().currentModFolder = "";
    }

    if (weekCache.count(name)) {
        weekCache[name].lastAccessFrame = cacheFrameCount;
        return C2D_SpriteSheetGetImage(weekCache[name].sheet, 0);
    }

    std::string path = Paths::image("storymenu/" + name);
    if (!Paths::fileExists(path)) {
        path = Paths::image("storymenu/placeholder");
    }
    if (Paths::fileExists(path)) {
        C2D_SpriteSheet s = C2D_SpriteSheetLoad(path.c_str());
        if (s) {
            C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            
            enforceLRUCache(weekCache, MAX_CACHE);
            weekCache[name] = {s, cacheFrameCount};
            return img;
        }
    }
    return {nullptr, nullptr};
}

C2D_Image StoryMenuState::getDiffImage(const std::string& name) {
    if (!selectableWeeks.empty() && curSelected >= 0 && curSelected < (int)selectableWeeks.size()) {
        WeekData& data = WeekData::weeksLoaded[selectableWeeks[curSelected]];
        ModHandler::get().currentModFolder = data.isMod ? data.modFolder : "";
    } else {
        ModHandler::get().currentModFolder = "";
    }

    if (diffCache.count(name)) {
        diffCache[name].lastAccessFrame = cacheFrameCount;
        return C2D_SpriteSheetGetImage(diffCache[name].sheet, 0);
    }

    std::string path = Paths::image("menudifficulties/" + name);
    if (!Paths::fileExists(path)) {
        path = Paths::image("menudifficulties/placeholder");
    }
    if (Paths::fileExists(path)) {
        C2D_SpriteSheet s = C2D_SpriteSheetLoad(path.c_str());
        if (s) {
            C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
            if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            
            enforceLRUCache(diffCache, MAX_CACHE);
            diffCache[name] = {s, cacheFrameCount};
            return img;
        }
    }
    return {nullptr, nullptr};
}
