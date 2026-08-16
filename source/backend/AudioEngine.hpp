#pragma once
#include <3ds.h>
#include <citro3d.h>
#include <tremor/ivorbisfile.h>
#include "AdpcmDecoder.hpp"
#include <stdio.h>
#include <string.h>
#include <string>
#include <map>

#define SAMPLE_RATE 44100
#define CHANNELS 2
#define BUFFER_SAMPLES 2048
#define BUFFER_COUNT 4

class AudioEngine {
public:
    static bool init(const char* instPath, const char* vocalsPath = nullptr);
    static void start();
    static void pause();
    static void resume();
    static void update();
    static void exit();
    static double getSongPosition();
    static double getTotalTime();
    static bool isFinished();
    static void clearSoundCache();

    static void setVocalsVolume(float vol);
    static void playSound(const std::string& path, float vol = 1.0f);
    
    // SFX Methods
    static void initMissSounds();
    static void playMissSound();
    
    static void initCountdownSounds();
    static void playCountdownSound(int tick);
    static void freeCountdownSounds();
    
    static bool isLoaded;
    static bool hasVocals;
    static bool paused;
    static long actualSampleRate;

private:
    static void fill(int id, bool countAsPlayed, bool isVocals = false);
    
    // Instrumental
    static ndspWaveBuf waveBuf[BUFFER_COUNT];
    static int16_t *bufferData;
    static OggVorbis_File vf;
    static int fillBuf;
    static uint64_t totalSamples;

    // Vocals
    static ndspWaveBuf vocalsWaveBuf[BUFFER_COUNT];
    static int16_t *vocalsBufferData;
    static OggVorbis_File vocalsVf;
    static int vocalsFillBuf;
    
    // ADP state
    static FILE* adpFile;
    static bool isAdp;
    static AdpcmDecoder::State adpState;
    static uint32_t adpTotalSamples;
    static uint32_t adpSamplesRead;

    static FILE* vocalsAdpFile;
    static bool vocalsIsAdp;
    static AdpcmDecoder::State vocalsAdpState;
    static uint32_t vocalsAdpTotalSamples;
    static uint32_t vocalsAdpSamplesRead;
    
    static double lastSampleTick;
    static double pauseOffset;

    // SFX decoding data
    struct SfxData {
        int16_t* buffer;
        uint32_t samplesPerChannel;
        int channels;
        int rate;
    };
    struct SoundData {
        int16_t* buffer;
        uint32_t samplesPerChannel;
        int channels;
        int rate;
    };
    static std::map<std::string, SoundData> soundCache;
    
    static SfxData missSfx[3];
    static SfxData countdownSfx[4];
    static ndspWaveBuf sfxWaveBuf[3]; // Persistent buffers for channels 2, 3, 4
    static int sndChannelIdx;
};

class MusicPlayer {
public:
    static bool play(const char* path, float volume = 0.7f);
    static void playMenuMusic(); // play freakyMenu if not already playing
    static void stop();
    static void pause();
    static void resume();
    static void update();       // call every frame
    static bool isPlaying();
    static double getPosition(); // Returns elapsed time in milliseconds
    static double getDuration(); // Returns total track duration in milliseconds (0 if unknown)
    static void setVolume(float volume);

private:
    static constexpr int CHANNEL    = 5;
    static constexpr int BUF_COUNT  = 2;
    static constexpr int BUF_SMPLS  = 4096;

    static OggVorbis_File  vf;
    static ndspWaveBuf     waveBuf[BUF_COUNT];
    static int16_t*        audioData;
    static int             fillIdx;
    static bool            loaded;
    static bool            playing;
    static bool            mPaused;
    static int             mChannels;
    static int             mSampleRate;

    static uint64_t        totalSamples;
    static double          lastSampleTick;
    static double          pauseOffset;
    static double          trackDurationMs;

    static std::string     currentTrackPath;
    static void fillBuffer(int idx);
};

