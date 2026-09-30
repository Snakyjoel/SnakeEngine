#include "AudioEngine.hpp"
#include <malloc.h>
#include <string>
#include <algorithm>

static LightLock s_musicLock;
static LightLock s_audioLock;
static Thread s_audioThread = nullptr;
static volatile bool s_audioThreadRunning = false;

static size_t vorbis_read_cb(void* ptr, size_t size, size_t nmemb, void* datasource) {
    return fread(ptr, size, nmemb, (FILE*)datasource);
}

static int vorbis_seek_cb(void* datasource, ogg_int64_t offset, int whence) {
    return fseek((FILE*)datasource, (long)offset, whence);
}

static int vorbis_close_cb(void* datasource) {
    return fclose((FILE*)datasource);
}

static long vorbis_tell_cb(void* datasource) {
    return ftell((FILE*)datasource);
}

static ov_callbacks s_vorbisCallbacks = {
    vorbis_read_cb,
    vorbis_seek_cb,
    vorbis_close_cb,
    vorbis_tell_cb
};

static void audioThreadMain(void* arg) {
    while (s_audioThreadRunning) {
        MusicPlayer::update();
        AudioEngine::update();
        svcSleepThread(5000000LL); // 5 ms sleep per iteration
    }
}

void AudioEngine::startAudioThread() {
    if (s_audioThreadRunning) return;
    LightLock_Init(&s_musicLock);
    LightLock_Init(&s_audioLock);
    s_audioThreadRunning = true;
    s_audioThread = threadCreate(audioThreadMain, nullptr, 32 * 1024, 0x18, -2, false);
}

void AudioEngine::stopAudioThread() {
    if (!s_audioThreadRunning) return;
    s_audioThreadRunning = false;
    if (s_audioThread) {
        threadJoin(s_audioThread, U64_MAX);
        threadFree(s_audioThread);
        s_audioThread = nullptr;
    }
}

std::map<std::string, AudioEngine::SoundData> AudioEngine::soundCache;

// Instrumental
ndspWaveBuf AudioEngine::waveBuf[BUFFER_COUNT];
int16_t* AudioEngine::bufferData = nullptr;
OggVorbis_File AudioEngine::vf;
int AudioEngine::fillBuf = 0;
uint64_t AudioEngine::totalSamples = 0;
bool AudioEngine::oggEof = false;

FILE* AudioEngine::adpFile = nullptr;
bool AudioEngine::isAdp = false;
AdpcmDecoder::State AudioEngine::adpState;
uint32_t AudioEngine::adpTotalSamples = 0;
uint32_t AudioEngine::adpSamplesRead = 0;

// Vocals
ndspWaveBuf AudioEngine::vocalsWaveBuf[BUFFER_COUNT];
int16_t* AudioEngine::vocalsBufferData = nullptr;
OggVorbis_File AudioEngine::vocalsVf;
int AudioEngine::vocalsFillBuf = 0;
bool AudioEngine::vocalsOggEof = false;

FILE* AudioEngine::vocalsAdpFile = nullptr;
bool AudioEngine::vocalsIsAdp = false;
AdpcmDecoder::State AudioEngine::vocalsAdpState;
uint32_t AudioEngine::vocalsAdpTotalSamples = 0;
uint32_t AudioEngine::vocalsAdpSamplesRead = 0;

// WAV state variables
FILE* AudioEngine::wavFile = nullptr;
bool AudioEngine::isWav = false;
uint32_t AudioEngine::wavDataOffset = 0;
uint32_t AudioEngine::wavTotalSamples = 0;
uint32_t AudioEngine::wavSamplesRead = 0;

FILE* AudioEngine::vocalsWavFile = nullptr;
bool AudioEngine::vocalsIsWav = false;
uint32_t AudioEngine::vocalsWavDataOffset = 0;
uint32_t AudioEngine::vocalsWavTotalSamples = 0;
uint32_t AudioEngine::vocalsWavSamplesRead = 0;

int AudioEngine::instNumChannels = 2;
int AudioEngine::vocalsNumChannels = 2;

bool AudioEngine::isLoaded = false;
bool AudioEngine::hasVocals = false;
bool AudioEngine::paused = false;
long AudioEngine::actualSampleRate = 44100;
double AudioEngine::pauseOffset = 0;
double AudioEngine::lastSampleTick = 0;

static bool openWavFile(const char* path, FILE*& outWav, uint32_t& outSampleRate, int& outChannels, uint32_t& outDataOffset, uint32_t& outTotalSamples) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    char riff[4];
    if (fread(riff, 1, 4, f) != 4 || memcmp(riff, "RIFF", 4) != 0) { fclose(f); return false; }
    fseek(f, 4, SEEK_CUR); // skip size
    char wave[4];
    if (fread(wave, 1, 4, f) != 4 || memcmp(wave, "WAVE", 4) != 0) { fclose(f); return false; }

    outChannels = 2;
    outSampleRate = 44100;
    outTotalSamples = 0;
    outDataOffset = 0;
    int bitDepth = 16;
    bool foundFmt = false;
    bool foundData = false;

    while (!foundData) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        if (fread(chunkId, 1, 4, f) != 4) break;
        if (fread(&chunkSize, 4, 1, f) != 1) break;

        if (memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat = 0;
            fread(&audioFormat, 2, 1, f);
            if (audioFormat != 1) { fclose(f); return false; } // Only PCM
            uint16_t chans = 0;
            fread(&chans, 2, 1, f);
            outChannels = chans;
            fread(&outSampleRate, 4, 1, f);
            fseek(f, 6, SEEK_CUR); // skip byteRate and blockAlign
            uint16_t bits = 0;
            fread(&bits, 2, 1, f);
            bitDepth = bits;
            if (bitDepth != 16) { fclose(f); return false; }
            if (chunkSize > 16) fseek(f, chunkSize - 16, SEEK_CUR);
            foundFmt = true;
        } else if (memcmp(chunkId, "data", 4) == 0) {
            outDataOffset = ftell(f);
            outTotalSamples = chunkSize / (outChannels * (bitDepth / 8));
            foundData = true;
        } else {
            fseek(f, chunkSize, SEEK_CUR);
        }
    }

    if (foundFmt && foundData) {
        outWav = f;
        fseek(outWav, outDataOffset, SEEK_SET);
        return true;
    }
    
    fclose(f);
    return false;
}

