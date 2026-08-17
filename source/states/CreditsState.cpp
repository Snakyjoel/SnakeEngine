#include "CreditsState.hpp"
#include "MainMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "SparrowParser.hpp"
#include "../backend/stb_image.h"
#include "../backend/SpritesheetCache.hpp"
#include "../objects/Alphabet.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <math.h>

struct RawTexHeader {
    char magic[4];
    uint16_t width;
    uint16_t height;
    uint16_t origW;
    uint16_t origH;
};

void CreditsState::init() {
    VCRFontFix();
    
    // Load built-in credits icons spritesheet
    std::string sheetPath = "romfs:/preload/images/menus/creditsIcons.t3x";
    std::string xmlPath = "romfs:/preload/images/menus/creditsIcons.xml";
    iconSheet = C2D_SpriteSheetLoad(sheetPath.c_str());
    if (!iconSheet) {
        sheetPath = "romfs:/preload/images/menus/creditsIcons.png";
        iconSheet = C2D_SpriteSheetLoad(sheetPath.c_str());
    }

    if (iconSheet) {
        SparrowParser::parseXml(xmlPath, iconFrames);
        C2D_Image mainImg = C2D_SpriteSheetGetImage(iconSheet, 0);
        if (mainImg.tex) C3D_TexSetFilter(mainImg.tex, GPU_LINEAR, GPU_LINEAR);
        float rw = mainImg.subtex->right - mainImg.subtex->left;
        float rh = mainImg.subtex->bottom - mainImg.subtex->top;

        for (auto& f : iconFrames) {
            f.tex = mainImg.tex;
            f.uv.width = (u16)f.w;
            f.uv.height = (u16)f.h;
            f.uv.left = mainImg.subtex->left + ((float)f.x * rw / (float)mainImg.subtex->width);
            f.uv.top = mainImg.subtex->top + ((float)f.y * rh / (float)mainImg.subtex->height);
            f.uv.right = mainImg.subtex->left + ((float)(f.x + f.w) * rw / (float)mainImg.subtex->width);
            f.uv.bottom = mainImg.subtex->top + ((float)(f.y + f.h) * rh / (float)mainImg.subtex->height);
        }
    }

    // Set up standard built-in groups
    {
        CreditsGroup base;
        base.name = "Friday Night Funkin'";
        base.iconFrame = "baseGame";
        base.isMod = false;
        
        auto addEntry = [&](const std::string& header, const std::string& body) {
            CreditEntry e;
            e.isTitle = true;
            e.text1 = header;
            base.entries.push_back(e);
            
            e.isTitle = false;
            e.text1 = body;
            e.text2 = "";
            base.entries.push_back(e);
        };

        addEntry("Friday Night Funkin'", "A video game created by\nThe Funkin' Crew Inc.");
        addEntry("The Funkin' Crew Inc. Shareholders", "ninjamuffin99\nPhantomArcade\nKawai Sprite\nevilsk8r");
        addEntry("Direction and Art Lead", "PhantomArcade");
        addEntry("Music Lead", "Isaac “Kawai Sprite” Garcia");
        addEntry("Co-Direction and Programming Lead", "ninjamuffin99");
        addEntry("Mobile Lead", "MoonDroid (Zack)");
        addEntry("Production Manager", "Hundrec");
        addEntry("Team Organizers", "Hundrec\nAbnormalPoof");
        addEntry("Producer", "Kawa Teaño");
        addEntry("Artists", "PhantomArcade\nevilsk8r\nbeck");
        addEntry("Pixel Art", "moawling\nIGJHSpritin");
        addEntry("Cutscene Storyboards & SFX", "PhantomArcade");
        addEntry("Additional Background Design", "Red Minus");
        addEntry("Cutscene Animation", "Figburn\nSade\nTopium\nBlairTheUnseriousGuy");
        addEntry("Cutscene Cleanup", "PennilessRagamuffin\nbeck");
        addEntry("Cutscene Background Art", "beck");
        addEntry("Additional Art", "Jeff Bandelin\nMogy64\nChipsGoWoah\nMin Ho Kim (Deegeemin)\nPKettles\npeepo173");
        addEntry("Additional Character Design", "Tom Fulp - Pico School Characters\nJohnnyUtah - Tankman\nSrPelo - Skid and Pump\nMagna - Otis\ngacktenzo - Preppy Otis");
        addEntry("Music Production", "Saruky\ncrisp");
        addEntry("Featured Guest Musicians (thus far)", "Bassetfilms\nKohta Takahashi\nLotus Juice\nMETAROOM\nnuphory\nSaster\nsix impala\nTeraVex\nThat Andy Guy\ntsuyunoshi\nXploshi\nTee Lopes\nRRThiel");
        addEntry("Programming", "Eric \"EliteMasterEric\" Myllyoja\nfabs\nKadeDev");
        addEntry("Additional Programming", "Jenny Crowe\nember ana\nMike Welsh\nSaharan\nIan Harrigan\nOsaka Red LLC: Thomas J Webb\nEmma (MtH)\nGeorge Kurelic\nWill Blanton\nVictor - Cheemsandfriends\nHundrec\nAbnormalPoof\nMaybeMaru");
        addEntry("Mobile Porting", "MAJigsaw77\nLuckydog7\nKarim Akra\nsector_5");
        addEntry("Devops and Additional Internal Tooling", "ember ana");
        addEntry("Gameplay Design", "PhantomArcade\nCameron Taylor\nJenny Crowe\nSpazkid\nfabs\nEmma (MtH)");
        addEntry("Kickstarter Backer Portal Programming", "Shingai Shamu");
        addEntry("Merchandise Partners and Designers", "Needlejuice Records: Jace McLain\nNeedlejuice Records: Brandon Brown\nType-4: Coby Win\nIvanAlmighty\nMogy64\nChipsGoWoah\nMin Ho Kim\nPKettles\nJeff Bandelin\nPhantomArcade\nevilsk8r\nbeck\nMakeship: Seebs\nMakeship: Anna N");
        addEntry("Production and Business Development Partner - Windflower Games", "Sunni Pavlovic\nKristen Lynch");
        addEntry("Additional Administrative Assistance", "moawling");
        addEntry("Quality Assurance - Indium Play", "Lead Tester: Mihajlo Vuković\nTester: Andrej Naumovski\nDajana Dimovska");
        addEntry("Accounting: Molinari Oswald", "Francis Molinari\nAaron Hofmann\nKatherine Stauffer\nJane Haring");
        addEntry("US Legal: Odin Law", "Brandon Huffman\nMichele Robichaux\nConnor Richards\nPam Driver\nJacob Barefoot");
        addEntry("CA Legal: DLA Piper", "Ryan Black\nBrian Wong");
        addEntry("Special Thanks", "Tom Fulp\nJeff Bandelin\nThe entire Molinari Oswald Crew\nThe entire Odin Law function\nSrPelo");
        addEntry("Cameron would like to specially thank", "henry, snackers, digi, joemega, caddy, pewpew\nmilkhead jack\nkatt\narko, pepe, cashu, ookiyo\nKrystin, Kaye-lyn, and Cassidy, Mack, Levi, and Jasmine.\nLaurel\nClone Hero\nInnersloth, Puffballs, Forest and Victoria\nStuffedWombat\nmmatt_ugh\nlucas and jack taterguy and marty emrox\nLuis\nGeoKureli, Will Blanton, Austin East, Squidly\nfizzd\nbbpanzu\nEtika\nFoamymuffin (insert travis scott lyrics here)\nSiIvaGunner\nFreddie Dredd");
        addEntry("Kawa would like to specially thank", "Alexei Pepers\nXalavier Nelson Jr.");
        addEntry("Eric would like to specially thank", "Rob and Jill Myllyoja\nKadeDev\nShadow Mario");
        addEntry("Hazel (Ravy) would like to specially thank", "d1ggo");
        addEntry("Mobile Team special thanks", "cub, setai\nGalacticBaguette, Yowze, Snovi\nAguaCrunch, pb_lauro, Rulet, Rusron, Megalo_palewhite, Serizyu\n8-bitryan\nStax, NoraYotsu, AndroidSharky, IdioticLuwuke\nPeppyWall, Klavier, Roadr, Limon\nKoniro, Key, Zuki, LunaMyria, Rattatuwu, ToffeeCaramel\nNinkey, Snak, Codist\nValenPratama\nyetet (June), IDontCareAbtKaz\nSchepka\nAmari, DatRand (Vlad), Ressu2, Kekkra, CaptainRoku\ncat (Ariel), deathgobrr\nMario Master (MasterX)\nrichTrash21, PurSnake, Naisonji, HopKa, Matr4ss\nRedar13, Sirox, Shufa, D.Dregz, Sodaree\ndUmer, G0lda, Voodoo, Vemer, Sadshrimp");

        groups.push_back(base);
    }
    {
        CreditsGroup psych;
        psych.name = "Psych Engine (From Version 0.6.3)";
        psych.iconFrame = "psychEngine";
        psych.isMod = false;
        
        auto addTitle = [&](const std::string& title) {
            CreditEntry e;
            e.isTitle = true;
            e.text1 = title;
            psych.entries.push_back(e);
        };

        auto addEntry = [&](const std::string& body, const std::string& sub = "") {
            CreditEntry e;
            e.isTitle = false;
            e.text1 = body;
            e.text2 = sub;
            psych.entries.push_back(e);
        };

        addTitle("Psych Engine Team");
        addEntry("Shadow Mario", "Main Programmer of Psych Engine");
        addEntry("RiverOaken", "Main Artist/Animator of Psych Engine");
        addEntry("shubs", "Additional Programmer of Psych Engine");
        
        addTitle("Former Engine Members");
        addEntry("bb-panzu", "Ex-Programmer of Psych Engine");

        addTitle("Engine Contributions");
        addEntry("iFlicky", "Composer of Psync and Tea Time\nMade the Dialogue Sounds");
        addEntry("SquirraRNG", "Crash Handler and Base code for\nChart Editor's Wavefrom");
        addEntry("EliteMasterEric", "Runtime Shaders support");
        addEntry("PolybiusProxy", ".MP4 Video Loader Library (HxCodec)");
        addEntry("KadeDev", "Fixed some cool stuff on Chart Editor\nand other PRs");
        addEntry("Keioki", "Note Splash Animations");
        addEntry("Nebula the Zorua", "LUA JIT Fork and some Lua reworks");
        addEntry("Smokey", "Sprite Atlas support");

        groups.push_back(psych);
    }

    {
        CreditsGroup snake;
        snake.name = "Snake Engine";
        snake.iconFrame = "snakeEngine";
        snake.isMod = false;
        
        auto addTitle = [&](const std::string& title) {
            CreditEntry e;
            e.isTitle = true;
            e.text1 = title;
            snake.entries.push_back(e);
        };

        auto addEntry = [&](const std::string& body, const std::string& sub = "") {
            CreditEntry e;
            e.isTitle = false;
            e.text1 = body;
            e.text2 = sub;
            snake.entries.push_back(e);
        };

        addTitle("Snake Engine");
        addEntry("Made by:", "SnakyJoel");
        
        addTitle("Special thanks to:");
        addEntry("Psych Engine team", "Engine on which the port was based");
        addEntry("Friday Night Funkin' team", "Original creators of FNF");
        addEntry("Luc", "Artist of storymode banners");
        addEntry("Elitra090", "Helped with porting the weeks");
        addEntry("Natexs", "Pong game used as a template");
        addEntry("Cocottyna", "Artist of the old engine icon and banner");
        addEntry("AweSamdudeVR", "Helped with the menu background");
        addEntry("EduMakesStuff91", "Concept for freeplay menu");
        addEntry("Extintor and Chedar", "Helped in the development of the old Unity edition of the engine");
        addEntry("Joako_jp and cutefoxpuppy", "Helped with week end 1");
        addEntry("GameCrafterDev", "Helped with spritesheet optimization");
        addEntry("Oliwierpl", "Suggested the system lua functions");
        addEntry("Mimikitty3", "Composed the home menu sound");

        

        groups.push_back(snake);
    }

    ModHandler::get().scanMods();
    auto& activeMods = ModHandler::get().getMods();
    for (const auto& mod : activeMods) {
        std::string modPath = std::string("sdmc:/SnakeEngine/") + mod.folder;
        std::string creditsPath = modPath + "/data/credits.txt";
        if (Paths::fileExists(creditsPath)) {
            CreditsGroup modGrp;
            modGrp.name = mod.name;
            modGrp.isMod = true;
            modGrp.modFolder = mod.folder;
            
            std::string music1 = modPath + "/music/freeplayAndCredits.ogg";
            std::string music2 = modPath + "/sounds/freeplayAndCredits.ogg";
            if (Paths::fileExists(music1)) modGrp.musicPath = music1;
            else if (Paths::fileExists(music2)) modGrp.musicPath = music2;

            parseCreditsFile(modGrp, creditsPath);
            groups.push_back(modGrp);
        }
    }

    SpritesheetCache::get().load("shared/images/Alphabet");

    curSelected = 0;
    subState = STATE_SELECTING;
    musicPlaying = false;
    
    std::string bgPath = "romfs:/shared/images/menuBG.t3x";
    if (Paths::fileExists(bgPath)) {
        bgSheet = C2D_SpriteSheetLoad(bgPath.c_str());
        if (bgSheet) {
            topBG = C2D_SpriteSheetGetImage(bgSheet, 0);
            if (topBG.tex) C3D_TexSetFilter(topBG.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }
    
    std::string bgbPath = "romfs:/shared/images/menuBGB.t3x";
    if (Paths::fileExists(bgbPath)) {
        bottomBGSheet = C2D_SpriteSheetLoad(bgbPath.c_str());
        if (bottomBGSheet) {
            bottomBG = C2D_SpriteSheetGetImage(bottomBGSheet, 0);
            if (bottomBG.tex) C3D_TexSetFilter(bottomBG.tex, GPU_LINEAR, GPU_LINEAR);
        }
    }
    
    updateIconCache();
}

void CreditsState::parseCreditsFile(CreditsGroup& group, const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        
        std::string trimmed = line.substr(first);
        if (trimmed.empty()) continue;

        size_t split = trimmed.find("::");
        if (split != std::string::npos) {
            std::vector<std::string> parts;
            std::string temp = trimmed;
            size_t pos = 0;
            while ((pos = temp.find("::")) != std::string::npos) {
                parts.push_back(temp.substr(0, pos));
                temp.erase(0, pos + 2);
            }
            parts.push_back(temp);

            if (parts.size() >= 3) {
                CreditEntry e;
                e.isTitle = false;
                e.text1 = parts[0];
                e.text2 = parts[2];
                group.entries.push_back(e);
            }
        } else {
            CreditEntry e;
            e.isTitle = true;
            e.text1 = trimmed;
            group.entries.push_back(e);
        }
    }
}

void CreditsState::loadModIcon(CreditsGroup& group) {
    if (group.iconLoaded) return;

    std::string basePath = std::string("sdmc:/SnakeEngine/") + group.modFolder + "/pack";
    std::string rawPath = basePath + ".rawtex";
    std::string t3xPath = basePath + ".t3x";
    std::string pngPath = basePath + ".png";
    std::string fallbackPath = "romfs:/preload/images/menus/noIcon.png";

    std::string targetPath = "";
    bool isPng = false;
    bool isT3x = false;
    bool isRaw = false;

    if (Paths::fileExists(rawPath)) {
        targetPath = rawPath;
        isRaw = true;
    } else if (Paths::fileExists(t3xPath)) {
        targetPath = t3xPath;
        isT3x = true;
    } else if (Paths::fileExists(pngPath)) {
        targetPath = pngPath;
        isPng = true;
    } else if (Paths::fileExists(fallbackPath)) {
        targetPath = fallbackPath;
        isPng = true;
    }

    if (isRaw) {
        FILE* f = fopen(targetPath.c_str(), "rb");
        if (f) {
            RawTexHeader header;
            if (fread(&header, sizeof(RawTexHeader), 1, f) == 1 && strncmp(header.magic, "RWTX", 4) == 0) {
                group.manualTex = new C3D_Tex();
                if (C3D_TexInit(group.manualTex, header.width, header.height, GPU_RGBA8)) {
                    C3D_TexSetFilter(group.manualTex, GPU_LINEAR, GPU_LINEAR);
                    
                    size_t dataSize = (size_t)header.width * header.height * 4;
                    void* data = linearAlloc(dataSize);
                    if (data) {
                        fread(data, dataSize, 1, f);
                        C3D_TexUpload(group.manualTex, data);
                        C3D_TexFlush(group.manualTex);
                        linearFree(data);
                        
                        group.manualSub = new Tex3DS_SubTexture();
                        group.manualSub->width = header.origW; group.manualSub->height = header.origH;
                        group.manualSub->left = 0.0f; group.manualSub->top = 1.0f;
                        group.manualSub->right = (float)header.origW / header.width;
                        group.manualSub->bottom = 1.0f - ((float)header.origH / header.height);
                        
                        group.modIcon.tex = group.manualTex;
                        group.modIcon.subtex = group.manualSub;
                    } else {
                        delete group.manualTex;
                        group.manualTex = nullptr;
                    }
                } else {
                    delete group.manualTex;
                    group.manualTex = nullptr;
                }
            }
            fclose(f);
        }
    } else if (isT3x) {
        group.modSheet = C2D_SpriteSheetLoad(targetPath.c_str());
        if (group.modSheet) {
            group.modIcon = C2D_SpriteSheetGetImage(group.modSheet, 0);
        }
    } else if (isPng) {
        int w, h, c;
        unsigned char* data = stbi_load(targetPath.c_str(), &w, &h, &c, 4);
        if (data) {
            int pw = 1, ph = 1;
            while(pw < w) pw *= 2;
            while(ph < h) ph *= 2;

            group.manualTex = new C3D_Tex();
            if (C3D_TexInit(group.manualTex, pw, ph, GPU_RGBA8)) {
                C3D_TexSetFilter(group.manualTex, GPU_LINEAR, GPU_LINEAR);
                
                uint32_t* swizzled = (uint32_t*)linearAlloc(pw * ph * 4);
                if (swizzled) {
                    memset(swizzled, 0, pw * ph * 4);
                    
                    for(int y=0; y<h; y++) {
                        for(int x=0; x<w; x++) {
                            int src = (y*w+x)*4;
                            uint32_t px = (data[src]<<24)|(data[src+1]<<16)|(data[src+2]<<8)|data[src+3];
                            uint32_t i = (x & 7) | ((y & 7) << 8);
                            i = (i ^ (i << 2)) & 0x1313;
                            i = (i ^ (i << 1)) & 0x1515;
                            
                            uint32_t tx = x >> 3;
                            uint32_t ty = y >> 3;
                            uint32_t tile_start = (ty * (pw >> 3) + tx) << 6;
                            uint32_t local_idx = (i & 0xFF) | (((i >> 8) & 0xFF) << 1);
                            
                            swizzled[tile_start + local_idx] = px;
                        }
                    }
                    C3D_TexUpload(group.manualTex, swizzled);
                    C3D_TexFlush(group.manualTex);
                    linearFree(swizzled);

                    group.manualSub = new Tex3DS_SubTexture();
                    group.manualSub->width = w; group.manualSub->height = h;
                    group.manualSub->left = 0.0f; group.manualSub->top = 1.0f;
                    group.manualSub->right = (float)w / pw; group.manualSub->bottom = 1.0f - ((float)h / ph);

                    group.modIcon.tex = group.manualTex;
                    group.modIcon.subtex = group.manualSub;
                } else {
                    delete group.manualTex;
                    group.manualTex = nullptr;
                }
            } else {
                delete group.manualTex;
                group.manualTex = nullptr;
            }
            stbi_image_free(data);
        }
    }
    
    group.iconLoaded = true;
}

void CreditsState::freeModIcon(CreditsGroup& group) {
    if (!group.iconLoaded) return;
    
    if (group.manualTex) {
        C3D_TexDelete(group.manualTex);
        delete group.manualTex;
        group.manualTex = nullptr;
    }
    if (group.manualSub) {
        delete group.manualSub;
        group.manualSub = nullptr;
    }
    if (group.modSheet) {
        C2D_SpriteSheetFree(group.modSheet);
        group.modSheet = nullptr;
    }
    group.modIcon.tex = nullptr;
    group.modIcon.subtex = nullptr;
    group.iconLoaded = false;
}

void CreditsState::updateIconCache() {
    int count = (int)groups.size();
    if (count == 0) return;

    for (int i = 0; i < count; i++) {
        if (groups[i].isMod) {
            float diff = (float)i - scrollPercent;
            while (diff < -count / 2.0f) diff += count;
            while (diff > count / 2.0f) diff -= count;

            if (std::abs(diff) <= 1.8f) {
                loadModIcon(groups[i]);
            } else {
                freeModIcon(groups[i]);
            }
        }
    }
}

static float getEntryHeight(const CreditEntry& entry) {
    if (entry.isTitle) {
        int lines = 0;
        std::stringstream ss(entry.text1);
        std::string para;
        while (std::getline(ss, para, '\n')) {
            lines += 1 + (int)(para.length() / 22);
        }
        return (float)lines * 20.0f + 25.0f;
    }
    
    int lines1 = 0;
    {
        std::stringstream ss(entry.text1);
        std::string para;
        while (std::getline(ss, para, '\n')) {
            lines1 += 1 + (int)(para.length() / 32);
        }
    }
    float height = (float)lines1 * 14.0f;
    
    if (!entry.text2.empty()) {
        int lines2 = 0;
        std::stringstream ss(entry.text2);
        std::string para;
        while (std::getline(ss, para, '\n')) {
            lines2 += 1 + (int)(para.length() / 42);
        }
        height += (float)lines2 * 11.0f + 6.0f;
    }
    return height + 24.0f; // margins
}

void CreditsState::update(float dt) {
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    int count = (int)groups.size();
    
    if (subState == STATE_SELECTING) {
        int oldSelected = curSelected;
        
        if (kDown & (KEY_DUP | KEY_CPAD_UP)) {
            curSelected--;
            if (curSelected < 0) curSelected = count - 1;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
            curSelected++;
            if (curSelected >= count) curSelected = 0;
            AudioEngine::playSound("romfs:/preload/sounds/scrollMenu.ogg", 0.7f);
        }
        
        if (count > 0 && oldSelected != curSelected) {
            if (curSelected - oldSelected == 1 - count) {
                scrollPercent -= count;
            } else if (curSelected - oldSelected == count - 1) {
                scrollPercent += count;
            }
        }

        if (kDown & KEY_B) {
            AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            switchState(new MainMenuState());
            return;
        }

        if (kDown & (KEY_A | KEY_START)) {
            AudioEngine::playSound("romfs:/preload/sounds/confirmMenu.ogg", 0.7f);
            subState = STATE_SCROLLING;
            
            scrollY = 480.0f;
            
            totalScrollHeight = 0.0f;
            for (const auto& entry : groups[curSelected].entries) {
                totalScrollHeight += getEntryHeight(entry);
            }
            totalScrollHeight += 150.0f; // Ending margin

            std::string specialMusic = groups[curSelected].musicPath;
            if (specialMusic.empty() && Paths::fileExists("romfs:/preload/music/freeplayAndCredits.ogg")) {
                specialMusic = "romfs:/preload/music/freeplayAndCredits.ogg";
            }

            if (!specialMusic.empty()) {
                MusicPlayer::stop();
                MusicPlayer::play(specialMusic.c_str(), 0.7f);
                musicPlaying = true;
            }
        }
    } 
    else if (subState == STATE_SCROLLING) {
        float speedMult = (kHeld & KEY_A) ? 4.0f : 1.0f;
        scrollY -= scrollSpeed * speedMult * dt;
        
        if (scrollY < -totalScrollHeight || (kDown & (KEY_B | KEY_START))) {
            if (kDown & (KEY_B | KEY_START)) {
                AudioEngine::playSound("romfs:/preload/sounds/cancelMenu.ogg", 0.7f);
            }
            subState = STATE_SELECTING;
            if (musicPlaying) {
                MusicPlayer::stop();
                MusicPlayer::playMenuMusic();
                musicPlaying = false;
            }
        }
    }

    if (count > 0) {
        scrollPercent += (curSelected - scrollPercent) * 12.0f * dt;
        
        if (scrollPercent < 0.0f) {
            scrollPercent += count;
        } else if (scrollPercent >= count) {
            scrollPercent -= count;
        }
        
        updateIconCache();
    }
}

void CreditsState::drawScrollText(const std::string& text, float x, float y, float scale, bool centered, float border, u32 color, float wrapWidth) {
    std::vector<std::string> paragraphs;
    std::string currentParagraph = "";
    for (char c : text) {
        if (c == '\n') {
            paragraphs.push_back(currentParagraph);
            currentParagraph = "";
        } else {
            currentParagraph += c;
        }
    }
    paragraphs.push_back(currentParagraph);

    std::vector<std::string> lines;
    for (const auto& para : paragraphs) {
        if (wrapWidth <= 0.0f || para.empty()) {
            lines.push_back(para);
        } else {
            std::stringstream ss(para);
            std::string word;
            std::string line = "";
            while (ss >> word) {
                std::string testLine = line.empty() ? word : line + " " + word;
                C2D_Text gText;
                C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, testLine.c_str());
                float tw, th;
                C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
                
                if (tw > wrapWidth && !line.empty()) {
                    lines.push_back(line);
                    line = word;
                } else {
                    line = testLine;
                }
            }
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
    }

    float lineHeight = 28.0f * scale;
    float totalHeight = lines.size() * lineHeight;
    float startY = centered ? (y - totalHeight / 2.0f) : y;

    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].empty()) continue;
        C2D_Text gText;
        C2D_TextFontParse(&gText, vcrFont, vcrFontBuf, lines[i].c_str());
        C2D_TextOptimize(&gText);
        float tw, th;
        C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
        
        float dx = centered ? (x - tw / 2.0f) : x;
        float dy = startY + (float)i * lineHeight;
        dx = std::round(dx); dy = std::round(dy);
        if (border > 0.0f) {
            DrawTextBorderFull(&gText, dx, dy, 0.84f, scale, scale, border, CBlack);
        }
        C2D_DrawText(&gText, C2D_WithColor, dx, dy, 0.85f, scale, scale, color);
    }
}

