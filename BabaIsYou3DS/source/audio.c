#include "audio.h"

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CH_MUSIC 0
#define CH_MOVE  1
#define CH_WIN   2

typedef struct {
    s8 *data;
    u32 size;
    float rate;
    ndspWaveBuf wave;
} Sample;

static Sample music;
static Sample moveSfx;
static Sample winSfx;
static bool musicOn = true;
static bool ndspReady = false;

static bool loadRaw(const char *path, Sample *s, float rate)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long size = ftell(f);
    if (size <= 0) { fclose(f); return false; }
    rewind(f);

    s->data = (s8 *)linearAlloc((size_t)size);
    if (!s->data) { fclose(f); return false; }
    if (fread(s->data, 1, (size_t)size, f) != (size_t)size) {
        linearFree(s->data);
        s->data = NULL;
        fclose(f);
        return false;
    }
    fclose(f);

    s->size = (u32)size;
    s->rate = rate;
    DSP_FlushDataCache(s->data, s->size);
    memset(&s->wave, 0, sizeof(s->wave));
    s->wave.data_pcm8 = s->data;
    s->wave.nsamples = s->size;
    return true;
}

static void setupChannel(int ch, float rate, float volume)
{
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, rate);
    ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM8);

    float mix[12] = {0};
    mix[0] = volume;
    mix[1] = volume;
    ndspChnSetMix(ch, mix);
}

static void startMusic(void)
{
    if (!ndspReady || !musicOn || !music.data) return;
    ndspChnWaveBufClear(CH_MUSIC);
    setupChannel(CH_MUSIC, music.rate, 0.58f);
    memset(&music.wave, 0, sizeof(music.wave));
    music.wave.data_pcm8 = music.data;
    music.wave.nsamples = music.size;
    music.wave.looping = true;
    ndspChnWaveBufAdd(CH_MUSIC, &music.wave);
}

static void playOneShot(Sample *s, int channel, float volume)
{
    if (!ndspReady || !s->data) return;
    ndspChnWaveBufClear(channel);
    setupChannel(channel, s->rate, volume);
    memset(&s->wave, 0, sizeof(s->wave));
    s->wave.data_pcm8 = s->data;
    s->wave.nsamples = s->size;
    s->wave.looping = false;
    ndspChnWaveBufAdd(channel, &s->wave);
}

bool audioInit(void)
{
    Result rc = ndspInit();
    if (R_FAILED(rc)) {
        ndspReady = false;
        return false; // Game still runs; it will simply be silent.
    }

    ndspReady = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    loadRaw("romfs:/audio/move.raw", &moveSfx, 11025.0f);
    loadRaw("romfs:/audio/win.raw", &winSfx, 11025.0f);
    loadRaw("romfs:/audio/music.raw", &music, 8000.0f);
    startMusic();
    return true;
}

void audioShutdown(void)
{
    if (ndspReady) {
        ndspChnWaveBufClear(CH_MUSIC);
        ndspChnWaveBufClear(CH_MOVE);
        ndspChnWaveBufClear(CH_WIN);
    }

    if (music.data) linearFree(music.data);
    if (moveSfx.data) linearFree(moveSfx.data);
    if (winSfx.data) linearFree(winSfx.data);
    memset(&music, 0, sizeof(music));
    memset(&moveSfx, 0, sizeof(moveSfx));
    memset(&winSfx, 0, sizeof(winSfx));

    if (ndspReady) ndspExit();
    ndspReady = false;
}

void audioPlayMove(void)
{
    playOneShot(&moveSfx, CH_MOVE, 0.80f);
}

void audioPlayWin(void)
{
    playOneShot(&winSfx, CH_WIN, 0.90f);
}

void audioToggleMusic(void)
{
    musicOn = !musicOn;
    if (!ndspReady) return;
    if (musicOn)
        startMusic();
    else
        ndspChnWaveBufClear(CH_MUSIC);
}

bool audioMusicEnabled(void)
{
    return musicOn;
}

bool audioAvailable(void)
{
    return ndspReady;
}