bool AudioEngine::init(const char* instPath, const char* vocalsPath) {
    exit();

    std::string adpInstPath = instPath;
    if (adpInstPath.find(".ogg") != std::string::npos) {
        adpInstPath.replace(adpInstPath.find(".ogg"), 4, ".adp");
    }

    isAdp = false;
    isWav = false;
    oggEof = false;
    vocalsOggEof = false;
    memset(&vf, 0, sizeof(OggVorbis_File));
    memset(&vocalsVf, 0, sizeof(OggVorbis_File));
    actualSampleRate = 44100;
    int instChannels = 2;

    uint32_t wavSR = 0;
    int wavCh = 0;
    uint32_t wavDO = 0;
    uint32_t wavTS = 0;

    if (openWavFile(instPath, wavFile, wavSR, wavCh, wavDO, wavTS)) {
        isWav = true;
        actualSampleRate = wavSR;
        instChannels = wavCh;
        wavDataOffset = wavDO;
        wavTotalSamples = wavTS;
        wavSamplesRead = 0;
    } else {
        adpFile = fopen(adpInstPath.c_str(), "rb");
        if (adpFile) {
            AdpcmEncoder::Header header;
            if (fread(&header, sizeof(header), 1, adpFile) == 1 && memcmp(header.magic, "SADP", 4) == 0 && header.numSamples > 0) {
                isAdp = true;
                actualSampleRate = header.sampleRate;
                adpTotalSamples = header.numSamples;
                adpSamplesRead = 0;
                adpState.predictor = 0;
                adpState.stepIndex = 0;
                instChannels = header.channels;
            } else {
                fclose(adpFile);
                adpFile = nullptr;
            }
        }
    }

    if (!isAdp && !isWav) {
        FILE* fInst = fopen(instPath, "rb");
        if (!fInst) {
            FILE* log = fopen("sdmc:/snake_log.txt", "a");
            if(log) { fprintf(log, "Failed to open inst file: %s\n", instPath); fclose(log); }
            return false;
        }
        int err = ov_open_callbacks(fInst, &vf, NULL, 0, s_vorbisCallbacks);
        if (err < 0) {
            FILE* log = fopen("sdmc:/snake_log.txt", "a");
            if(log) { fprintf(log, "ov_open_callbacks failed with code %d for %s\n", err, instPath); fclose(log); }
            fclose(fInst);
            return false;
        }
        vorbis_info* vi = ov_info(&vf, -1);
        actualSampleRate = vi->rate;
        instChannels = vi->channels;
    }

    // Check Vocals
    hasVocals = false;
    long vocalsRate = 44100;
    int vocalsChannels = 2;
    vocalsIsAdp = false;
    vocalsIsWav = false;

    if (vocalsPath != nullptr) {
        std::string adpVocPath = vocalsPath;
        if (adpVocPath.find(".ogg") != std::string::npos) {
            adpVocPath.replace(adpVocPath.find(".ogg"), 4, ".adp");
        } else if (adpVocPath.find(".wav") != std::string::npos) {
            adpVocPath.replace(adpVocPath.find(".wav"), 4, ".adp");
        }

        uint32_t vWavSR = 0;
        int vWavCh = 0;
        uint32_t vWavDO = 0;
        uint32_t vWavTS = 0;

        if (openWavFile(vocalsPath, vocalsWavFile, vWavSR, vWavCh, vWavDO, vWavTS)) {
            vocalsIsWav = true;
            hasVocals = true;
            vocalsRate = vWavSR;
            vocalsChannels = vWavCh;
            vocalsWavDataOffset = vWavDO;
            vocalsWavTotalSamples = vWavTS;
            vocalsWavSamplesRead = 0;
        } else {
            vocalsAdpFile = fopen(adpVocPath.c_str(), "rb");
            if (vocalsAdpFile) {
                AdpcmEncoder::Header vHeader;
                if (fread(&vHeader, sizeof(vHeader), 1, vocalsAdpFile) == 1 && memcmp(vHeader.magic, "SADP", 4) == 0 && vHeader.numSamples > 0) {
                    vocalsIsAdp = true;
                    hasVocals = true;
                    vocalsRate = vHeader.sampleRate;
                    vocalsAdpTotalSamples = vHeader.numSamples;
                    vocalsAdpSamplesRead = 0;
                    vocalsAdpState.predictor = 0;
                    vocalsAdpState.stepIndex = 0;
                    vocalsChannels = vHeader.channels;
                } else {
                    fclose(vocalsAdpFile);
                    vocalsAdpFile = nullptr;
                }
            }
        }

        if (!vocalsIsAdp && !vocalsIsWav) {
            FILE* fVoc = fopen(vocalsPath, "rb");
            if (fVoc) {
                if (ov_open_callbacks(fVoc, &vocalsVf, NULL, 0, s_vorbisCallbacks) >= 0) {
                    hasVocals = true;
                    vorbis_info* viV = ov_info(&vocalsVf, -1);
                    vocalsRate = viV->rate;
                    vocalsChannels = viV->channels;
                } else {
                    fclose(fVoc);
                }
            }
        }
    }

    instNumChannels = instChannels;
    vocalsNumChannels = vocalsChannels;

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetFormat(0, instChannels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, actualSampleRate);

    bufferData = (int16_t*)linearMemAlign(BUFFER_SAMPLES * instChannels * BUFFER_COUNT * sizeof(int16_t), 0x80);
    memset(waveBuf, 0, sizeof(waveBuf));

    if (hasVocals) {
        ndspChnSetFormat(1, vocalsChannels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
        ndspChnSetInterp(1, NDSP_INTERP_LINEAR);
        ndspChnSetRate(1, vocalsRate);
        vocalsBufferData = (int16_t*)linearMemAlign(BUFFER_SAMPLES * vocalsChannels * BUFFER_COUNT * sizeof(int16_t), 0x80);
        memset(vocalsWaveBuf, 0, sizeof(vocalsWaveBuf));
    }

    totalSamples = 0;
    lastSampleTick = 0;
    fillBuf = 0;
    vocalsFillBuf = 0;
    if (hasVocals) setVocalsVolume(1.0f);
    
    isLoaded = true;
    return true;
}


void AudioEngine::start() {
    if (!isLoaded) return;
    totalSamples = 0;
    ndspChnSetPaused(0, true);
    if (hasVocals) ndspChnSetPaused(1, true);

    for (int i = 0; i < BUFFER_COUNT; i++) {
        waveBuf[i].data_vaddr = &bufferData[i * BUFFER_SAMPLES * instNumChannels];
        waveBuf[i].nsamples = BUFFER_SAMPLES;
        fill(i, false, false); 
        ndspChnWaveBufAdd(0, &waveBuf[i]);

        if (hasVocals) {
            vocalsWaveBuf[i].data_vaddr = &vocalsBufferData[i * BUFFER_SAMPLES * vocalsNumChannels];
            vocalsWaveBuf[i].nsamples = BUFFER_SAMPLES;
            fill(i, false, true);
            ndspChnWaveBufAdd(1, &vocalsWaveBuf[i]);
        }
    }
    
    lastSampleTick = (double)osGetTime();
    pauseOffset = 0;
    paused = false;
    ndspChnSetPaused(0, false);
    if (hasVocals) ndspChnSetPaused(1, false);
}

void AudioEngine::pause() {
    if (!isLoaded || paused) return;
    paused = true;
    pauseOffset = (double)osGetTime() - lastSampleTick;
    ndspChnSetPaused(0, true);
    if (hasVocals) ndspChnSetPaused(1, true);
}

void AudioEngine::resume() {
    if (!isLoaded || !paused) return;
    paused = false;
    lastSampleTick = (double)osGetTime() - pauseOffset;
    ndspChnSetPaused(0, false);
    if (hasVocals) ndspChnSetPaused(1, false);
}

void AudioEngine::fill(int id, bool countAsPlayed, bool isVocals) {
    if (countAsPlayed && !isVocals) {
        totalSamples += waveBuf[id].nsamples;
        lastSampleTick = (double)osGetTime();
    }

    ndspWaveBuf* targetBuf = isVocals ? &vocalsWaveBuf[id] : &waveBuf[id];
    int16_t* ptr = (int16_t*)targetBuf->data_vaddr;
    bool usingAdp = isVocals ? vocalsIsAdp : isAdp;
    bool usingWav = isVocals ? vocalsIsWav : isWav;
    
    size_t samplesToRead = BUFFER_SAMPLES;
    size_t samplesRead = 0;

    if (usingAdp) {
        FILE* f = isVocals ? vocalsAdpFile : adpFile;
        uint32_t& adpRead = isVocals ? vocalsAdpSamplesRead : adpSamplesRead;
        uint32_t adpTotal = isVocals ? vocalsAdpTotalSamples : adpTotalSamples;
        AdpcmDecoder::State& state = isVocals ? vocalsAdpState : adpState;

        samplesToRead = std::min((uint32_t)BUFFER_SAMPLES, adpTotal - adpRead);
        if (samplesToRead > 0) {
            uint32_t bytesToRead = samplesToRead / 2; // ADP is 4bpp
            uint8_t* adpBuffer = (uint8_t*)malloc(bytesToRead);
            if (adpBuffer) {
                fread(adpBuffer, 1, bytesToRead, f);
                uint32_t decoded = 0;
                AdpcmDecoder::decodeIMA(adpBuffer, bytesToRead, ptr, decoded, state);
                samplesRead = decoded;
                adpRead += decoded;
                free(adpBuffer);
            }
        }
    } else if (usingWav) {
        FILE* f = isVocals ? vocalsWavFile : wavFile;
        uint32_t& wavRead = isVocals ? vocalsWavSamplesRead : wavSamplesRead;
        uint32_t wavTotal = isVocals ? vocalsWavTotalSamples : wavTotalSamples;
        int numChannels = isVocals ? vocalsNumChannels : instNumChannels;
        
        samplesToRead = std::min((uint32_t)BUFFER_SAMPLES, wavTotal - wavRead);
        if (samplesToRead > 0) {
            size_t bytesToRead = samplesToRead * numChannels * sizeof(int16_t);
            size_t bytesRead = fread(ptr, 1, bytesToRead, f);
            samplesRead = bytesRead / (numChannels * sizeof(int16_t));
            wavRead += samplesRead;
        } else {
            if (isVocals) vocalsOggEof = true;
            else oggEof = true;
        }
    } else {
        OggVorbis_File* targetVf = isVocals ? &vocalsVf : &vf;
        int numChannels = isVocals ? vocalsNumChannels : instNumChannels;
        while (samplesRead < BUFFER_SAMPLES) {
            int bitstream;
            long read = ov_read(targetVf, (char*)(ptr + samplesRead * numChannels), (BUFFER_SAMPLES - samplesRead) * numChannels * 2, &bitstream);
            if (read < 0) {
                if (read == OV_HOLE) continue;
                FILE* log = fopen("sdmc:/snake_log.txt", "a");
                if(log) { fprintf(log, "ov_read failed with %ld in AudioEngine::fill\n", read); fclose(log); }
                break;
            }
            if (read == 0) {
                if (isVocals) vocalsOggEof = true;
                else oggEof = true;
                break;
            }
            samplesRead += read / (numChannels * 2);
        }
    }
    
    int numChannels = isVocals ? vocalsNumChannels : instNumChannels;
    targetBuf->nsamples = samplesRead;
    DSP_FlushDataCache(targetBuf->data_vaddr, targetBuf->nsamples * numChannels * 2);
    DSP_FlushDataCache(targetBuf, sizeof(ndspWaveBuf));
}

double AudioEngine::getSongPosition() {
    if (!isLoaded || lastSampleTick == 0) return 0;
    double anchorMs = (double)totalSamples / (double)actualSampleRate * 1000.0;
    double diff = paused ? pauseOffset : ((double)osGetTime() - lastSampleTick);
    return anchorMs + diff;
}

double AudioEngine::getTotalTime() {
    if (!isLoaded) return 0;
    if (isAdp) return (double)adpTotalSamples / (double)actualSampleRate * 1000.0;
    if (isWav) return (double)wavTotalSamples / (double)actualSampleRate * 1000.0;
    return (double)ov_pcm_total(&vf, -1) / (double)actualSampleRate * 1000.0;
}

bool AudioEngine::isFinished() {
    if (!isLoaded) return true;
    bool eof = false;
    if (isAdp) {
        eof = (adpSamplesRead + 16 >= adpTotalSamples);
    } else if (isWav) {
        eof = (wavSamplesRead + 16 >= wavTotalSamples);
    } else {
        long long tell = ov_pcm_tell(&vf);
        long long total = ov_pcm_total(&vf, -1);
        eof = oggEof || (tell >= 0 && total > 0 && tell + 16 >= total);
    }
    
    if (!eof) return false;
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (waveBuf[i].status != NDSP_WBUF_DONE) return false;
    }
    return true;
}

