#include "render.h"
#include "assets.h"

#include <nds.h>

#define HW_SPRITE_SIZE 32
#define MAX_OAM_SPRITES 128

static int cameraX;
static int cameraY;
static bool cameraValid;
static u16 *assetGfx[SPRITE_ASSET_COUNT];

static const ImageAsset *assetForObject(ObjectType type)
{
    switch (type) {
        case OBJ_WALL: return &wallobj;
        case OBJ_ROCK: return &rock;
        case OBJ_FLAG: return &flag;
        case OBJ_TILE: return &tile;
        case OBJ_BABA: return &babadown_0;
        case OBJ_TEXT_WALL: return &text_wall;
        case OBJ_TEXT_ROCK: return &text_rock;
        case OBJ_TEXT_FLAG: return &text_flag;
        case OBJ_TEXT_BABA: return &text_baba;
        case OBJ_TEXT_IS: return &text_is;
        case OBJ_TEXT_STOP: return &text_stop;
        case OBJ_TEXT_PUSH: return &text_push;
        case OBJ_TEXT_WIN: return &text_win;
        case OBJ_TEXT_YOU: return &text_you;
        default: return NULL;
    }
}

static const ImageAsset *babaFrame(Facing facing, int animation)
{
    animation %= 3;
    if (facing == FACE_RIGHT) {
        const ImageAsset *frames[] = {&babaright_0, &babaright_1, &babaright_2};
        return frames[animation];
    }
    if (facing == FACE_LEFT) {
        const ImageAsset *frames[] = {&babaleft_0, &babaleft_1, &babaleft_2};
        return frames[animation];
    }
    if (facing == FACE_UP) {
        const ImageAsset *frames[] = {&babaup_0, &babaup_1, &babaup_2};
        return frames[animation];
    }
    const ImageAsset *frames[] = {&babadown_0, &babadown_1, &babadown_2};
    return frames[animation];
}

static int assetIndex(const ImageAsset *asset)
{
    for (int i = 0; i < SPRITE_ASSET_COUNT; i++) {
        if (sprite_assets[i] == asset)
            return i;
    }
    return -1;
}

static u16 *gfxForAsset(const ImageAsset *asset)
{
    int i = assetIndex(asset);
    return (i >= 0) ? assetGfx[i] : NULL;
}

static int objectPriority(ObjectType type)
{
    // Smaller OAM priority numbers are drawn in front.
    if (gameIsText(type))
        return 1;
    if (type == OBJ_TILE)
        return 3;
    if (type == OBJ_BABA)
        return 0;
    return 2;
}

static bool spriteVisible(int x, int y)
{
    return x > -HW_SPRITE_SIZE && x < SCREEN_WIDTH &&
           y > -HW_SPRITE_SIZE && y < SCREEN_HEIGHT;
}

static void submitSprite(const ImageAsset *asset, int worldX, int worldY,
                         int priority, int *spriteCount)
{
    if (!asset || *spriteCount >= MAX_OAM_SPRITES)
        return;

    // Source graphics are top-left anchored inside their 32x32 OBJ slot, just
    // like the PSP version: the image's pixel (0, 0) is the gameplay cell's
    // pixel (0, 0). Smaller 24x24 art is padded only on the right/bottom.
    int x = worldX - cameraX;
    int y = worldY - cameraY;

    if (!spriteVisible(x, y))
        return;

    u16 *gfx = gfxForAsset(asset);
    if (!gfx)
        return;

    oamSet(&oamMain, *spriteCount,
           x, y,
           priority, 0,
           SpriteSize_32x32, SpriteColorFormat_16Color,
           gfx,
           -1,
           false, false,
           false, false,
           false);
    (*spriteCount)++;
}

static void cameraTarget(const Game *g, int *outX, int *outY)
{
    int focusX = (g->levelWidth * TILE_SIZE) / 2;
    int focusY = (g->levelHeight * TILE_SIZE) / 2;

    for (int i = 0; i < g->objectCount; i++) {
        const GameObject *obj = &g->objects[i];
        if (gameHasProperty(g, obj->type, PROP_YOU)) {
            focusX = ((obj->renderX * TILE_SIZE) >> FIX_SHIFT) + TILE_SIZE / 2;
            focusY = ((obj->renderY * TILE_SIZE) >> FIX_SHIFT) + TILE_SIZE / 2;
            break;
        }
    }

    int worldW = g->levelWidth * TILE_SIZE;
    int worldH = g->levelHeight * TILE_SIZE;
    int tx, ty;

    if (worldW <= SCREEN_WIDTH) {
        // Negative camera values center small maps.
        tx = -(SCREEN_WIDTH - worldW) / 2;
    } else {
        tx = focusX - SCREEN_WIDTH / 2;
        if (tx < 0) tx = 0;
        int maxX = worldW - SCREEN_WIDTH;
        if (tx > maxX) tx = maxX;
    }

    if (worldH <= SCREEN_HEIGHT) {
        ty = -(SCREEN_HEIGHT - worldH) / 2;
    } else {
        ty = focusY - SCREEN_HEIGHT / 2;
        if (ty < 0) ty = 0;
        int maxY = worldH - SCREEN_HEIGHT;
        if (ty > maxY) ty = maxY;
    }

    *outX = tx;
    *outY = ty;
}

