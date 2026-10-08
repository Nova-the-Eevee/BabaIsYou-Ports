#include "assets3ds.h"

static C2D_SpriteSheet sheet;

bool assetsInit(void)
{
    sheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
    if (!sheet)
        return false;
    if (C2D_SpriteSheetCount(sheet) < SPR_COUNT) {
        C2D_SpriteSheetFree(sheet);
        sheet = NULL;
        return false;
    }
    return true;
}

void assetsExit(void)
{
    if (sheet) {
        C2D_SpriteSheetFree(sheet);
        sheet = NULL;
    }
}

C2D_Image assetsImage(SpriteAsset asset)
{
    return C2D_SpriteSheetGetImage(sheet, (size_t)asset);
}

C2D_Image assetsForObject(ObjectType type)
{
    switch (type) {
        case OBJ_WALL: return assetsImage(SPR_WALL);
        case OBJ_ROCK: return assetsImage(SPR_ROCK);
        case OBJ_FLAG: return assetsImage(SPR_FLAG);
        case OBJ_TILE: return assetsImage(SPR_TILE);
        case OBJ_BABA: return assetsImage(SPR_BABADOWN_0);
        case OBJ_TEXT_WALL: return assetsImage(SPR_TEXT_WALL);
        case OBJ_TEXT_ROCK: return assetsImage(SPR_TEXT_ROCK);
        case OBJ_TEXT_FLAG: return assetsImage(SPR_TEXT_FLAG);
        case OBJ_TEXT_BABA: return assetsImage(SPR_TEXT_BABA);
        case OBJ_TEXT_IS: return assetsImage(SPR_TEXT_IS);
        case OBJ_TEXT_STOP: return assetsImage(SPR_TEXT_STOP);
        case OBJ_TEXT_PUSH: return assetsImage(SPR_TEXT_PUSH);
        case OBJ_TEXT_WIN: return assetsImage(SPR_TEXT_WIN);
        case OBJ_TEXT_YOU: return assetsImage(SPR_TEXT_YOU);
        default: return assetsImage(SPR_TILE);
    }
}

C2D_Image assetsBabaFrame(Facing facing, int animation)
{
    animation %= 3;
    int base;
    switch (facing) {
        case FACE_RIGHT: base = SPR_BABARIGHT_0; break;
        case FACE_LEFT:  base = SPR_BABALEFT_0;  break;
        case FACE_UP:    base = SPR_BABAUP_0;    break;
        default:         base = SPR_BABADOWN_0;  break;
    }
    return assetsImage((SpriteAsset)(base + animation));
}