void AudioEngine::setVocalsVolume(float vol) {
    if (!isLoaded || !hasVocals) return;
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = vol; mix[1] = vol;
    ndspChnSetMix(1, mix);
}

void AudioEngine::update() {
    LightLock_Lock(&s_audioLock);
    if (!isLoaded || paused) {
        LightLock_Unlock(&s_audioLock);
        return;
    }
    
    bool instEof = false;
    if (isAdp) {
        instEof = (adpSamplesRead + 16 >= adpTotalSamples);
    } else if (isWav) {
        instEof = (wavSamplesRead + 16 >= wavTotalSamples);
    } else {
        long long tell = ov_pcm_tell(&vf);
        long long total = ov_pcm_total(&vf, -1);
        instEof = oggEof || (tell >= 0 && total > 0 && tell + 16 >= total);
    }

    if (!instEof && waveBuf[fillBuf].status == NDSP_WBUF_DONE) {
        fill(fillBuf, true, false);
        ndspChnWaveBufAdd(0, &waveBuf[fillBuf]);
        fillBuf = (fillBuf + 1) % BUFFER_COUNT;
    }
    
    if (hasVocals) {
        bool vocEof = false;
        if (vocalsIsAdp) {
            vocEof = (vocalsAdpSamplesRead + 16 >= vocalsAdpTotalSamples);
        } else if (vocalsIsWav) {
            vocEof = (vocalsWavSamplesRead + 16 >= vocalsWavTotalSamples);
        } else {
            long long tell = ov_pcm_tell(&vocalsVf);
            long long total = ov_pcm_total(&vocalsVf, -1);
            vocEof = vocalsOggEof || (tell >= 0 && total > 0 && tell + 16 >= total);
        }
        
        if (!vocEof && vocalsWaveBuf[vocalsFillBuf].status == NDSP_WBUF_DONE) {
            fill(vocalsFillBuf, false, true);
            ndspChnWaveBufAdd(1, &vocalsWaveBuf[vocalsFillBuf]);
            vocalsFillBuf = (vocalsFillBuf + 1) % BUFFER_COUNT;
        }
    }
    LightLock_Unlock(&s_audioLock);
}

