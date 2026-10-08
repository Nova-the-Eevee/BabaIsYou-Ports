#include "audio.h"

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    void *data;
    unsigned int size;
    unsigned int rate;
} Sample;

static Sample music;
static Sample moveSfx;
static Sample winSfx;
static int musicChannel = -1;
static bool musicOn = true;

static bool loadRaw(const char *path, Sample *sample, unsigned int rate)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long size = ftell(f);
    if (size <= 0) { fclose(f); return false; }
    rewind(f);

    sample->data = malloc((size_t)size);
    if (!sample->data) { fclose(f); return false; }
    if (fread(sample->data, 1, (size_t)size, f) != (size_t)size) {
        free(sample->data);
        sample->data = NULL;
        fclose(f);
        return false;
    }
    fclose(f);

    sample->size = (unsigned int)size;
    sample->rate = rate;
    DC_FlushRange(sample->data, sample->size);
    return true;
}

static void startMusic(void)
{
    if (!musicOn || !music.data) return;
    musicChannel = soundPlaySample(music.data, SoundFormat_8Bit, music.size,
                                   music.rate, 72, 64, true, 0);
}

bool audioInit(void)
{
    soundEnable();
    bool okMove = loadRaw("nitro:/audio/move.raw", &moveSfx, 11025);
    bool okWin = loadRaw("nitro:/audio/win.raw", &winSfx, 11025);
    bool okMusic = loadRaw("nitro:/audio/music.raw", &music, 8000);
    if (okMusic) startMusic();
    return okMove || okWin || okMusic;
}

void audioShutdown(void)
{
    if (musicChannel >= 0) soundKill(musicChannel);
    free(music.data);
    free(moveSfx.data);
    free(winSfx.data);
    music.data = moveSfx.data = winSfx.data = NULL;
}

void audioPlayMove(void)
{
    if (moveSfx.data)
        soundPlaySample(moveSfx.data, SoundFormat_8Bit, moveSfx.size,
                        moveSfx.rate, 100, 64, false, 0);
}

void audioPlayWin(void)
{
    if (winSfx.data)
        soundPlaySample(winSfx.data, SoundFormat_8Bit, winSfx.size,
                        winSfx.rate, 110, 64, false, 0);
}

void audioToggleMusic(void)
{
    musicOn = !musicOn;
    if (!musicOn) {
        if (musicChannel >= 0) {
            soundKill(musicChannel);
            musicChannel = -1;
        }
    } else {
        startMusic();
    }
}

bool audioMusicEnabled(void)
{
    return musicOn;
}
