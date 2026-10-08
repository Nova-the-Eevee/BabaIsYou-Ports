#include "ui.h"
#include "assets.h"
#include "font6x8.h"

#include <nds.h>
#include <stdio.h>
#include <string.h>

#define UI_W 256
#define UI_H 192
#define UI_STRIDE 256
#define MINI_SIZE 10
#define MINI_PALETTE_BASE 64

#define FONT_W 6
#define FONT_H 8
#define TEXT_COLS (UI_W / FONT_W)

// The sub screen uses the whole 128 KiB of VRAM C as two 64 KiB 8bpp
// bitmap pages. We always draw into the hidden page and flip it at VBlank.
#define PAGE_A 0
#define PAGE_B 4

enum {
    C_BG = 16,
    C_FRAME,
    C_FLOOR,
    C_WALL,
    C_WORD,
    C_ROCK,
    C_FLAG,
    C_BABA,
    C_VIEW,
    C_BUTTON,
    C_BUTTON_SEL,
    C_DIM,
    C_DANGER,
    C_OK,
    C_TEXT
};

static int bitmapBg;
static int frontPage;
static int backPage;
static uint16_t *fb;
static bool pendingFlip;

static uint16_t *pagePtr(int page)
{
    return BG_BMP_RAM_SUB(page);
}

static void selectBackBuffer(void)
{
    fb = pagePtr(backPage);
}

static void clearScreen(uint8_t color)
{
    uint16_t pair = (uint16_t)color | ((uint16_t)color << 8);
    dmaFillHalfWords(pair, fb, 256 * 256);
}

static void putPixel(int x, int y, uint8_t color)
{
    if ((unsigned)x >= UI_W || (unsigned)y >= UI_H)
        return;

    int pixel = y * UI_STRIDE + x;
    volatile uint16_t *dst = (volatile uint16_t *)fb + (pixel >> 1);
    uint16_t old = *dst;

    if (pixel & 1)
        old = (old & 0x00FFu) | ((uint16_t)color << 8);
    else
        old = (old & 0xFF00u) | color;

    *dst = old;
}

static void fillRect(int x, int y, int w, int h, uint8_t color)
{
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > UI_W) w = UI_W - x;
    if (y + h > UI_H) h = UI_H - y;
    if (w <= 0 || h <= 0) return;

    uint16_t pair = (uint16_t)color | ((uint16_t)color << 8);

    for (int yy = 0; yy < h; yy++) {
        int px = x;
        int remaining = w;

        if (px & 1) {
            putPixel(px, y + yy, color);
            px++;
            remaining--;
        }

        volatile uint16_t *dst = (volatile uint16_t *)fb
                               + ((y + yy) * UI_STRIDE + px) / 2;
        int pairs = remaining / 2;
        for (int i = 0; i < pairs; i++)
            dst[i] = pair;

        px += pairs * 2;
        remaining -= pairs * 2;

        if (remaining)
            putPixel(px, y + yy, color);
    }
}

static void drawRect(int x, int y, int w, int h, uint8_t color)
{
    if (w <= 0 || h <= 0) return;
    fillRect(x, y, w, 1, color);
    fillRect(x, y + h - 1, w, 1, color);
    fillRect(x, y, 1, h, color);
    fillRect(x + w - 1, y, 1, h, color);
}

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void drawCharPx(int x, int y, char ch, uint8_t color)
{
    unsigned c = (unsigned char)ch;
    if (c < 32 || c > 126)
        c = '?';

    const uint8_t *glyph = ui_font6x8[c - 32];
    for (int gy = 0; gy < FONT_H; gy++) {
        uint8_t bits = glyph[gy];
        for (int gx = 0; gx < FONT_W; gx++) {
            // Font rows store the leftmost glyph pixel in the high side of
            // the 5-bit pattern. Read them MSB-to-LSB so each character is
            // not mirrored horizontally. Bit 5 is unused spacing.
            if (gx < 5 && (bits & (1u << (4 - gx))))
                putPixel(x + gx, y + gy, color);
        }
    }
}

static void textPx(int x, int y, const char *s, uint8_t color)
{
    if (!s) return;
    for (; *s; s++, x += FONT_W)
        drawCharPx(x, y, *s, color);
}

static void textAt(int col, int row, const char *s)
{
    textPx(col * FONT_W, row * FONT_H, s, C_TEXT);
}

static void textCentered(int row, const char *s)
{
    if (!s) return;
    int len = (int)strlen(s);
    int x = (UI_W - len * FONT_W) / 2;
    if (x < 0) x = 0;
    textPx(x, row * FONT_H, s, C_TEXT);
}

static void finishFrame(void)
{
    pendingFlip = true;
}

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

static void drawMiniAsset(const ImageAsset *asset, int cellX, int cellY, int cell)
{
    if (!asset || !asset->mini || cell <= 0) return;

    int size = cell < MINI_SIZE ? cell : MINI_SIZE;
    // Keep the minimap art top-left anchored too. This matches the top-screen
    // sprite renderer and avoids objects appearing to drift within cells.
    int ox = cellX;
    int oy = cellY;

    for (int dy = 0; dy < size; dy++) {
        int sy = (dy * MINI_SIZE) / size;
        for (int dx = 0; dx < size; dx++) {
            int sx = (dx * MINI_SIZE) / size;
            uint8_t pi = asset->mini[sy * MINI_SIZE + sx];
            if (pi)
                putPixel(ox + dx, oy + dy, MINI_PALETTE_BASE + pi);
        }
    }
}