void AudioEngine::exit() {
    LightLock_Lock(&s_audioLock);
    if (isLoaded) {
        // Pause channels before clearing to stop new DSP callbacks
        ndspChnSetPaused(0, true);
        if (hasVocals) ndspChnSetPaused(1, true);
        ndspChnSetPaused(2, true);
        ndspChnSetPaused(3, true);
        ndspChnSetPaused(4, true);

        // Reset channels - this drains the DSP queue immediately
        ndspChnReset(0);
        if (hasVocals) ndspChnReset(1);
        ndspChnReset(2);
        ndspChnReset(3);
        ndspChnReset(4);

        // Give the DSP one audio frame (~16 ms) to finish any in-flight DMA before
        // we free the linear memory it was reading from.
        // NOTE: Do NOT poll waveBuf.status — ndspChnReset() does not update the
        // status field in Citra's NDSP emulation, which would cause an infinite loop.
        svcSleepThread(16000000LL); // 16 ms

        if (isAdp) {
            if (adpFile) fclose(adpFile);
            adpFile = nullptr;
        } else if (isWav) {
            if (wavFile) fclose(wavFile);
            wavFile = nullptr;
        } else {
            ov_clear(&vf);
        }
        
        if (hasVocals) {
            if (vocalsIsAdp) {
                if (vocalsAdpFile) fclose(vocalsAdpFile);
                vocalsAdpFile = nullptr;
            } else if (vocalsIsWav) {
                if (vocalsWavFile) fclose(vocalsWavFile);
                vocalsWavFile = nullptr;
            } else {
                ov_clear(&vocalsVf);
            }
        }
        
        linearFree(bufferData);
        if (vocalsBufferData) linearFree(vocalsBufferData);
        bufferData = nullptr;
        vocalsBufferData = nullptr;
        isLoaded = false;
        hasVocals = false;
    }
    LightLock_Unlock(&s_audioLock);

    for (auto const& pair : soundCache) {
        if (pair.second.buffer) {
            linearFree(pair.second.buffer);
        }
    }
    soundCache.clear();
}

void AudioEngine::clearSoundCache() {
    ndspChnSetPaused(2, true);
    ndspChnSetPaused(3, true);
    ndspChnSetPaused(4, true);
    ndspChnReset(2);
    ndspChnReset(3);
    ndspChnReset(4);
    for (auto const& pair : soundCache) {
        if (pair.second.buffer) {
            linearFree(pair.second.buffer);
        }
    }
    soundCache.clear();
}

