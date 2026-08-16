#include "AudioEngine.hpp"
#include <malloc.h>
#include <string>
#include <algorithm>

std::map<std::string, AudioEngine::SoundData> AudioEngine::soundCache;

// Instrumental
ndspWaveBuf AudioEngine::waveBuf[BUFFER_COUNT];
int16_t* AudioEngine::bufferData = nullptr;
OggVorbis_File AudioEngine::vf;
int AudioEngine::fillBuf = 0;
uint64_t AudioEngine::totalSamples = 0;

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

FILE* AudioEngine::vocalsAdpFile = nullptr;
bool AudioEngine::vocalsIsAdp = false;
AdpcmDecoder::State AudioEngine::vocalsAdpState;
uint32_t AudioEngine::vocalsAdpTotalSamples = 0;
uint32_t AudioEngine::vocalsAdpSamplesRead = 0;

bool AudioEngine::isLoaded = false;
bool AudioEngine::hasVocals = false;
bool AudioEngine::paused = false;
long AudioEngine::actualSampleRate = 44100;
double AudioEngine::pauseOffset = 0;
double AudioEngine::lastSampleTick = 0;

bool AudioEngine::init(const char* instPath, const char* vocalsPath) {
    std::string adpInstPath = instPath;
    if (adpInstPath.find(".ogg") != std::string::npos) {
        adpInstPath.replace(adpInstPath.find(".ogg"), 4, ".adp");
    }

    isAdp = false;
    actualSampleRate = 44100;
    int instChannels = 2;

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

    if (!isAdp) {
        FILE* fInst = fopen(instPath, "rb");
        if (!fInst) return false;
        if (ov_open(fInst, &vf, NULL, 0) < 0) {
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

    if (vocalsPath != nullptr) {
        std::string adpVocPath = vocalsPath;
        if (adpVocPath.find(".ogg") != std::string::npos) {
            adpVocPath.replace(adpVocPath.find(".ogg"), 4, ".adp");
        }

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

        if (!vocalsIsAdp) {
            FILE* fVoc = fopen(vocalsPath, "rb");
            if (fVoc) {
                if (ov_open(fVoc, &vocalsVf, NULL, 0) >= 0) {
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

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetFormat(0, instChannels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, actualSampleRate);

    bufferData = (int16_t*)linearAlloc(BUFFER_SAMPLES * 2 * BUFFER_COUNT * sizeof(int16_t));
    memset(waveBuf, 0, sizeof(waveBuf));

    if (hasVocals) {
        ndspChnSetFormat(1, vocalsChannels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
        ndspChnSetInterp(1, NDSP_INTERP_LINEAR);
        ndspChnSetRate(1, vocalsRate);
        vocalsBufferData = (int16_t*)linearAlloc(BUFFER_SAMPLES * 2 * BUFFER_COUNT * sizeof(int16_t));
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
        waveBuf[i].data_vaddr = &bufferData[i * BUFFER_SAMPLES * CHANNELS];
        waveBuf[i].nsamples = BUFFER_SAMPLES;
        fill(i, false, false); 
        ndspChnWaveBufAdd(0, &waveBuf[i]);

        if (hasVocals) {
            vocalsWaveBuf[i].data_vaddr = &vocalsBufferData[i * BUFFER_SAMPLES * CHANNELS];
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
    } else {
        OggVorbis_File* targetVf = isVocals ? &vocalsVf : &vf;
        while (samplesRead < BUFFER_SAMPLES) {
            int bitstream;
            long read = ov_read(targetVf, (char*)(ptr + samplesRead * CHANNELS), (BUFFER_SAMPLES - samplesRead) * CHANNELS * 2, &bitstream);
            if (read <= 0) break;
            samplesRead += read / (CHANNELS * 2);
        }
    }
    
    targetBuf->nsamples = samplesRead;
    DSP_FlushDataCache(targetBuf->data_vaddr, targetBuf->nsamples * CHANNELS * 2);
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
    return (double)ov_pcm_total(&vf, -1) / (double)actualSampleRate * 1000.0;
}

bool AudioEngine::isFinished() {
    if (!isLoaded) return true;
    bool eof = false;
    if (isAdp) {
        eof = (adpSamplesRead + 16 >= adpTotalSamples);
    } else {
        long long tell = ov_pcm_tell(&vf);
        long long total = ov_pcm_total(&vf, -1);
        eof = (tell >= 0 && total >= 0 && tell + 16 >= total);
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
    if (!isLoaded || paused) return;
    
    bool instEof = false;
    if (isAdp) {
        instEof = (adpSamplesRead + 16 >= adpTotalSamples);
    } else {
        long long tell = ov_pcm_tell(&vf);
        long long total = ov_pcm_total(&vf, -1);
        instEof = (tell >= 0 && total >= 0 && tell + 16 >= total);
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
        } else {
            long long tell = ov_pcm_tell(&vocalsVf);
            long long total = ov_pcm_total(&vocalsVf, -1);
            vocEof = (tell >= 0 && total >= 0 && tell + 16 >= total);
        }
        
        if (!vocEof && vocalsWaveBuf[vocalsFillBuf].status == NDSP_WBUF_DONE) {
            fill(vocalsFillBuf, false, true);
            ndspChnWaveBufAdd(1, &vocalsWaveBuf[vocalsFillBuf]);
            vocalsFillBuf = (vocalsFillBuf + 1) % BUFFER_COUNT;
        }
    }
}

void AudioEngine::exit() {
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
        } else {
            ov_clear(&vf);
        }
        
        if (hasVocals) {
            if (vocalsIsAdp) {
                if (vocalsAdpFile) fclose(vocalsAdpFile);
                vocalsAdpFile = nullptr;
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

    for (auto const& pair : soundCache) {
        if (pair.second.buffer) {
            linearFree(pair.second.buffer);
        }
    }
    soundCache.clear();
}

void AudioEngine::clearSoundCache() {
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
        if (ov_open(f, &sfxVf, NULL, 0) < 0) {
            fclose(f);
            return;
        }
        
        vorbis_info* vi = ov_info(&sfxVf, -1);
        long totalSamples = ov_pcm_total(&sfxVf, -1);
        sound.samplesPerChannel = totalSamples;
        sound.channels = vi->channels;
        sound.rate = vi->rate;
        
        uint32_t bufferSize = totalSamples * vi->channels * sizeof(int16_t);
        sound.buffer = (int16_t*)linearAlloc(bufferSize);
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
        
        soundCache[actualPath] = sound;
    } else {
        sound = it->second;
    }

    int ch = sndChannelIdx;
    sndChannelIdx++;
    if (sndChannelIdx > 4) sndChannelIdx = 2;
    
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, sound.rate);
    ndspChnSetFormat(ch, sound.channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    
    float mix[12];
    memset(mix, 0, sizeof(mix));
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
        missSfx[i].buffer = (int16_t*)linearAlloc(bufferSize);
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
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, missSfx[idx].rate);
    ndspChnSetFormat(ch, missSfx[idx].channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12]; memset(mix, 0, sizeof(mix));
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
    std::string files[4] = {"intro3.ogg", "intro2.ogg", "intro1.ogg", "introGo.ogg"};
    for (int i = 0; i < 4; i++) {
        if (countdownSfx[i].buffer) continue;
        std::string path = "romfs:/shared/sounds/" + files[i];
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
        countdownSfx[i].buffer = (int16_t*)linearAlloc(bufferSize);
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
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, countdownSfx[tick].rate);
    ndspChnSetFormat(ch, countdownSfx[tick].channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12]; memset(mix, 0, sizeof(mix));
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

void MusicPlayer::fillBuffer(int idx) {
    int16_t* buf = (int16_t*)waveBuf[idx].data_vaddr;
    uint32_t totalBytes = BUF_SMPLS * mChannels * sizeof(int16_t);
    uint32_t read = 0;
    int bitstream = 0;
    while (read < totalBytes) {
        long ret = ov_read(&vf, (char*)buf + read, totalBytes - read, &bitstream);
        if (ret <= 0) {
            ov_raw_seek(&vf, 0); // loop
            if (read == 0) { memset(buf, 0, totalBytes); break; }
            memset(buf + read, 0, totalBytes - read);
            break;
        }
        read += ret;
    }
    DSP_FlushDataCache(buf, totalBytes);
    waveBuf[idx].nsamples = BUF_SMPLS;
    waveBuf[idx].status   = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(CHANNEL, &waveBuf[idx]);
}

bool MusicPlayer::play(const char* path, float volume) {
    stop();
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    if (ov_open(f, &vf, NULL, 0) < 0) { fclose(f); return false; }
    vorbis_info* vi = ov_info(&vf, -1);
    mChannels    = vi->channels;
    mSampleRate  = vi->rate;

    uint32_t bufSize = BUF_SMPLS * mChannels * sizeof(int16_t) * BUF_COUNT;
    audioData = (int16_t*)linearAlloc(bufSize);
    if (!audioData) { ov_clear(&vf); return false; }

    memset(waveBuf, 0, sizeof(waveBuf));
    for (int i = 0; i < BUF_COUNT; i++)
        waveBuf[i].data_vaddr = audioData + i * BUF_SMPLS * mChannels;

    ndspChnReset(CHANNEL);
    ndspChnSetPaused(CHANNEL, true); // Pause initially to avoid stuttering
    ndspChnSetInterp(CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetRate(CHANNEL, (float)mSampleRate);
    ndspChnSetFormat(CHANNEL, mChannels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12]; memset(mix, 0, sizeof(mix));
    mix[0] = mix[1] = volume;
    ndspChnSetMix(CHANNEL, mix);

    loaded  = true;
    playing = true;
    mPaused = false;
    fillIdx = 0;
    currentTrackPath = path;

    // Cache track total duration in ms
    ogg_int64_t pcmDur = ov_pcm_total(&vf, -1);
    trackDurationMs = (pcmDur > 0) ? ((double)pcmDur / (double)mSampleRate * 1000.0) : 0.0;
    
    totalSamples = 0;
    lastSampleTick = (double)osGetTime();
    pauseOffset = 0;

    for (int i = 0; i < BUF_COUNT; i++) fillBuffer(i);
    
    ndspChnSetPaused(CHANNEL, false); // Start playing now that we have pre-filled buffers
    return true;
}

void MusicPlayer::playMenuMusic() {
    if (loaded && playing && !mPaused && currentTrackPath == "romfs:/preload/music/freakyMenu.ogg") return; // already running
    if (mPaused && currentTrackPath == "romfs:/preload/music/freakyMenu.ogg") { resume(); return; }
    play("romfs:/preload/music/freakyMenu.ogg", 0.7f);
}

void MusicPlayer::stop() {
    if (!loaded) return;
    ndspChnReset(CHANNEL);
    // Fixed 16ms sleep — same rationale as AudioEngine::exit().
    // Polling waveBuf.status after ndspChnReset causes infinite loops in Citra.
    svcSleepThread(16000000LL);
    ov_clear(&vf);
    if (audioData) { linearFree(audioData); audioData = nullptr; }
    loaded = playing = mPaused = false;
    trackDurationMs = 0.0;
}

void MusicPlayer::pause() {
    if (!loaded || mPaused) return;
    ndspChnSetPaused(CHANNEL, true);
    mPaused = true;
    pauseOffset = (double)osGetTime() - lastSampleTick;
}

void MusicPlayer::resume() {
    if (!loaded || !mPaused) return;
    mPaused = false;
    lastSampleTick = (double)osGetTime() - pauseOffset;
    ndspChnSetPaused(CHANNEL, false);
}

bool MusicPlayer::isPlaying() { return loaded && playing && !mPaused; }

double MusicPlayer::getDuration() { return trackDurationMs; }

double MusicPlayer::getPosition() {
    if (!loaded || lastSampleTick == 0) return 0;
    double anchorMs = (double)totalSamples / (double)mSampleRate * 1000.0;
    double diff = mPaused ? pauseOffset : ((double)osGetTime() - lastSampleTick);
    return anchorMs + diff;
}

void MusicPlayer::update() {
    if (!loaded || !playing || mPaused) return;
    for (int i = 0; i < BUF_COUNT; i++) {
        if (waveBuf[i].status == NDSP_WBUF_DONE) {
            totalSamples += waveBuf[i].nsamples;
            lastSampleTick = (double)osGetTime();
            fillBuffer(i);
        }
    }
}

void MusicPlayer::setVolume(float volume) {
    if (!loaded) return;
    float mix[12]; memset(mix, 0, sizeof(mix));
    mix[0] = mix[1] = volume;
    ndspChnSetMix(CHANNEL, mix);
}
