#pragma once

#include <stdbool.h>
#include "game.h"

bool rendererInit(void);
void rendererResetCamera(const Game *g);
void rendererDraw(const Game *g);
void rendererGetCamera(int *x, int *y);