void AudioEngine::playSound(const std::string& path, float vol) {
    std::string actualPath = path;
    if (actualPath.find(".wav") != std::string::npos) {
        FILE* testFile = fopen(actualPath.c_str(), "rb");
        if (testFile) {
            fclose(testFile);
        } else {
            actualPath.replace(actualPath.find(".wav"), 4, ".ogg");
        }
    }

    SoundData sound;
    auto it = soundCache.find(actualPath);
    if (it == soundCache.end()) {
        FILE* f = fopen(actualPath.c_str(), "rb");
        if (!f) return;
        
        OggVorbis_File sfxVf;
        if (ov_open_callbacks(f, &sfxVf, NULL, 0, s_vorbisCallbacks) < 0) {
            fclose(f);
            return;
        }
        
        vorbis_info* vi = ov_info(&sfxVf, -1);
        long totalSamples = ov_pcm_total(&sfxVf, -1);
        sound.samplesPerChannel = totalSamples;
        sound.channels = vi->channels;
        sound.rate = vi->rate;
        
        uint32_t bufferSize = totalSamples * vi->channels * sizeof(int16_t);
        sound.buffer = (int16_t*)linearMemAlign(bufferSize, 0x80);
        if (sound.buffer) {
            uint32_t totalRead = 0;
            int bitstream = 0;
            while (totalRead < bufferSize) {
                long ret = ov_read(&sfxVf, (char*)sound.buffer + totalRead, bufferSize - totalRead, &bitstream);
                if (ret <= 0) break;
                totalRead += ret;
            }
            DSP_FlushDataCache(sound.buffer, bufferSize);
        }
        ov_clear(&sfxVf);
        
        if (!sound.buffer) return;
        
        soundCache.emplace(actualPath, sound);
    } else {
        sound = it->second;
    }

    int ch = sndChannelIdx;
    sndChannelIdx++;
    if (sndChannelIdx > 4) sndChannelIdx = 2;
    
    ndspChnSetPaused(ch, true);
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, sound.rate);
    ndspChnSetFormat(ch, sound.channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    
    float mix[12] = {0};
    mix[0] = vol;
    mix[1] = vol;
    ndspChnSetMix(ch, mix);
    
    int sfxBufIdx = ch - 2;
    memset(&sfxWaveBuf[sfxBufIdx], 0, sizeof(ndspWaveBuf));
    sfxWaveBuf[sfxBufIdx].data_vaddr = sound.buffer;
    sfxWaveBuf[sfxBufIdx].nsamples = sound.samplesPerChannel;
    sfxWaveBuf[sfxBufIdx].status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(ch, &sfxWaveBuf[sfxBufIdx]);
}

AudioEngine::SfxData AudioEngine::missSfx[3] = { {nullptr, 0, 0, 0}, {nullptr, 0, 0, 0}, {nullptr, 0, 0, 0} };
AudioEngine::SfxData AudioEngine::countdownSfx[4] = { {nullptr, 0, 0, 0}, {nullptr, 0, 0, 0}, {nullptr, 0, 0, 0}, {nullptr, 0, 0, 0} };
ndspWaveBuf AudioEngine::sfxWaveBuf[3];
int AudioEngine::sndChannelIdx = 2;

void AudioEngine::initMissSounds() {
    for (int i = 0; i < 3; i++) {
        if (missSfx[i].buffer) continue;
        std::string path = "romfs:/shared/sounds/missnote" + std::to_string(i+1) + ".ogg";
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) continue;
        OggVorbis_File sfxVf;
        if (ov_open(f, &sfxVf, NULL, 0) < 0) { fclose(f); continue; }
        vorbis_info* vi = ov_info(&sfxVf, -1);
        long totalSamples = ov_pcm_total(&sfxVf, -1);
        missSfx[i].samplesPerChannel = totalSamples;
        missSfx[i].channels = vi->channels;
        missSfx[i].rate = vi->rate;
        uint32_t bufferSize = totalSamples * vi->channels * sizeof(int16_t);
        missSfx[i].buffer = (int16_t*)linearMemAlign(bufferSize, 0x80);
        if (missSfx[i].buffer) {
            uint32_t totalRead = 0; int bitstream = 0;
            while (totalRead < bufferSize) {
                long ret = ov_read(&sfxVf, (char*)missSfx[i].buffer + totalRead, bufferSize - totalRead, &bitstream);
                if (ret <= 0) break;
                totalRead += ret;
            }
            DSP_FlushDataCache(missSfx[i].buffer, bufferSize);
        }
        ov_clear(&sfxVf);
    }
}

void AudioEngine::playMissSound() {
    int idx = rand() % 3;
    if (!missSfx[idx].buffer) return;
    int ch = sndChannelIdx;
    sndChannelIdx++;
    if (sndChannelIdx > 4) sndChannelIdx = 2;
    ndspChnSetPaused(ch, true);
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, missSfx[idx].rate);
    ndspChnSetFormat(ch, missSfx[idx].channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {0};
    mix[0] = 0.8f; mix[1] = 0.8f;
    ndspChnSetMix(ch, mix);
    int sfxBufIdx = ch - 2;
    memset(&sfxWaveBuf[sfxBufIdx], 0, sizeof(ndspWaveBuf));
    sfxWaveBuf[sfxBufIdx].data_vaddr = missSfx[idx].buffer;
    sfxWaveBuf[sfxBufIdx].nsamples = missSfx[idx].samplesPerChannel;
    sfxWaveBuf[sfxBufIdx].status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(ch, &sfxWaveBuf[sfxBufIdx]);
}

