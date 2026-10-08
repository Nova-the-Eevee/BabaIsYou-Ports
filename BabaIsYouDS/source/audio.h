#pragma once
#include <stdbool.h>

bool audioInit(void);
void audioShutdown(void);
void audioPlayMove(void);
void audioPlayWin(void);
void audioToggleMusic(void);
bool audioMusicEnabled(void);
