#include "StoryMenuState.hpp"
#include "PlayState.hpp"
#include "MainMenuState.hpp"
#include "VideoState.hpp"
#include "../backend/ModHandler.hpp"
#include "../backend/AudioEngine.hpp"
#include "../backend/Paths.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <3ds.h>

static std::string lastDifficultyName = "Normal";


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
        arrowLeftFrame      = getUIFrame("arrow left");
        arrowPushLeftFrame  = getUIFrame("arrow push left");
        arrowRightFrame     = getUIFrame("arrow right");
        arrowPushRightFrame = getUIFrame("arrow push right");
        lockFrame           = getUIFrame("lock");
    }

    // TRACKS
    if (tracksSheet) { C2D_SpriteSheetFree(tracksSheet); tracksSheet = nullptr; }
    tracksSheet = C2D_SpriteSheetLoad("romfs:/preload/images/menus/Menu_Tracks.t3x");
    if (tracksSheet) {
        tracksImg = C2D_SpriteSheetGetImage(tracksSheet, 0);
    } else {
        tracksImg = {};
    }

    // Reset confirm animation
    selectedWeek   = false;
    confirmTimer   = 0.0f;
    flickerTimer   = 0.0f;
    flickerVisible = true;
    pendingDiffSuffix.clear();
    pendingIntroVideo.clear();
    pendingIsMod    = false;
    pendingModFolder.clear();

    // ── Start background loading thread ──────────────────────────────────
    lastSelectedCheck = -1;
    lastDiffCheck     = -1;
    loadingAngle      = 0.0f;
    weekSheets.clear();
    pendingWeekIndices.clear();
    activeBgSheet   = nullptr;  activeBgName.clear();
    activeDiffSheet = nullptr;  activeDiffName.clear();

    LightLock_Init(&loadLock);
    LightEvent_Init(&loadEvent, RESET_ONESHOT);
    threadRunning = true;
    s32 prio;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    loadThread = threadCreate(threadMain, this, 32 * 1024, prio + 1, -2, false);

    triggerWindowLoad();
    triggerDiffLoad();
    // ─────────────────────────────────────────────────────────────────────
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
    // ── Consume background-thread results ─────────────────────────────────
    loadingAngle += dt * 3.14159f * 2.0f;

    {
        std::deque<LoadedResult> local;
        LightLock_Lock(&loadLock);
        local.swap(resultQueue);
        LightLock_Unlock(&loadLock);

        for (auto& res : local) {
            if (!res.buffer) continue;
            C2D_SpriteSheet s = C2D_SpriteSheetLoadFromMem(res.buffer, res.size);
            linearFree(res.buffer);
            if (!s) continue;

            if (res.type == AsyncLoadRequest::WEEK_BANNER) {
                // Free any old sheet at this slot
                auto it = weekSheets.find(res.weekIndex);
                if (it != weekSheets.end() && it->second) Paths_freeSpriteSheet(it->second);
                weekSheets[res.weekIndex] = s;
                C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
                if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            } else if (res.type == AsyncLoadRequest::BACKGROUND) {
                if (activeBgSheet) Paths_freeSpriteSheet(activeBgSheet);
                activeBgSheet = s;
                C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
                if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            } else if (res.type == AsyncLoadRequest::DIFFICULTY) {
                if (activeDiffSheet) Paths_freeSpriteSheet(activeDiffSheet);
                activeDiffSheet = s;
                C2D_Image img = C2D_SpriteSheetGetImage(s, 0);
                if (img.tex) C3D_TexSetFilter(img.tex, GPU_LINEAR, GPU_LINEAR);
            }
        }
    }

    // Trigger new loads when selection or difficulty changes
    if (lastSelectedCheck != curSelected) {
        lastSelectedCheck = curSelected;
        triggerWindowLoad();
    }
    if (lastDiffCheck != curDifficulty) {
        lastDiffCheck = curDifficulty;
        triggerDiffLoad();
    }
    // ─────────────────────────────────────────────────────────────────────

    // Confirm animation update
    if (selectedWeek) {
        confirmTimer += dt;
        flickerTimer += dt;
        if (flickerTimer >= 0.06f) {
            flickerTimer -= 0.06f;
            flickerVisible = !flickerVisible;
        }
        if (confirmTimer >= 1.0f) {
            if (!pendingIntroVideo.empty()) {
                WeekData& data = WeekData::weeksLoaded[selectableWeeks[curSelected]];
                switchState(new VideoState(pendingIntroVideo, new PlayState(data, 0, pendingDiffSuffix)));
            } else {
                WeekData& data = WeekData::weeksLoaded[selectableWeeks[curSelected]];
                switchState(new PlayState(data, 0, pendingDiffSuffix));
            }
        }
        return;
    }

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
            std::string weekName = selectableWeeks[curSelected];
            WeekData& data = WeekData::weeksLoaded[weekName];

            if (!data.songs.empty()) {
                std::string diff = curWeekDiffs[curDifficulty];
                pendingDiffSuffix = "";
                if (diff == "Easy") pendingDiffSuffix = "easy";
                else if (diff == "Hard") pendingDiffSuffix = "hard";
                else if (diff != "Normal") {
                    pendingDiffSuffix = diff;
                    std::transform(pendingDiffSuffix.begin(), pendingDiffSuffix.end(), pendingDiffSuffix.begin(), ::tolower);
                }
                pendingIsMod    = data.isMod;
                pendingModFolder = data.isMod ? data.modFolder : "";
                pendingIntroVideo = data.songs[0].introVideo;

                if (data.isMod) {
                    ModHandler::get().currentModFolder = data.modFolder;
                }
                MusicPlayer::stop();

                // confirm animation
                AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
                selectedWeek   = true;
                confirmTimer   = 0.0f;
                flickerTimer   = 0.0f;
                flickerVisible = true;
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
                drawImage(bgImg, 200.0f - (w / 2.0f) + get3DOffset(-10.0f), 120.0f - (h / 2.0f), 0.5f);
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
        
        AddText(storyText, 200 + get3DOffset(10.0f), 12, 0.45f, true, 0.0f, C2D_Color32(0xB2, 0xB2, 0xB2, 255), 0.0f);
        AddText("LEVEL SCORE: 0", 200 + get3DOffset(10.0f), 31, 0.45f, true, 0.0f, CWhite, 0.0f);
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
            drawImage(dImg, 200 - (dW / 2.0f) + get3DOffset(10.0f), diffY - (dH / 2.0f), 0.85f);
            
            if (aLeftFrame && aLeftFrame->tex) {
                float aH = frameLogicalH(*aLeftFrame);
                drawFrameAt(*aLeftFrame, 200 - (dW / 2.0f) - 35 + get3DOffset(10.0f), diffY - (aH / 2.0f), 0.85f);
            }
            if (aRightFrame && aRightFrame->tex) {
                float aW = frameLogicalW(*aRightFrame);
                float aH = frameLogicalH(*aRightFrame);
                drawFrameAt(*aRightFrame, 200 + (dW / 2.0f) + 35 - aW + get3DOffset(10.0f), diffY - (aH / 2.0f), 0.85f);
            }
        } else {
            AddText("< " + curDiffStr + " >", 200 + get3DOffset(10.0f), diffY, 0.6f, true, 0.0f, CWhite, 0.0f);
        }
    }
 
    C2D_SceneBegin(bottom);
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
 
    if (!selectableWeeks.empty()) {
        std::string weekName = selectableWeeks[curSelected];
        WeekData& data = WeekData::weeksLoaded[weekName];
 
        u32 tracksColor = C2D_Color32(0xE5, 0x57, 0x77, 255);
        // TRACKS
        if (tracksImg.tex) {
            drawImageScaled(tracksImg, 30.0f, 32.0f, 1.0f, 0.7f, 0.7f);
        } else {
            AddText("TRACKS", 30, 30, 0.65f, false, 0.0f, tracksColor, 0.0f);
        }

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

            // Flicker
            if (selectedWeek && isSelected && !flickerVisible) continue;

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
                    C2D_AlphaImageTint(&tint, 0.6f);
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
                u32 textCol = isSelected ? CWhite : C2D_Color32(110, 110, 110, 154); // 154 ≈ 0.6*255
                std::string weekDisplayName = WeekData::weeksLoaded[selectableWeeks[i]].weekName;
                AddText(weekDisplayName, listX, itemY, isSelected ? 0.65f : 0.45f, true, isSelected ? 2.0f : 0.0f, textCol, 0.0f);
            }
        }

    }
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
    // Stop background thread
    threadRunning = false;
    LightEvent_Signal(&loadEvent);
    if (loadThread) {
        threadJoin(loadThread, U64_MAX);
        threadFree(loadThread);
        loadThread = nullptr;
    }

    // Free any unconsumed raw buffers in the result queue
    LightLock_Lock(&loadLock);
    for (auto& r : resultQueue) if (r.buffer) linearFree(r.buffer);
    resultQueue.clear();
    LightLock_Unlock(&loadLock);

    // Free sliding-window week sheets
    for (auto& p : weekSheets) if (p.second) Paths_freeSpriteSheet(p.second);
    weekSheets.clear();
    if (activeBgSheet)   { Paths_freeSpriteSheet(activeBgSheet);   activeBgSheet   = nullptr; }
    if (activeDiffSheet) { Paths_freeSpriteSheet(activeDiffSheet); activeDiffSheet = nullptr; }

    if (uiSheet)     C2D_SpriteSheetFree(uiSheet);
    if (tracksSheet) { C2D_SpriteSheetFree(tracksSheet); tracksSheet = nullptr; }

    C2D_TextBufDelete(vcrFontBuf);
}