void AudioEngine::initCountdownSounds() {
    static const char* const files[4] = {"intro3.ogg", "intro2.ogg", "intro1.ogg", "introGo.ogg"};
    for (int i = 0; i < 4; i++) {
        if (countdownSfx[i].buffer) continue;
        std::string path = "romfs:/shared/sounds/" + std::string(files[i]);
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) continue;
        OggVorbis_File sfxVf;
        if (ov_open(f, &sfxVf, NULL, 0) < 0) { fclose(f); continue; }
        vorbis_info* vi = ov_info(&sfxVf, -1);
        long totalSamples = ov_pcm_total(&sfxVf, -1);
        countdownSfx[i].samplesPerChannel = totalSamples;
        countdownSfx[i].channels = vi->channels;
        countdownSfx[i].rate = vi->rate;
        uint32_t bufferSize = totalSamples * vi->channels * sizeof(int16_t);
        countdownSfx[i].buffer = (int16_t*)linearMemAlign(bufferSize, 0x80);
        if (countdownSfx[i].buffer) {
            uint32_t totalRead = 0; int bitstream = 0;
            while (totalRead < bufferSize) {
                long ret = ov_read(&sfxVf, (char*)countdownSfx[i].buffer + totalRead, bufferSize - totalRead, &bitstream);
                if (ret <= 0) break;
                totalRead += ret;
            }
            DSP_FlushDataCache(countdownSfx[i].buffer, bufferSize);
        }
        ov_clear(&sfxVf);
    }
}

void AudioEngine::playCountdownSound(int tick) {
    if (tick < 0 || tick > 3) return;
    if (!countdownSfx[tick].buffer) return;
    int ch = sndChannelIdx;
    sndChannelIdx++;
    if (sndChannelIdx > 4) sndChannelIdx = 2;
    ndspChnSetPaused(ch, true);
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, countdownSfx[tick].rate);
    ndspChnSetFormat(ch, countdownSfx[tick].channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {0};
    mix[0] = 0.8f; mix[1] = 0.8f;
    ndspChnSetMix(ch, mix);
    int sfxBufIdx = ch - 2;
    memset(&sfxWaveBuf[sfxBufIdx], 0, sizeof(ndspWaveBuf));
    sfxWaveBuf[sfxBufIdx].data_vaddr = countdownSfx[tick].buffer;
    sfxWaveBuf[sfxBufIdx].nsamples = countdownSfx[tick].samplesPerChannel;
    sfxWaveBuf[sfxBufIdx].status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(ch, &sfxWaveBuf[sfxBufIdx]);
}

void AudioEngine::freeCountdownSounds() {
    ndspChnSetPaused(2, true);
    ndspChnSetPaused(3, true);
    ndspChnSetPaused(4, true);
    ndspChnReset(2);
    ndspChnReset(3);
    ndspChnReset(4);
    for (int i = 0; i < 4; i++) {
        if (countdownSfx[i].buffer) {
            linearFree(countdownSfx[i].buffer);
            countdownSfx[i].buffer = nullptr;
            countdownSfx[i].samplesPerChannel = 0;
        }
    }
}

// ── MusicPlayer implementation ────────────────────────────────────────────────
float          MusicPlayer::realtimePcm[64] = {0};
float          MusicPlayer::realtimePeak = 0.0f;
OggVorbis_File MusicPlayer::vf;
ndspWaveBuf    MusicPlayer::waveBuf[MusicPlayer::BUF_COUNT];
int16_t*       MusicPlayer::audioData  = nullptr;
int            MusicPlayer::fillIdx    = 0;
bool           MusicPlayer::loaded     = false;
bool           MusicPlayer::playing    = false;
bool           MusicPlayer::mPaused    = false;
int            MusicPlayer::mChannels  = 2;
int            MusicPlayer::mSampleRate = 44100;

uint64_t       MusicPlayer::totalSamples = 0;
double         MusicPlayer::lastSampleTick = 0;
double         MusicPlayer::pauseOffset = 0;
std::string    MusicPlayer::currentTrackPath = "";
double         MusicPlayer::trackDurationMs  = 0.0;
bool           MusicPlayer::isAdpMode  = false;
FILE*          MusicPlayer::adpFile    = nullptr;
uint32_t       MusicPlayer::adpTotalSamples = 0;
uint32_t       MusicPlayer::adpSamplesRead  = 0;
AdpcmDecoder::State MusicPlayer::adpDecState;

bool           MusicPlayer::isWavMode  = false;
FILE*          MusicPlayer::wavFile    = nullptr;
uint32_t       MusicPlayer::wavDataOffset = 0;
uint32_t       MusicPlayer::wavTotalSamples = 0;
uint32_t       MusicPlayer::wavSamplesRead  = 0;

