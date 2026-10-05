#include "CreditsState.hpp"
#include "MainMenuState.hpp"
#include "../backend/AudioEngine.hpp"
#include "SparrowParser.hpp"
#include "../backend/stb_image.h"
#include "../backend/SpritesheetCache.hpp"
#include "../objects/Alphabet.hpp"
#include "../objects/ButtonPrompt.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

struct RawTexHeader {
    char magic[4];
    uint16_t width;
    uint16_t height;
    uint16_t origW;
    uint16_t origH;
};

static void drawRotatedRect(float cx, float cy, float w, float h, float angleRad, u32 color, float depth) {
    float c = cosf(angleRad), s = sinf(angleRad);
    float hw = w * 0.5f, hh = h * 0.5f;

    auto rot = [cx, cy, c, s](float dx, float dy, float& ox, float& oy) {
        ox = cx + dx * c - dy * s;
        oy = cy + dx * s + dy * c;
    };

    float x1, y1, x2, y2, x3, y3, x4, y4;
    rot(-hw, -hh, x1, y1);
    rot( hw, -hh, x2, y2);
    rot( hw,  hh, x3, y3);
    rot(-hw,  hh, x4, y4);

    C2D_DrawTriangle(x1, y1, color, x2, y2, color, x3, y3, color, depth);
    C2D_DrawTriangle(x1, y1, color, x3, y3, color, x4, y4, color, depth);
}

static int countParagraphLines(const std::string& str, int charsPerLine) {
    if (str.empty()) return 1;
    int lines = 0;
    size_t start = 0;
    while (start < str.length()) {
        size_t end = str.find('\n', start);
        if (end == std::string::npos) {
            lines += 1 + (int)((str.length() - start) / charsPerLine);
            break;
        }
        lines += 1 + (int)((end - start) / charsPerLine);
        start = end + 1;
    }
    return lines;
}

static float getEntryHeight(const CreditEntry& entry) {
    if (entry.isTitle) {
        int lines = countParagraphLines(entry.text1, 20);
        return (float)lines * 24.0f + 25.0f;
    }
    
    int lines1 = countParagraphLines(entry.text1, 26);
    float height = (float)lines1 * 18.0f;
    
    if (!entry.text2.empty()) {
        int lines2 = countParagraphLines(entry.text2, 34);
        height += (float)lines2 * 14.0f + 6.0f;
    }
    return height + 24.0f; // margins
}

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
        psych.name = "Psych Engine";
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
        addEntry("Elitra090", "Helped with porting and fixing");
        addEntry("Natexs", "Pong game used as a template");
        addEntry("Cocottyna", "Artist of the old engine icon and banner");
        addEntry("AweSamdudeVR", "Helped with the menu background");
        addEntry("EduMakesStuff91", "Concept for freeplay menu");
        addEntry("Extintor and Chedar", "Helped in the development of the old Unity edition of the engine");
        addEntry("Joako_jp and cutefoxpuppy", "Helped with week end 1");
        addEntry("GameCrafterDev", "Helped with spritesheet optimization");
        addEntry("Oliwierpl", "Suggested the system lua functions");
        addEntry("Mimikitty3", "Composed the home menu sound");
        addEntry("Fukita", "Fixed the sustains visuals");
        addEntry("Jon SpeedArts", "Ported the week 6");

        addTitle("TOP DONATORS, THANKS <3");
        addEntry("SG Lara", "US$ 28,08");

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

    quanticoFont = C2D_FontLoad("romfs:/fonts/Quantico-Bold.bcfnt");
    quanticoFontBuf = C2D_TextBufNew(4096);
    inconsolataFont = C2D_FontLoad("romfs:/fonts/Inconsolata-Black.bcfnt");
    inconsolataFontBuf = C2D_TextBufNew(4096);
    textScrollTime = 0.0f;
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
            size_t secondSplit = trimmed.find("::", split + 2);
            if (secondSplit != std::string::npos) {
                CreditEntry e;
                e.isTitle = false;
                e.text1 = trimmed.substr(0, split);
                e.text2 = trimmed.substr(secondSplit + 2);
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
            if (std::abs(diff) <= 5.8f) { // Sliding window caching
                loadModIcon(groups[i]);
            } else {
                freeModIcon(groups[i]);
            }
        }
    }
}

