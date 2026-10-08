#pragma once
#include <stdint.h>

#define SPRITE_ASSET_COUNT 25

typedef struct {
    const uint8_t *tiles;
    const uint8_t *mini;
} ImageAsset;

extern const uint16_t sprite_palette[16];

extern const uint8_t babaright_0_tiles[512];
extern const uint8_t babaright_0_mini[100];
extern const ImageAsset babaright_0;
extern const uint8_t babaright_1_tiles[512];
extern const uint8_t babaright_1_mini[100];
extern const ImageAsset babaright_1;
extern const uint8_t babaright_2_tiles[512];
extern const uint8_t babaright_2_mini[100];
extern const ImageAsset babaright_2;
extern const uint8_t babaleft_0_tiles[512];
extern const uint8_t babaleft_0_mini[100];
extern const ImageAsset babaleft_0;
extern const uint8_t babaleft_1_tiles[512];
extern const uint8_t babaleft_1_mini[100];
extern const ImageAsset babaleft_1;
extern const uint8_t babaleft_2_tiles[512];
extern const uint8_t babaleft_2_mini[100];
extern const ImageAsset babaleft_2;
extern const uint8_t babaup_0_tiles[512];
extern const uint8_t babaup_0_mini[100];
extern const ImageAsset babaup_0;
extern const uint8_t babaup_1_tiles[512];
extern const uint8_t babaup_1_mini[100];
extern const ImageAsset babaup_1;
extern const uint8_t babaup_2_tiles[512];
extern const uint8_t babaup_2_mini[100];
extern const ImageAsset babaup_2;
extern const uint8_t babadown_0_tiles[512];
extern const uint8_t babadown_0_mini[100];
extern const ImageAsset babadown_0;
extern const uint8_t babadown_1_tiles[512];
extern const uint8_t babadown_1_mini[100];
extern const ImageAsset babadown_1;
extern const uint8_t babadown_2_tiles[512];
extern const uint8_t babadown_2_mini[100];
extern const ImageAsset babadown_2;
extern const uint8_t wallobj_tiles[512];
extern const uint8_t wallobj_mini[100];
extern const ImageAsset wallobj;
extern const uint8_t rock_tiles[512];
extern const uint8_t rock_mini[100];
extern const ImageAsset rock;
extern const uint8_t flag_tiles[512];
extern const uint8_t flag_mini[100];
extern const ImageAsset flag;
extern const uint8_t tile_tiles[512];
extern const uint8_t tile_mini[100];
extern const ImageAsset tile;
extern const uint8_t text_wall_tiles[512];
extern const uint8_t text_wall_mini[100];
extern const ImageAsset text_wall;
extern const uint8_t text_is_tiles[512];
extern const uint8_t text_is_mini[100];
extern const ImageAsset text_is;
extern const uint8_t text_stop_tiles[512];
extern const uint8_t text_stop_mini[100];
extern const ImageAsset text_stop;
extern const uint8_t text_rock_tiles[512];
extern const uint8_t text_rock_mini[100];
extern const ImageAsset text_rock;
extern const uint8_t text_flag_tiles[512];
extern const uint8_t text_flag_mini[100];
extern const ImageAsset text_flag;
extern const uint8_t text_push_tiles[512];
extern const uint8_t text_push_mini[100];
extern const ImageAsset text_push;
extern const uint8_t text_win_tiles[512];
extern const uint8_t text_win_mini[100];
extern const ImageAsset text_win;
extern const uint8_t text_baba_tiles[512];
extern const uint8_t text_baba_mini[100];
extern const ImageAsset text_baba;
extern const uint8_t text_you_tiles[512];
extern const uint8_t text_you_mini[100];
extern const ImageAsset text_you;

extern const ImageAsset *const sprite_assets[SPRITE_ASSET_COUNT];