void MusicPlayer::fillBuffer(int idx) {
    int16_t* buf = (int16_t*)waveBuf[idx].data_vaddr;
    uint32_t totalBytes = BUF_SMPLS * mChannels * sizeof(int16_t);
    
    if (isAdpMode) {
        if (!adpFile) {
            memset(buf, 0, totalBytes);
        } else {
            uint32_t samplesNeeded = BUF_SMPLS * mChannels;
            uint32_t bytesNeeded = samplesNeeded / 2; // 4bpp ADP
            uint8_t* adpTmp = (uint8_t*)malloc(bytesNeeded);
            if (adpTmp) {
                size_t r = fread(adpTmp, 1, bytesNeeded, adpFile);
                if (r > 0) {
                    uint32_t decoded = 0;
                    AdpcmDecoder::decodeIMA(adpTmp, (uint32_t)r, buf, decoded, adpDecState);
                    adpSamplesRead += decoded;
                } else {
                    fseek(adpFile, sizeof(AdpcmEncoder::Header), SEEK_SET);
                    adpDecState.predictor = 0;
                    adpDecState.stepIndex = 0;
                    size_t r2 = fread(adpTmp, 1, bytesNeeded, adpFile);
                    if (r2 > 0) {
                        uint32_t decoded = 0;
                        AdpcmDecoder::decodeIMA(adpTmp, (uint32_t)r2, buf, decoded, adpDecState);
                        adpSamplesRead += decoded;
                    } else {
                        memset(buf, 0, totalBytes);
                    }
                }
                free(adpTmp);
            } else {
                memset(buf, 0, totalBytes);
            }
        }
    } else if (isWavMode) {
        if (!wavFile) {
            memset(buf, 0, totalBytes);
        } else {
            uint32_t samplesNeeded = BUF_SMPLS * mChannels;
            uint32_t samplesToRead = std::min((uint32_t)samplesNeeded, wavTotalSamples - wavSamplesRead);
            
            if (samplesToRead > 0) {
                size_t bytesToRead = samplesToRead * sizeof(int16_t);
                size_t r = fread(buf, 1, bytesToRead, wavFile);
                wavSamplesRead += r / sizeof(int16_t);
                if (r < totalBytes) {
                    memset((char*)buf + r, 0, totalBytes - r);
                }
            } else {
                fseek(wavFile, wavDataOffset, SEEK_SET);
                wavSamplesRead = 0;
                size_t r = fread(buf, 1, totalBytes, wavFile);
                wavSamplesRead += r / sizeof(int16_t);
                if (r < totalBytes) {
                    memset((char*)buf + r, 0, totalBytes - r);
                }
            }
        }
    } else {
        uint32_t read = 0;
        int bitstream = 0;
        while (read < totalBytes) {
            long ret = ov_read(&vf, (char*)buf + read, totalBytes - read, &bitstream);
            if (ret < 0) {
                if (ret == OV_HOLE) continue;
                break;
            }
            if (ret == 0) {
                ov_raw_seek(&vf, 0); // loop back
                if (read == 0) { memset(buf, 0, totalBytes); break; }
                memset((char*)buf + read, 0, totalBytes - read);
                break;
            }
            read += ret;
        }
    }

    // Capture Realtime Audio PCM Waveform Samples for Visualizer Spectrogram
    float maxPeak = 0.001f;
    uint32_t totalSamples = BUF_SMPLS * mChannels;
    uint32_t stride = totalSamples / 64;
    if (stride < 1) stride = 1;
    for (int k = 0; k < 64; k++) {
        uint32_t sampleIdx = k * stride;
        if (sampleIdx < totalSamples) {
            float norm = (float)buf[sampleIdx] / 32768.0f;
            realtimePcm[k] = norm;
            float absVal = fabsf(norm);
            if (absVal > maxPeak) maxPeak = absVal;
        }
    }
    realtimePeak = maxPeak;

    DSP_FlushDataCache(buf, totalBytes);
    waveBuf[idx].nsamples = BUF_SMPLS;
    waveBuf[idx].status   = NDSP_WBUF_FREE;
    DSP_FlushDataCache(&waveBuf[idx], sizeof(ndspWaveBuf));
    ndspChnWaveBufAdd(CHANNEL, &waveBuf[idx]);
}