bool uiInit(void)
{
    bitmapBg = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, PAGE_A, 0);
    if (bitmapBg < 0)
        return false;

    frontPage = PAGE_A;
    backPage = PAGE_B;
    pendingFlip = false;
    bgSetMapBase(bitmapBg, frontPage);
    bgSetPriority(bitmapBg, 0);

    BG_PALETTE_SUB[C_BG]         = RGB15(2, 2, 3);
    BG_PALETTE_SUB[C_FRAME]      = RGB15(7, 7, 10);
    BG_PALETTE_SUB[C_FLOOR]      = RGB15(3, 4, 7);
    BG_PALETTE_SUB[C_WALL]       = RGB15(9, 10, 13);
    BG_PALETTE_SUB[C_WORD]       = RGB15(12, 9, 24);
    BG_PALETTE_SUB[C_ROCK]       = RGB15(25, 15, 2);
    BG_PALETTE_SUB[C_FLAG]       = RGB15(31, 27, 5);
    BG_PALETTE_SUB[C_BABA]       = RGB15(31, 31, 31);
    BG_PALETTE_SUB[C_VIEW]       = RGB15(10, 25, 31);
    BG_PALETTE_SUB[C_BUTTON]     = RGB15(9, 8, 21);
    BG_PALETTE_SUB[C_BUTTON_SEL] = RGB15(14, 12, 29);
    BG_PALETTE_SUB[C_DIM]        = RGB15(17, 17, 20);
    BG_PALETTE_SUB[C_DANGER]     = RGB15(31, 8, 8);
    BG_PALETTE_SUB[C_OK]         = RGB15(9, 28, 13);
    BG_PALETTE_SUB[C_TEXT]       = RGB15(31, 31, 31);

    for (int i = 0; i < 16; i++)
        BG_PALETTE_SUB[MINI_PALETTE_BASE + i] = sprite_palette[i];

    // Start with both pages cleared so the very first flip is clean.
    fb = pagePtr(PAGE_A);
    clearScreen(C_BG);
    fb = pagePtr(PAGE_B);
    clearScreen(C_BG);
    selectBackBuffer();
    return true;
}

void uiPresent(void)
{
    if (!pendingFlip)
        return;

    // Called immediately after swiWaitForVBlank(). The new page is already
    // complete, so changing the bitmap base is effectively instantaneous and
    // the LCD never sees us drawing into the visible page.
    bgSetMapBase(bitmapBg, backPage);

    int oldFront = frontPage;
    frontPage = backPage;
    backPage = oldFront;
    selectBackBuffer();
    pendingFlip = false;
}

void uiDrawMap(const Game *g, int cameraX, int cameraY)
{
    clearScreen(C_BG);
    if (!g || g->levelWidth <= 0 || g->levelHeight <= 0) {
        finishFrame();
        return;
    }

    const int marginX = 18;
    const int marginY = 18;
    int cellX = (UI_W - marginX * 2) / g->levelWidth;
    int cellY = (UI_H - marginY * 2) / g->levelHeight;
    int cell = cellX < cellY ? cellX : cellY;
    if (cell > 11) cell = 11;
    if (cell < 3) cell = 3;

    int mapW = g->levelWidth * cell;
    int mapH = g->levelHeight * cell;
    int ox = (UI_W - mapW) / 2;
    int oy = (UI_H - mapH) / 2;

    drawRect(ox - 4, oy - 4, mapW + 8, mapH + 8, C_FRAME);

    const ObjectType passes[] = {
        OBJ_TILE, OBJ_WALL,
        OBJ_TEXT_WALL, OBJ_TEXT_ROCK, OBJ_TEXT_FLAG, OBJ_TEXT_BABA,
        OBJ_TEXT_IS, OBJ_TEXT_STOP, OBJ_TEXT_PUSH, OBJ_TEXT_WIN, OBJ_TEXT_YOU,
        OBJ_ROCK, OBJ_FLAG, OBJ_BABA
    };

    for (unsigned p = 0; p < sizeof(passes) / sizeof(passes[0]); p++) {
        ObjectType t = passes[p];
        const ImageAsset *asset = assetForObject(t);
        for (int i = 0; i < g->objectCount; i++) {
            const GameObject *obj = &g->objects[i];
            if (obj->type != t) continue;
            drawMiniAsset(asset,
                          ox + obj->gridX * cell,
                          oy + obj->gridY * cell,
                          cell);
        }
    }

    int worldW = g->levelWidth * TILE_SIZE;
    int worldH = g->levelHeight * TILE_SIZE;
    int left = clampi(cameraX, 0, worldW);
    int top = clampi(cameraY, 0, worldH);
    int right = clampi(cameraX + SCREEN_WIDTH, 0, worldW);
    int bottom = clampi(cameraY + SCREEN_HEIGHT, 0, worldH);

    int vx = ox + (left * cell) / TILE_SIZE;
    int vy = oy + (top * cell) / TILE_SIZE;
    int vw = ((right - left) * cell + TILE_SIZE - 1) / TILE_SIZE;
    int vh = ((bottom - top) * cell + TILE_SIZE - 1) / TILE_SIZE;
    if (vw > 0 && vh > 0)
        drawRect(vx, vy, vw, vh, C_VIEW);

    finishFrame();
}

