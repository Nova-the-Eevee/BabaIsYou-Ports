#pragma once

#include <citro2d.h>
#include "game.h"

#define TOP_SCREEN_W 400
#define TOP_SCREEN_H 240

void rendererResetCamera(const Game *g);
void rendererUpdateCamera(const Game *g);
void rendererGetCamera(int *x, int *y);
void rendererDrawTop(const Game *g, C3D_RenderTarget *target,
                     float eyeSign, float slider);