// ── Simplified getters: return from cache; draw() shows spinner if null ────

C2D_Image StoryMenuState::getWeekBackgroundImage(const std::string& name) {
    if (name == activeBgName && activeBgSheet)
        return C2D_SpriteSheetGetImage(activeBgSheet, 0);
    return {nullptr, nullptr};
}

C2D_Image StoryMenuState::getWeekImage(const std::string& name) {
    for (int i = 0; i < (int)selectableWeeks.size(); i++) {
        if (selectableWeeks[i] == name) {
            auto it = weekSheets.find(i);
            if (it != weekSheets.end() && it->second)
                return C2D_SpriteSheetGetImage(it->second, 0);
            break;
        }
    }
    return {nullptr, nullptr};
}

C2D_Image StoryMenuState::getDiffImage(const std::string& name) {
    if (name == activeDiffName && activeDiffSheet)
        return C2D_SpriteSheetGetImage(activeDiffSheet, 0);
    return {nullptr, nullptr};
}

// ── Background thread ──────────────────────────────────────────────────────

void StoryMenuState::threadMain(void* arg) {
    StoryMenuState* state = (StoryMenuState*)arg;
    while (state->threadRunning) {
        LightLock_Lock(&state->loadLock);
        if (state->requestQueue.empty()) {
            LightLock_Unlock(&state->loadLock);
            LightEvent_Wait(&state->loadEvent);
            continue;
        }
        AsyncLoadRequest req = state->requestQueue.front();
        state->requestQueue.pop_front();
        LightLock_Unlock(&state->loadLock);

        // fread with the pre-resolved path (safe from secondary thread)
        LoadedResult result;
        result.type      = req.type;
        result.weekIndex = req.weekIndex;

        FILE* f = fopen(req.resolvedPath.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = (size_t)ftell(f);
            fseek(f, 0, SEEK_SET);
            void* buf = linearAlloc(size);
            if (buf) {
                if (fread(buf, 1, size, f) == size) {
                    GSPGPU_FlushDataCache(buf, size);
                    result.buffer = buf;
                    result.size   = size;
                } else {
                    linearFree(buf);
                }
            }
            fclose(f);
        }

        LightLock_Lock(&state->loadLock);
        state->resultQueue.push_back(result);
        if (req.type == AsyncLoadRequest::WEEK_BANNER)
            state->pendingWeekIndices.erase(req.weekIndex);
        LightLock_Unlock(&state->loadLock);

        // Small yield between items so we don't starve the main thread
        svcSleepThread(500000LL); // 0.5 ms
    }
}

