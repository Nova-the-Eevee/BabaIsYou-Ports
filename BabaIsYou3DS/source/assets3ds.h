#pragma once

#include <citro2d.h>
#include <stdbool.h>
#include "game.h"

typedef enum {
    SPR_BABARIGHT_0 = 0,
    SPR_BABARIGHT_1,
    SPR_BABARIGHT_2,
    SPR_BABALEFT_0,
    SPR_BABALEFT_1,
    SPR_BABALEFT_2,
    SPR_BABAUP_0,
    SPR_BABAUP_1,
    SPR_BABAUP_2,
    SPR_BABADOWN_0,
    SPR_BABADOWN_1,
    SPR_BABADOWN_2,
    SPR_WALL,
    SPR_ROCK,
    SPR_FLAG,
    SPR_TILE,
    SPR_TEXT_WALL,
    SPR_TEXT_IS,
    SPR_TEXT_STOP,
    SPR_TEXT_ROCK,
    SPR_TEXT_FLAG,
    SPR_TEXT_PUSH,
    SPR_TEXT_WIN,
    SPR_TEXT_BABA,
    SPR_TEXT_YOU,
    SPR_COUNT
} SpriteAsset;

bool assetsInit(void);
void assetsExit(void);
C2D_Image assetsImage(SpriteAsset asset);
C2D_Image assetsForObject(ObjectType type);
C2D_Image assetsBabaFrame(Facing facing, int animation);