bool MusicPlayer::play(const char* path, float volume) {
    stop();
    LightLock_Lock(&s_musicLock);

    isAdpMode = false;
    isWavMode = false;
    adpFile = nullptr;
    wavFile = nullptr;

    std::string adpPath = path;
    if (adpPath.find(".ogg") != std::string::npos) {
        adpPath.replace(adpPath.find(".ogg"), 4, ".adp");
    }

    bool isAdp = false;
    AdpcmEncoder::Header adpHead;
    FILE* checkF = fopen(adpPath.c_str(), "rb");
    if (checkF) {
        if (fread(&adpHead, sizeof(adpHead), 1, checkF) == 1 &&
            memcmp(adpHead.magic, "SADP", 4) == 0 && adpHead.numSamples > 0) {
            isAdp = true;
        } else {
            fclose(checkF);
            checkF = nullptr;
        }
    }

    if (!isAdp && std::string(path) != adpPath) {
        checkF = fopen(path, "rb");
        if (checkF) {
            if (fread(&adpHead, sizeof(adpHead), 1, checkF) == 1 &&
                memcmp(adpHead.magic, "SADP", 4) == 0 && adpHead.numSamples > 0) {
                isAdp = true;
            } else {
                fclose(checkF);
                checkF = nullptr;
            }
        }
    }

    if (isAdp && checkF) {
        fseek(checkF, sizeof(AdpcmEncoder::Header), SEEK_SET);
        isAdpMode = true;
        adpFile = checkF;
        mChannels = adpHead.channels;
        mSampleRate = adpHead.sampleRate;
        adpTotalSamples = adpHead.numSamples;
        adpSamplesRead = 0;
        adpDecState.predictor = 0;
        adpDecState.stepIndex = 0;
        trackDurationMs = ((double)adpTotalSamples / (double)mSampleRate) * 1000.0;
    } else {
        if (checkF) { fclose(checkF); checkF = nullptr; }
        
        uint32_t wSR = 0;
        int wCh = 0;
        uint32_t wDO = 0;
        uint32_t wTS = 0;
        
        if (openWavFile(path, wavFile, wSR, wCh, wDO, wTS)) {
            isWavMode = true;
            mChannels = wCh;
            mSampleRate = wSR;
            wavDataOffset = wDO;
            wavTotalSamples = wTS;
            wavSamplesRead = 0;
            trackDurationMs = ((double)wavTotalSamples / (double)mSampleRate) * 1000.0;
        } else {
            FILE* f = fopen(path, "rb");
            if (!f) {
                FILE* log = fopen("sdmc:/snake_log.txt", "a");
                if(log) { fprintf(log, "MusicPlayer failed to open: %s\n", path); fclose(log); }
                LightLock_Unlock(&s_musicLock);
                return false;
            }
            memset(&vf, 0, sizeof(OggVorbis_File));
            int err = ov_open_callbacks(f, &vf, NULL, 0, s_vorbisCallbacks);
            if (err < 0) {
                FILE* log = fopen("sdmc:/snake_log.txt", "a");
                if(log) { fprintf(log, "MusicPlayer ov_open_callbacks failed with code %d for %s\n", err, path); fclose(log); }
                fclose(f);
                LightLock_Unlock(&s_musicLock);
                return false;
            }
            vorbis_info* vi = ov_info(&vf, -1);
            mChannels    = vi->channels;
            mSampleRate  = vi->rate;

            ogg_int64_t pcmDur = ov_pcm_total(&vf, -1);
            trackDurationMs = (pcmDur > 0) ? ((double)pcmDur / (double)mSampleRate * 1000.0) : 0.0;
        }
    }

    uint32_t bufSize = BUF_SMPLS * mChannels * sizeof(int16_t) * BUF_COUNT;
    audioData = (int16_t*)linearMemAlign(bufSize, 0x80);
    if (!audioData) {
        if (!isAdpMode) ov_clear(&vf);
        else if (adpFile) { fclose(adpFile); adpFile = nullptr; }
        LightLock_Unlock(&s_musicLock);
        return false;
    }

    memset(waveBuf, 0, sizeof(waveBuf));
    for (int i = 0; i < BUF_COUNT; i++)
        waveBuf[i].data_vaddr = audioData + i * BUF_SMPLS * mChannels;

    ndspChnSetPaused(CHANNEL, true); // Pause initially to avoid stuttering
    ndspChnReset(CHANNEL);
    ndspChnSetInterp(CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetRate(CHANNEL, (float)mSampleRate);
    ndspChnSetFormat(CHANNEL, mChannels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12]; memset(mix, 0, sizeof(mix));
    if (mChannels == 2) {
        mix[0] = volume; // Left -> Front Left
        mix[1] = 0.0f;
        mix[2] = 0.0f;
        mix[3] = volume; // Right -> Front Right
    } else {
        mix[0] = volume;
        mix[1] = volume;
    }
    ndspChnSetMix(CHANNEL, mix);

    loaded  = true;
    playing = true;
    mPaused = false;
    fillIdx = 0;
    currentTrackPath = path;

    totalSamples = 0;
    lastSampleTick = (double)osGetTime();
    pauseOffset = 0;

    for (int i = 0; i < BUF_COUNT; i++) fillBuffer(i);
    
    ndspChnSetPaused(CHANNEL, false); // Start playing now that we have pre-filled buffers
    LightLock_Unlock(&s_musicLock);
    return true;
}

void MusicPlayer::playMenuMusic() {
    if (loaded && playing && !mPaused && currentTrackPath == "romfs:/preload/music/freakyMenu.ogg") return; // already running
    if (mPaused && currentTrackPath == "romfs:/preload/music/freakyMenu.ogg") { resume(); return; }
    play("romfs:/preload/music/freakyMenu.ogg", 0.7f);
}

void MusicPlayer::stop() {
    LightLock_Lock(&s_musicLock);
    if (!loaded) {
        LightLock_Unlock(&s_musicLock);
        return;
    }
    ndspChnSetPaused(CHANNEL, true);
    ndspChnReset(CHANNEL);
    svcSleepThread(16000000LL);
    if (isAdpMode) {
        if (adpFile) { fclose(adpFile); adpFile = nullptr; }
        isAdpMode = false;
    } else if (isWavMode) {
        if (wavFile) { fclose(wavFile); wavFile = nullptr; }
        isWavMode = false;
    } else {
        ov_clear(&vf);
    }
    if (audioData) { linearFree(audioData); audioData = nullptr; }
    loaded = playing = mPaused = false;
    trackDurationMs = 0.0;
    memset(realtimePcm, 0, sizeof(realtimePcm));
    realtimePeak = 0.0f;
    LightLock_Unlock(&s_musicLock);
}

void MusicPlayer::pause() {
    LightLock_Lock(&s_musicLock);
    if (!loaded || mPaused) {
        LightLock_Unlock(&s_musicLock);
        return;
    }
    ndspChnSetPaused(CHANNEL, true);
    mPaused = true;
    pauseOffset = (double)osGetTime() - lastSampleTick;
    LightLock_Unlock(&s_musicLock);
}

void MusicPlayer::resume() {
    LightLock_Lock(&s_musicLock);
    if (!loaded || !mPaused) {
        LightLock_Unlock(&s_musicLock);
        return;
    }
    mPaused = false;
    lastSampleTick = (double)osGetTime() - pauseOffset;
    ndspChnSetPaused(CHANNEL, false);
    LightLock_Unlock(&s_musicLock);
}

bool MusicPlayer::isPlaying() { return loaded && playing && !mPaused; }

double MusicPlayer::getDuration() { return trackDurationMs; }

double MusicPlayer::getPosition() {
    if (!loaded || lastSampleTick == 0) return 0;
    double anchorMs = (double)totalSamples / (double)mSampleRate * 1000.0;
    double diff = mPaused ? pauseOffset : ((double)osGetTime() - lastSampleTick);
    return anchorMs + diff;
}

void MusicPlayer::getRealtimeWaveform(float* outWave, int count) {
    if (!outWave || count <= 0) return;
    LightLock_Lock(&s_musicLock);
    int maxPts = (count < 64) ? count : 64;
    for (int i = 0; i < maxPts; i++) {
        outWave[i] = loaded ? realtimePcm[i] : 0.0f;
    }
    for (int i = maxPts; i < count; i++) {
        outWave[i] = 0.0f;
    }
    LightLock_Unlock(&s_musicLock);
}

float MusicPlayer::getRealtimePeak() {
    return loaded ? realtimePeak : 0.0f;
}

void MusicPlayer::update() {
    LightLock_Lock(&s_musicLock);
    if (!loaded || !playing || mPaused) {
        LightLock_Unlock(&s_musicLock);
        return;
    }
    for (int i = 0; i < BUF_COUNT; i++) {
        if (waveBuf[i].status == NDSP_WBUF_DONE) {
            totalSamples += waveBuf[i].nsamples;
            lastSampleTick = (double)osGetTime();
            fillBuffer(i);
        }
    }
    LightLock_Unlock(&s_musicLock);
}

void MusicPlayer::setVolume(float volume) {
    LightLock_Lock(&s_musicLock);
    if (loaded) {
        float mix[12]; memset(mix, 0, sizeof(mix));
        if (mChannels == 2) {
            mix[0] = volume;
            mix[1] = 0.0f;
            mix[2] = 0.0f;
            mix[3] = volume;
        } else {
            mix[0] = volume;
            mix[1] = volume;
        }
        ndspChnSetMix(CHANNEL, mix);
    }
    LightLock_Unlock(&s_musicLock);
}
