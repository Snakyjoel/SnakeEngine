#pragma once

#include "AdpcmDecoder.hpp"
#include <citro2d.h>
#include <citro3d.h>
#include <3ds.h>
#include <zlib.h>
#include <string>
#include <vector>
#include <stdio.h>

class SnakyDecoder {
public:
    static constexpr int RING_SIZE = 8;
    static constexpr int PADDED_WIDTH = 512;
    static constexpr int PADDED_HEIGHT = 256;
    static constexpr int AUDIO_BUF_COUNT = 15;

    SnakyDecoder();
    ~SnakyDecoder();

    bool open(const std::string& videoPath, bool includeAudio = true);
    void start();
    void stop();
    void close();

    // Frame consumption
    bool getNextFrame(uint16_t** outBuffer, int& outFrameId, int& outRingSlot);
    void releaseFrame(int slotIndex);

    // Audio & sync
    float getAudioMasterTime(float fallbackTimer);
    void updateAudioTracking();
    void setTargetFrame(int targetFrame);

    // Status getters
    bool isOpen() const { return file != nullptr; }
    bool isEnded() const { return videoEnded && ringCount == 0; }
    bool isVideoFileEnded() const { return videoEnded; }
    uint16_t getWidth() const { return width; }
    uint16_t getHeight() const { return height; }
    uint8_t getFps() const { return fps > 0 ? fps : 24; }
    uint8_t getFormat() const { return format; }
    uint32_t getTotalFrames() const { return totalFrames; }
    bool hasAudioTrack() const { return hasAudio; }
    int getAvailableFrames() const { return ringCount; }

private:
    std::string path;
    FILE* file = nullptr;
    bool includeAudioTrack = true;

    // Header metadata
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t fps = 24;
    uint8_t format = 0;
    uint32_t totalFrames = 0;
    uint16_t audioRate = 32000;
    uint8_t channels = 1;
    uint8_t audioFormat = 0;
    bool isZlib = false;
    bool hasAudio = false;

    // Ring buffer (512x256 pitch linear buffers)
    uint16_t* decodeBuf[RING_SIZE];
    int ringFrameIds[RING_SIZE];
    volatile int writeIdx = 0;
    volatile int readIdx = 0;
    volatile int ringCount = 0;
    volatile int lockedReadIdx = -1;

    // Decoding state & persistent zlib stream
    z_stream zstream;
    bool zstreamInitialized = false;
    std::vector<uint8_t> compressedBuf;
    std::vector<uint8_t> uncompressedBuf;
    int currentFrame = 0;
    volatile int targetFrameCached = 0;
    volatile bool skipToNextKey = false;
    volatile bool videoEnded = false;

    // Threading
    Thread decodeThread = nullptr;
    LightEvent eventDecodeRequest;
    LightLock decodeLock;
    volatile bool threadRunning = false;
    volatile bool isPaused = false;

    // Audio (NDSP channel 5)
    ndspWaveBuf waveBuf[AUDIO_BUF_COUNT];
    int16_t* audioBufferData = nullptr;
    uint64_t audioSamplesPlayed = 0;
    AdpcmDecoder::State adpcmState;
    std::vector<uint8_t> adpcmAudioBuf;

    void readHeader();
    void decodeChunk();

    static void threadMain(void* arg);
};