// ── triggerWindowLoad: sliding window ±3 week banners + background ─────────

void StoryMenuState::triggerWindowLoad() {
    int n = (int)selectableWeeks.size();
    if (n == 0) return;

    // 1. Compute desired window
    int lo = std::max(0, curSelected - 3);
    int hi = std::min(n - 1, curSelected + 3);

    // 2. Free sheets outside the window
    std::vector<int> toErase;
    for (auto& p : weekSheets) {
        if (p.first < lo || p.first > hi) {
            if (p.second) Paths_freeSpriteSheet(p.second);
            toErase.push_back(p.first);
        }
    }
    for (int idx : toErase) weekSheets.erase(idx);

    // 3. Cancel queued week-banner requests outside window
    LightLock_Lock(&loadLock);
    requestQueue.erase(
        std::remove_if(requestQueue.begin(), requestQueue.end(),
            [&](const AsyncLoadRequest& r) {
                return r.type == AsyncLoadRequest::WEEK_BANNER &&
                       (r.weekIndex < lo || r.weekIndex > hi);
            }),
        requestQueue.end());
    // Rebuild pendingWeekIndices from what remains
    pendingWeekIndices.clear();
    for (auto& r : requestQueue)
        if (r.type == AsyncLoadRequest::WEEK_BANNER)
            pendingWeekIndices.insert(r.weekIndex);
    LightLock_Unlock(&loadLock);

    // 4. Queue background if week changed
    {
        std::string bgName;
        if (curSelected >= 0 && curSelected < n) {
            WeekData& wd = WeekData::weeksLoaded[selectableWeeks[curSelected]];
            ModHandler::get().currentModFolder = wd.isMod ? wd.modFolder : "";
            bgName = wd.weekBackground;
        }
        ModHandler::get().currentModFolder = "";

        if (bgName != activeBgName) {
            activeBgName = bgName;
            if (activeBgSheet) { Paths_freeSpriteSheet(activeBgSheet); activeBgSheet = nullptr; }
            if (!bgName.empty()) {
                std::string p = Paths::image("menubackgrounds/" + bgName);
                if (!Paths::fileExists(p)) p = Paths::image("menubackgrounds/placeholder");
                if (Paths::fileExists(p)) {
                    AsyncLoadRequest req;
                    req.type         = AsyncLoadRequest::BACKGROUND;
                    req.resolvedPath = p;
                    // Cancel previous bg request and push new one at front (high priority)
                    LightLock_Lock(&loadLock);
                    requestQueue.erase(
                        std::remove_if(requestQueue.begin(), requestQueue.end(),
                            [](const AsyncLoadRequest& r){ return r.type == AsyncLoadRequest::BACKGROUND; }),
                        requestQueue.end());
                    requestQueue.push_front(req);
                    LightLock_Unlock(&loadLock);
                }
            }
        }
    }

    // 5. Queue week banners in priority order: sel, sel-1, sel+1, sel-2, sel+2, sel-3, sel+3
    std::vector<int> order;
    order.push_back(curSelected);
    for (int delta = 1; delta <= 3; delta++) {
        if (curSelected - delta >= lo) order.push_back(curSelected - delta);
        if (curSelected + delta <= hi) order.push_back(curSelected + delta);
    }

    bool queued = false;
    LightLock_Lock(&loadLock);
    for (int idx : order) {
        if (weekSheets.count(idx) || pendingWeekIndices.count(idx)) continue;
        const std::string& weekName = selectableWeeks[idx];
        WeekData& wd = WeekData::weeksLoaded[weekName];
        ModHandler::get().currentModFolder = wd.isMod ? wd.modFolder : "";
        std::string p = Paths::image("storymenu/" + weekName);
        if (!Paths::fileExists(p)) p = Paths::image("storymenu/placeholder");
        ModHandler::get().currentModFolder = "";
        if (Paths::fileExists(p)) {
            AsyncLoadRequest req;
            req.type         = AsyncLoadRequest::WEEK_BANNER;
            req.weekIndex    = idx;
            req.resolvedPath = p;
            requestQueue.push_back(req);
            pendingWeekIndices.insert(idx);
            queued = true;
        }
    }
    LightLock_Unlock(&loadLock);

    if (queued) LightEvent_Signal(&loadEvent);
}