void CreditsState::update(float dt) {
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    int count = (int)groups.size();
    
    if (subState == STATE_SELECTING) {
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
        scrollPercent += ((float)curSelected - scrollPercent) * 12.0f * dt;
        updateIconCache();
    }
    textScrollTime += dt;
}

void CreditsState::drawScrollText(const std::string& text, float x, float y, float scale, bool centered, float border, u32 color, float wrapWidth) {
    C2D_Font fontToUse = inconsolataFont ? inconsolataFont : vcrFont;
    C2D_TextBuf bufToUse = inconsolataFontBuf ? inconsolataFontBuf : vcrFontBuf;

    std::vector<std::string> paragraphs;
    size_t start = 0;
    while (start <= text.length()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            paragraphs.push_back(text.substr(start));
            break;
        }
        paragraphs.push_back(text.substr(start, end - start));
        start = end + 1;
    }

    std::vector<std::string> lines;
    lines.reserve(paragraphs.size());
    for (const auto& para : paragraphs) {
        if (wrapWidth <= 0.0f || para.empty()) {
            lines.push_back(para);
        } else {
            std::string line = "";
            size_t pStart = 0;
            while (pStart < para.length()) {
                size_t pSpace = para.find(' ', pStart);
                std::string word = (pSpace == std::string::npos) ? para.substr(pStart) : para.substr(pStart, pSpace - pStart);
                pStart = (pSpace == std::string::npos) ? para.length() : pSpace + 1;
                if (word.empty()) continue;

                std::string testLine = line.empty() ? word : line + " " + word;
                C2D_Text gText;
                C2D_TextFontParse(&gText, fontToUse, bufToUse, testLine.c_str());
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
    float totalHeight = (float)lines.size() * lineHeight;
    float startY = centered ? (y - totalHeight * 0.5f) : y;

    for (size_t i = 0; i < lines.size(); i++) {
        if (lines[i].empty()) continue;
        C2D_Text gText;
        C2D_TextFontParse(&gText, fontToUse, bufToUse, lines[i].c_str());
        C2D_TextOptimize(&gText);
        float tw, th;
        C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
        
        float lineScale = scale;
        if (wrapWidth > 0.0f && tw > wrapWidth) {
            lineScale = scale * (wrapWidth / tw);
            C2D_TextGetDimensions(&gText, lineScale, lineScale, &tw, &th);
        }

        float dx = centered ? (x - tw * 0.5f) : x;
        float dy = startY + (float)i * lineHeight;
        dx = std::round(dx); dy = std::round(dy);
        if (border > 0.0f) {
            DrawTextBorderFull(&gText, dx, dy, 0.84f, lineScale, lineScale, border, CBlack);
        }
        C2D_DrawText(&gText, C2D_WithColor, dx, dy, 0.85f, lineScale, lineScale, color);
    }
}

static std::vector<std::string> wrapAlphabetText(const std::string& text, float scale, float maxWidth) {
    std::vector<std::string> lines;
    std::string currentLine = "";
    size_t start = 0;

    while (start < text.length()) {
        size_t spacePos = text.find(' ', start);
        std::string word = (spacePos == std::string::npos) ? text.substr(start) : text.substr(start, spacePos - start);
        start = (spacePos == std::string::npos) ? text.length() : spacePos + 1;
        if (word.empty()) continue;

        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        float tw = Alphabet::getTextWidth(testLine, scale);
        if (tw > maxWidth && !currentLine.empty()) {
            lines.push_back(currentLine);
            currentLine = word;
        } else {
            currentLine = testLine;
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }
    if (lines.empty()) {
        lines.push_back(text);
    }
    return lines;
}

static void drawGrid(float width, float height, float textScrollTime, float depth) {
    float size = 36.0f;
    float offset = fmodf(textScrollTime * 25.0f, size);
    u32 gridCol = C2D_Color32(0, 0, 0, 50);

    int xi = 0;
    for (float x = -size * 2.0f + offset; x < width + size; x += size, xi++) {
        int yi = 0;
        for (float y = -size * 2.0f + offset; y < height + size; y += size, yi++) {
            if ((xi + yi) & 1) continue;
            C2D_DrawRectSolid(x, y, depth, size, size, gridCol);
        }
    }
}

void CreditsState::draw(C3D_RenderTarget* top, C3D_RenderTarget* bottom) {
    C2D_SetTintMode(C2D_TintMult);
    ClearTextBuf();
    if (quanticoFontBuf) C2D_TextBufClear(quanticoFontBuf);
    if (inconsolataFontBuf) C2D_TextBufClear(inconsolataFontBuf);

    u32 bgColor = C2D_Color32(43, 0, 135, 255);
    float angleRad = -6.0f * (3.14159265f / 180.0f);

    if (subState == STATE_SELECTING) {
        // ── TOP SCREEN ────────────────────────────────────────────────────────
        C2D_SceneBegin(top);
        C2D_TargetClear(top, bgColor);
        
        // Infinite scrolling transparent black checkerboard grid
        drawGrid(400.0f, 240.0f, textScrollTime, 0.05f);

        if (bgSheet && topBG.tex) {
            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, C2D_Color32(255, 255, 255, 255), 1.0f);
            drawCenteredBG(topBG, 400.0f, 240.0f, 0.10f, &tint);
        }
        
        // Rotated black rectangle on left side of top screen
        drawRotatedRect(25.0f, 120.0f, 110.0f, 450.0f, angleRad, C2D_Color32(0, 0, 0, 255), 0.25f);

        // Vertical CREDITS text in Quantico-Bold font
        const char* creditsLetters[] = {"C", "R", "E", "D", "I", "T", "S", " "};
        float letterStep = 40.0f;
        float loopHeight = 8.0f * letterStep;
        float vScrollOffset = fmodf(textScrollTime * 15.0f, loopHeight);

        for (float baseY = -loopHeight; baseY < 270.0f; baseY += loopHeight) {
            for (int i = 0; i < 8; i++) {
                if (creditsLetters[i][0] == ' ') continue;
                float charY = baseY + vScrollOffset + (float)i * letterStep;
                if (charY > -60.0f && charY < 260.0f) {
                    C2D_Text letterObj;
                    C2D_TextFontParse(&letterObj, quanticoFont ? quanticoFont : vcrFont, quanticoFontBuf ? quanticoFontBuf : vcrFontBuf, creditsLetters[i]);
                    C2D_TextOptimize(&letterObj);
                    
                    // Shadow
                    DrawTextBorderFull(&letterObj, 18.5f, charY + 4.5f, 0.28f, 1.725f, 1.725f, 1.5f, C2D_Color32(50, 50, 50, 255));
                    C2D_DrawText(&letterObj, C2D_WithColor, 18.5f, charY + 4.5f, 0.285f, 1.725f, 1.725f, C2D_Color32(50, 50, 50, 255));

                    // Main border & text
                    DrawTextBorderFull(&letterObj, 12.0f, charY, 0.29f, 1.725f, 1.725f, 1.5f, CBlack);
                    C2D_DrawText(&letterObj, C2D_WithColor, 12.0f, charY, 0.30f, 1.725f, 1.725f, CWhite);
                }
            }
        }

        if (!groups.empty()) {
            int count = (int)groups.size();
            
            auto drawIcon = [&](int idx, float x, float y, float scale, float alpha) {
                auto& gp = groups[idx];
                C2D_Image img;
                const Frame* frame = nullptr;
                
                if (gp.isMod) {
                    if (gp.iconLoaded && gp.modIcon.tex) {
                        img = gp.modIcon;
                    } else {
                        return;
                    }
                } else {
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
                    drawFrameCentered(*frame, x, y, 0.5f, &tint, scale, scale);
                } else {
                    float w = img.subtex->width;
                    float h = img.subtex->height;
                    float drawX = x - (w * scale) * 0.5f;
                    float drawY = y - (h * scale) * 0.5f;
                    C2D_DrawImageAt(img, drawX, drawY, 0.5f, &tint, scale, scale);
                }
            };

            auto getIconWidth = [&](int idx, float scale) -> float {
                auto& gp = groups[idx];
                if (gp.isMod) {
                    if (gp.iconLoaded && gp.modIcon.tex && gp.modIcon.subtex) {
                        return gp.modIcon.subtex->width * scale;
                    }
                } else {
                    for (const auto& f : iconFrames) {
                        if (f.name.find(gp.iconFrame) == 0) {
                            return (float)f.uv.width * scale;
                        }
                    }
                }
                return 35.0f * scale;
            };

            for (int i = 0; i < count; i++) {
                float diff = (float)i - scrollPercent;
                float targetY = 120.0f + diff * 70.0f;
                if (targetY < -120.0f || targetY > 360.0f) continue;

                float absDiff = std::abs(diff);
                float t = std::max(0.0f, 1.0f - absDiff);
                t = t * t * (3.0f - 2.0f * t);

                float itemAlpha = 0.5f + t * 0.5f;
                float baseScale = 0.55f + t * 0.20f;
                const float nameScale = 0.8625f;
                float iconScale = baseScale * 0.80f;

                float textHeight = 70.0f * (240.0f / 720.0f);
                CachedSpritesheet* alphabetSheet = SpritesheetCache::get().load("shared/images/Alphabet");
                if (alphabetSheet) {
                    for (const auto& f : alphabetSheet->frames) {
                        if (f.name == "A0000") {
                            textHeight = frameLogicalH(f) * (240.0f / 720.0f);
                            break;
                        }
                    }
                }
                u32 color = C2D_Color32(255, 255, 255, (u8)(itemAlpha * 255.0f));

                float iconX = 340.0f;
                const float curSelectIconScale = 0.60f;
                float curSelectIconW = getIconWidth(i, curSelectIconScale);
                float curSelectIconLeft = iconX - curSelectIconW * 0.5f;
                float textRightX = curSelectIconLeft - 5.0f;
                float maxTextWidth = std::max(50.0f, textRightX - 40.0f);

                std::vector<std::string> lines = wrapAlphabetText(groups[i].name, nameScale, maxTextWidth);
                float lineHeight = textHeight * nameScale * 0.90f;
                float totalTextHeight = (float)lines.size() * lineHeight;
                float startTextY = targetY - totalTextHeight * 0.5f;

                for (size_t l = 0; l < lines.size(); l++) {
                    float lineW = Alphabet::getTextWidth(lines[l], nameScale);
                    float lineX = textRightX - lineW;
                    float lineY = startTextY + (float)l * lineHeight;
                    Alphabet::draw(lines[l], lineX, lineY, nameScale, itemAlpha, false, color);
                }

                drawIcon(i, iconX, targetY, iconScale, itemAlpha);
            }
        }
        
        // ── BOTTOM SCREEN ─────────────────────────────────────────────────────
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, bgColor);

        // Infinite scrolling transparent black checkerboard grid
        drawGrid(320.0f, 240.0f, textScrollTime, 0.05f);

        if (bottomBGSheet && bottomBG.tex) {
            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, C2D_Color32(255, 255, 255, 255), 1.0f);
            drawCenteredBG(bottomBG, 320.0f, 240.0f, 0.10f, &tint);
        }

        // 4 Infinite Scrolling Marquee Rows
        auto drawMarqueeRow = [&](const std::string& label, float y, float scale, float speed, float dir, u32 color) {
            std::string unitText = label + "   ";
            C2D_Text gText;
            C2D_TextFontParse(&gText, quanticoFont ? quanticoFont : vcrFont, quanticoFontBuf ? quanticoFontBuf : vcrFontBuf, unitText.c_str());
            C2D_TextOptimize(&gText);
            float tw = 0.0f, th = 0.0f;
            C2D_TextGetDimensions(&gText, scale, scale, &tw, &th);
            if (tw <= 0.0f) tw = 120.0f;

            float rawOffset = fmodf(textScrollTime * speed * dir, tw);
            if (rawOffset < 0.0f) rawOffset += tw;

            float startX = -tw + rawOffset;
            while (startX < 320.0f) {
                DrawTextBorderFull(&gText, startX, y, 0.14f, scale, scale, 1.5f, CBlack);
                C2D_DrawText(&gText, C2D_WithColor, startX, y, 0.15f, scale, scale, color);
                startX += tw;
            }
        };

        drawMarqueeRow("C++", 62.0f, 1.25f, 35.0f, 1.0f, CWhite);
        drawMarqueeRow("DEVKITARM", 102.0f, 0.90f, 35.0f, -1.0f, CWhite);
        drawMarqueeRow("LIBCTRU", 142.0f, 1.25f, 35.0f, 1.0f, CWhite);
        drawMarqueeRow("CITRO2D", 182.0f, 0.90f, 35.0f, -1.0f, CWhite);

        // Rotated black rectangle on left side of bottom screen
        drawRotatedRect(10.0f, 120.0f, 110.0f, 450.0f, angleRad, C2D_Color32(0, 0, 0, 255), 0.25f);

        // Header MADE WITH:
        C2D_Text headerText;
        C2D_TextFontParse(&headerText, quanticoFont ? quanticoFont : vcrFont, quanticoFontBuf ? quanticoFontBuf : vcrFontBuf, "MADE WITH:");
        C2D_TextOptimize(&headerText);
        DrawTextBorderFull(&headerText, 25.0f, 20.0f, 0.34f, 1.15f, 1.15f, 1.5f, CBlack);
        C2D_DrawText(&headerText, C2D_WithColor, 25.0f, 20.0f, 0.35f, 1.15f, 1.15f, CWhite);

        ButtonPrompt::drawPrompt("b", "Back", 8.0f, 205.0f, 0.70f, 1.0f);
        C2D_Flush();
    }
    else if (subState == STATE_SCROLLING) {
        auto& gp = groups[curSelected];

        // Top screen scrolling credits
        C2D_SceneBegin(top);
        C2D_TargetClear(top, CBlack);
        
        float currentY = scrollY;
        for (const auto& entry : gp.entries) {
            float entryHeight = getEntryHeight(entry);
            if (entry.isTitle) {
                if (currentY > -entryHeight && currentY < 260.0f) {
                    drawScrollText(entry.text1, 200.0f, currentY + entryHeight * 0.5f, 0.75f, true, 0.0f, CYellow, 360.0f);
                }
            } else {
                if (currentY > -entryHeight && currentY < 260.0f) {
                    int lines1 = countParagraphLines(entry.text1, 26);
                    float t1Height = (float)lines1 * 18.0f;
                    
                    if (entry.text2.empty()) {
                        drawScrollText(entry.text1, 200.0f, currentY + entryHeight * 0.5f, 0.60f, true, 0.0f, CWhite, 360.0f);
                    } else {
                        float t1Center = currentY + 12.0f + t1Height * 0.5f;
                        drawScrollText(entry.text1, 200.0f, t1Center, 0.60f, true, 0.0f, CWhite, 360.0f);
                        
                        int lines2 = countParagraphLines(entry.text2, 34);
                        float t2Height = (float)lines2 * 14.0f;
                        float t2Center = currentY + 12.0f + t1Height + 6.0f + t2Height * 0.5f;
                        drawScrollText(entry.text2, 200.0f, t2Center, 0.48f, true, 0.0f, CGray, 360.0f);
                    }
                }
            }
            currentY += entryHeight;
        }
        C2D_Flush();

        // Bottom screen scrolling credits
        C2D_SceneBegin(bottom);
        C2D_TargetClear(bottom, CBlack);

        currentY = scrollY - 240.0f;
        for (const auto& entry : gp.entries) {
            float entryHeight = getEntryHeight(entry);
            if (entry.isTitle) {
                if (currentY > -entryHeight && currentY < 260.0f) {
                    drawScrollText(entry.text1, 160.0f, currentY + entryHeight * 0.5f, 0.75f, true, 0.0f, CYellow, 290.0f);
                }
            } else {
                if (currentY > -entryHeight && currentY < 260.0f) {
                    int lines1 = countParagraphLines(entry.text1, 26);
                    float t1Height = (float)lines1 * 18.0f;
                    
                    if (entry.text2.empty()) {
                        drawScrollText(entry.text1, 160.0f, currentY + entryHeight * 0.5f, 0.60f, true, 0.0f, CWhite, 290.0f);
                    } else {
                        float t1Center = currentY + 12.0f + t1Height * 0.5f;
                        drawScrollText(entry.text1, 160.0f, t1Center, 0.60f, true, 0.0f, CWhite, 290.0f);
                        
                        int lines2 = countParagraphLines(entry.text2, 34);
                        float t2Height = (float)lines2 * 14.0f;
                        float t2Center = currentY + 12.0f + t1Height + 6.0f + t2Height * 0.5f;
                        drawScrollText(entry.text2, 160.0f, t2Center, 0.48f, true, 0.0f, CGray, 290.0f);
                    }
                }
            }
            currentY += entryHeight;
        }
        ButtonPrompt::drawPrompt("b", "Back", 8.0f, 205.0f, 0.70f, 1.0f);
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
    if (quanticoFont) C2D_FontFree(quanticoFont);
    if (quanticoFontBuf) C2D_TextBufDelete(quanticoFontBuf);
    if (inconsolataFont) C2D_FontFree(inconsolataFont);
    if (inconsolataFontBuf) C2D_TextBufDelete(inconsolataFontBuf);
}
