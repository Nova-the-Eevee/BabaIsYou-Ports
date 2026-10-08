#pragma once

#include <stdbool.h>
#include "game.h"

bool uiInit(void);
void uiPresent(void);
void uiDrawMap(const Game *g, int cameraX, int cameraY);
void uiDrawPauseMenu(int selected);
void uiDrawRules(const Game *g);
void uiDrawDebug(const Game *g, bool musicEnabled);
void uiDrawResetConfirm(void);
void uiDrawComplete(void);
void uiDrawFatal(const char *title, const char *line1, const char *line2);
int uiPauseButtonAt(int x, int y);