static const int buttonY[3] = {16, 76, 136};
static const char *buttonText[3] = {"CONTINUE", "DEBUG", "RULES"};

int uiPauseButtonAt(int x, int y)
{
    if (x < 16 || x >= 240) return -1;
    for (int i = 0; i < 3; i++)
        if (y >= buttonY[i] && y < buttonY[i] + 40)
            return i;
    return -1;
}

void uiDrawPauseMenu(int selected)
{
    clearScreen(C_BG);

    for (int i = 0; i < 3; i++) {
        uint8_t c = (i == selected) ? C_BUTTON_SEL : C_BUTTON;
        fillRect(16, buttonY[i], 224, 40, c);
        drawRect(16, buttonY[i], 224, 40, i == selected ? C_VIEW : C_FRAME);
        int y = buttonY[i] + (40 - FONT_H) / 2;
        int x = (UI_W - (int)strlen(buttonText[i]) * FONT_W) / 2;
        textPx(x, y, buttonText[i], C_TEXT);
    }

    finishFrame();
}

static void makeRuleLine(const Game *g, ObjectType type, char *out, size_t outSize)
{
    uint8_t r = g->rules[type];
    if (!r) { out[0] = '\0'; return; }

    snprintf(out, outSize, "%s IS", gameObjectName(type));
    if (r & PROP_YOU)  strncat(out, " YOU",  outSize - strlen(out) - 1);
    if (r & PROP_STOP) strncat(out, " STOP", outSize - strlen(out) - 1);
    if (r & PROP_PUSH) strncat(out, " PUSH", outSize - strlen(out) - 1);
    if (r & PROP_WIN)  strncat(out, " WIN",  outSize - strlen(out) - 1);
}

void uiDrawRules(const Game *g)
{
    clearScreen(C_BG);
    drawRect(16, 38, 224, 116, C_FRAME);

    textCentered(2, "CURRENT RULES");

    ObjectType subjects[] = {OBJ_BABA, OBJ_WALL, OBJ_ROCK, OBJ_FLAG};
    int row = 7;
    int count = 0;
    char line[64];

    for (unsigned i = 0; i < sizeof(subjects) / sizeof(subjects[0]); i++) {
        makeRuleLine(g, subjects[i], line, sizeof(line));
        if (!line[0]) continue;
        textAt(4, row, line);
        row += 3;
        count++;
    }

    if (!count)
        textCentered(11, "NO ACTIVE RULES");

    textCentered(22, "B BACK   START RESUME");
    finishFrame();
}

void uiDrawDebug(const Game *g, bool musicEnabled)
{
    clearScreen(C_BG);
    drawRect(24, 38, 208, 116, C_FRAME);

    textCentered(2, "DEBUG");

    char line[48];
    snprintf(line, sizeof(line), "LEVEL: %d", g->level);
    textAt(5, 7, line);
    snprintf(line, sizeof(line), "SIZE: %dX%d", g->levelWidth, g->levelHeight);
    textAt(5, 9, line);
    snprintf(line, sizeof(line), "OBJECTS: %d", g->objectCount);
    textAt(5, 11, line);
    snprintf(line, sizeof(line), "UNDO STATES: %d", g->historyCount);
    textAt(5, 13, line);
    snprintf(line, sizeof(line), "MUSIC: %s", musicEnabled ? "ON" : "OFF");
    textAt(5, 15, line);

    textCentered(22, "B BACK  X MUSIC  START RESUME");
    finishFrame();
}

void uiDrawResetConfirm(void)
{
    clearScreen(C_BG);
    fillRect(22, 94, 96, 42, C_OK);
    fillRect(138, 94, 96, 42, C_BUTTON);
    drawRect(22, 94, 96, 42, C_FRAME);
    drawRect(138, 94, 96, 42, C_FRAME);

    textCentered(5, "RESET LEVEL?");
    textAt(6, 14, "YES");
    textAt(21, 14, "NO");
    textCentered(20, "A YES      B NO");
    finishFrame();
}

void uiDrawComplete(void)
{
    clearScreen(C_BG);
    textCentered(6, "ALL LEVELS CLEAR!");
    textCentered(12, "A RESTART");
    textCentered(15, "SELECT EXIT");
    finishFrame();
}

void uiDrawFatal(const char *title, const char *line1, const char *line2)
{
    clearScreen(C_BG);
    fillRect(12, 18, 232, 34, C_DANGER);
    textCentered(4, title ? title : "ERROR");
    if (line1) textCentered(11, line1);
    if (line2) textCentered(13, line2);
    textCentered(20, "PRESS START");
    finishFrame();
}