bool rendererInit(void)
{
    cameraX = cameraY = 0;
    cameraValid = false;

    oamInit(&oamMain, SpriteMapping_1D_128, false);

    // Palette index 0 is transparent for 4bpp OBJ sprites.
    dmaCopy(sprite_palette, SPRITE_PALETTE, sizeof(sprite_palette));

    // Upload each unique image once. Every on-screen object can then point to
    // the same VRAM tile data, so 20 walls don't need 20 copies of wall art.
    for (int i = 0; i < SPRITE_ASSET_COUNT; i++) {
        assetGfx[i] = oamAllocateGfx(&oamMain,
                                     SpriteSize_32x32,
                                     SpriteColorFormat_16Color);
        if (!assetGfx[i])
            return false;
        dmaCopy(sprite_assets[i]->tiles, assetGfx[i], 512);
    }

    // Hide all OAM entries until the first rendererDraw().
    for (int i = 0; i < MAX_OAM_SPRITES; i++)
        oamSetHidden(&oamMain, i, true);
    oamUpdate(&oamMain);

    return true;
}

void rendererResetCamera(const Game *g)
{
    cameraTarget(g, &cameraX, &cameraY);
    cameraValid = true;
}

void rendererDraw(const Game *g)
{
    int targetX, targetY;
    cameraTarget(g, &targetX, &targetY);

    if (!cameraValid) {
        cameraX = targetX;
        cameraY = targetY;
        cameraValid = true;
    } else {
        cameraX += (targetX - cameraX) / 4;
        cameraY += (targetY - cameraY) / 4;
    }

    int spriteCount = 0;

    // Pass 1: floor/decorative tiles.
    for (int i = 0; i < g->objectCount; i++) {
        const GameObject *obj = &g->objects[i];
        if (obj->type != OBJ_TILE)
            continue;

        int worldX = (obj->renderX * TILE_SIZE) >> FIX_SHIFT;
        int worldY = (obj->renderY * TILE_SIZE) >> FIX_SHIFT;
        submitSprite(&tile, worldX, worldY, 3, &spriteCount);
    }

    // Pass 2: objects and word blocks. Active Baba is handled by the animation
    // pass so it can use the correct directional frame.
    for (int i = 0; i < g->objectCount; i++) {
        const GameObject *obj = &g->objects[i];
        if (obj->type == OBJ_TILE)
            continue;
        if (obj->type == OBJ_BABA && gameHasProperty(g, OBJ_BABA, PROP_YOU))
            continue;

        int worldX = (obj->renderX * TILE_SIZE) >> FIX_SHIFT;
        int worldY = (obj->renderY * TILE_SIZE) >> FIX_SHIFT;
        submitSprite(assetForObject(obj->type), worldX, worldY,
                     objectPriority(obj->type), &spriteCount);
    }

    // Pass 3: directional animation for every Baba that currently has YOU.
    for (int i = 0; i < g->objectCount; i++) {
        const GameObject *obj = &g->objects[i];
        if (obj->type != OBJ_BABA || !gameHasProperty(g, OBJ_BABA, PROP_YOU))
            continue;

        int worldX = (obj->renderX * TILE_SIZE) >> FIX_SHIFT;
        int worldY = (obj->renderY * TILE_SIZE) >> FIX_SHIFT;
        submitSprite(babaFrame(g->facing, g->animation), worldX, worldY,
                     0, &spriteCount);
    }

    // Anything used last frame but not this frame must be explicitly hidden.
    for (int i = spriteCount; i < MAX_OAM_SPRITES; i++)
        oamSetHidden(&oamMain, i, true);

    oamUpdate(&oamMain);
}

void rendererGetCamera(int *x, int *y)
{
    if (x) *x = cameraX;
    if (y) *y = cameraY;
}