void CreditsState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    ClearTextBuf();

    if (subState == STATE_SELECTING) {
        C2D_SceneBegin(top);
        C2D_TargetClear(top, C2D_Color32(146, 113, 253, 255));
        
        if (bgSheet) {
            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, C2D_Color32(39, 71, 220, 255), 1.0f);
            drawCenteredBG(topBG, 400.0f, 240.0f, 0.1f, &tint);
        }
        
        Alphabet::draw("CREDITS MENU", 200.0f, 20.0f, 1.1f, 1.0f, true, CWhite);
        
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, C2D_Color32(146, 113, 253, 255));

        if (bottomBGSheet) {
            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, C2D_Color32(39, 71, 220, 255), 1.0f);
            drawCenteredBG(bottomBG, 320.0f, 240.0f, 0.1f, &tint);
        }

        if (!groups.empty()) {
            int count = (int)groups.size();
            
            auto drawIcon = [&](int idx, float y, float scale, float alpha) {
                auto& gp = groups[idx];
                C2D_Image img;
                const Frame* frame = nullptr;
                
                if (gp.isMod) {
                    if (gp.iconLoaded && gp.modIcon.tex) {
                        img = gp.modIcon;
                    } else {
                        return; // Not loaded
                    }
                } else {
                    // Find built-in icon frame matching the prefix
                    bool found = false;
                    for (const auto& f : iconFrames) {
                        if (f.name.find(gp.iconFrame) == 0) {
                            img.tex = f.tex;
                            img.subtex = &f.uv;
                            frame = &f;
                            found = true;
                            break;
                        }
                    }
                    if (!found) return;
                }
                
                C2D_ImageTint tint;
                C2D_AlphaImageTint(&tint, alpha);
                
                if (frame) {
                    drawFrameCentered(*frame, 60.0f, y, 0.5f, &tint, scale, scale);
                } else {
                    float w = img.subtex->width;
                    float h = img.subtex->height;
                    float drawX = 60.0f - (w * scale) / 2.0f;
                    float drawY = y - (h * scale) / 2.0f;
                    C2D_DrawImageAt(img, drawX, drawY, 0.5f, &tint, scale, scale);
                }
            };

            for (int i = 0; i < count; i++) {
                float diff = (float)i - scrollPercent;
                // Since it's vertical now, targetY wraps around the list selection
                float targetY = 120.0f + diff * 75.0f;
                
                if (targetY < -50.0f || targetY > 290.0f) continue;

                bool isSelected = (i == curSelected);
                float itemAlpha = isSelected ? 1.0f : 0.6f;
                float scale = isSelected ? 0.75f : 0.60f;

                drawIcon(i, targetY, scale, itemAlpha);

                float textHeight = 70.0f * 1.0f * (240.0f / 720.0f);
                CachedSpritesheet* alphabetSheet = SpritesheetCache::get().load("shared/images/Alphabet");
                if (alphabetSheet) {
                    for (const auto& f : alphabetSheet->frames) {
                        if (f.name == "A0000") {
                            textHeight = frameLogicalH(f) * 1.0f * (240.0f / 720.0f);
                            break;
                        }
                    }
                }
                float textY = targetY - textHeight / 2.0f;
                u32 color = C2D_Color32(255, 255, 255, (u8)(itemAlpha * 255.0f));
                
                Alphabet::draw(groups[i].name, 110.0f, textY, 1.0f, itemAlpha, false, color);
            }
        }
        C2D_Flush();
    }
    else if (subState == STATE_SCROLLING) {
        auto& gp = groups[curSelected];

        C2D_SceneBegin(top);
        C2D_TargetClear(top, CBlack);
        
        float currentY = scrollY;
        for (const auto& entry : gp.entries) {
            float entryHeight = getEntryHeight(entry);
            if (entry.isTitle) {
                if (currentY > -entryHeight && currentY < 260.0f) {
                    drawScrollText(entry.text1, 200.0f, currentY + entryHeight/2.0f, 0.65f, true, 0.0f, CYellow, 280.0f);
                }
            } else {
                if (currentY > -entryHeight && currentY < 260.0f) {
                    int lines = 0;
                    {
                        std::stringstream ss(entry.text1);
                        std::string para;
                        while (std::getline(ss, para, '\n')) {
                            lines += 1 + (int)(para.length() / 32);
                        }
                    }
                    float t1Height = (float)lines * 14.0f;
                    
                    if (entry.text2.empty()) {
                        drawScrollText(entry.text1, 200.0f, currentY + entryHeight/2.0f, 0.5f, true, 0.0f, CWhite, 280.0f);
                    } else {
                        float t1Center = currentY + 12.0f + t1Height/2.0f;
                        drawScrollText(entry.text1, 200.0f, t1Center, 0.5f, true, 0.0f, CWhite, 280.0f);
                        
                        int lines2 = 0;
                        {
                            std::stringstream ss(entry.text2);
                            std::string para;
                            while (std::getline(ss, para, '\n')) {
                                lines2 += 1 + (int)(para.length() / 42);
                            }
                        }
                        float t2Height = (float)lines2 * 11.0f;
                        float t2Center = currentY + 12.0f + t1Height + 6.0f + t2Height/2.0f;
                        drawScrollText(entry.text2, 200.0f, t2Center, 0.38f, true, 0.0f, CGray, 280.0f);
                    }
                }
            }
            currentY += entryHeight;
        }
        C2D_Flush();

        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, CBlack);

        currentY = scrollY - 240.0f;
        for (const auto& entry : gp.entries) {
            float entryHeight = getEntryHeight(entry);
            if (entry.isTitle) {
                if (currentY > -entryHeight && currentY < 260.0f) {
                    drawScrollText(entry.text1, 160.0f, currentY + entryHeight/2.0f, 0.65f, true, 0.0f, CYellow, 280.0f);
                }
            } else {
                if (currentY > -entryHeight && currentY < 260.0f) {
                    int lines = 0;
                    {
                        std::stringstream ss(entry.text1);
                        std::string para;
                        while (std::getline(ss, para, '\n')) {
                            lines += 1 + (int)(para.length() / 32);
                        }
                    }
                    float t1Height = (float)lines * 14.0f;
                    
                    if (entry.text2.empty()) {
                        drawScrollText(entry.text1, 160.0f, currentY + entryHeight/2.0f, 0.5f, true, 0.0f, CWhite, 280.0f);
                    } else {
                        float t1Center = currentY + 12.0f + t1Height/2.0f;
                        drawScrollText(entry.text1, 160.0f, t1Center, 0.5f, true, 0.0f, CWhite, 280.0f);
                        
                        int lines2 = 0;
                        {
                            std::stringstream ss(entry.text2);
                            std::string para;
                            while (std::getline(ss, para, '\n')) {
                                lines2 += 1 + (int)(para.length() / 42);
                            }
                        }
                        float t2Height = (float)lines2 * 11.0f;
                        float t2Center = currentY + 12.0f + t1Height + 6.0f + t2Height/2.0f;
                        drawScrollText(entry.text2, 160.0f, t2Center, 0.38f, true, 0.0f, CGray, 280.0f);
                    }
                }
            }
            currentY += entryHeight;
        }
        C2D_Flush();
    }
}

void CreditsState::exitState() {
    if (iconSheet) C2D_SpriteSheetFree(iconSheet);
    if (bgSheet) C2D_SpriteSheetFree(bgSheet);
    if (bottomBGSheet) C2D_SpriteSheetFree(bottomBGSheet);
    
    // Free all loaded mod icons
    for (auto& gp : groups) {
        freeModIcon(gp);
    }
    
    C2D_TextBufDelete(vcrFontBuf);
}
