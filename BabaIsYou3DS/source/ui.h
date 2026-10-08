#pragma once

#include <citro2d.h>
#include <stdbool.h>
#include "game.h"

bool uiInit(void);
void uiExit(void);

void uiDrawMap(C3D_RenderTarget *target, const Game *g, int cameraX, int cameraY);
void uiDrawPauseMenu(C3D_RenderTarget *target, int selected);
void uiDrawRules(C3D_RenderTarget *target, const Game *g);
void uiDrawDebug(C3D_RenderTarget *target, const Game *g,
                 bool musicEnabled, bool soundAvailable, float slider,
                 int cameraX, int cameraY);
void uiDrawResetConfirm(C3D_RenderTarget *target);
void uiDrawComplete(C3D_RenderTarget *target);
void uiDrawFatal(C3D_RenderTarget *target, const char *title,
                 const char *line1, const char *line2);
void uiDrawBoot(C3D_RenderTarget *target, const char *stage, const char *detail);

int uiPauseButtonAt(int x, int y);