// ── triggerDiffLoad: load difficulty sprite for current week ───────────────

void StoryMenuState::triggerDiffLoad() {
    if (curWeekDiffs.empty()) return;

    std::string diffStr = curWeekDiffs[curDifficulty];
    std::string lower = diffStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == activeDiffName) return;

    activeDiffName = lower;
    if (activeDiffSheet) { Paths_freeSpriteSheet(activeDiffSheet); activeDiffSheet = nullptr; }

    // Resolve path on main thread (opendir is not safe from worker)
    int n = (int)selectableWeeks.size();
    if (n > 0 && curSelected >= 0 && curSelected < n) {
        WeekData& wd = WeekData::weeksLoaded[selectableWeeks[curSelected]];
        ModHandler::get().currentModFolder = wd.isMod ? wd.modFolder : "";
    }
    std::string p = Paths::image("menudifficulties/" + lower);
    if (!Paths::fileExists(p)) p = Paths::image("menudifficulties/placeholder");
    ModHandler::get().currentModFolder = "";

    if (Paths::fileExists(p)) {
        AsyncLoadRequest req;
        req.type         = AsyncLoadRequest::DIFFICULTY;
        req.resolvedPath = p;
        // Cancel previous diff request, push at front
        LightLock_Lock(&loadLock);
        requestQueue.erase(
            std::remove_if(requestQueue.begin(), requestQueue.end(),
                [](const AsyncLoadRequest& r){ return r.type == AsyncLoadRequest::DIFFICULTY; }),
            requestQueue.end());
        requestQueue.push_front(req);
        LightLock_Unlock(&loadLock);
        LightEvent_Signal(&loadEvent);
    }
}
