#include "ui.h"
#include "assets3ds.h"
#include "render.h"
#include "font6x8.h"

#include <stdio.h>
#include <string.h>

#define BOTTOM_W 320
#define BOTTOM_H 240
#define FONT_CELL_W 6.0f
#define FONT_CELL_H 8.0f

static void begin(C3D_RenderTarget *target)
{
    C2D_TargetClear(target, C2D_Color32(15, 14, 22, 255));
    C2D_SceneBegin(target);
}

// Built-in 5x7 font renderer. This intentionally does NOT use C2D_TextBufNew()
// or the 3DS shared system font. That keeps the game bootable in emulators
// that don't have dumped system font data installed.
static float fontPixelSize(float scale)
{
    // The previous UI used Citro2D's system font at roughly a 30px base size.
    // Mapping scale to a 1.2..3px pixel keeps the same general proportions.
    float px = scale * 4.0f;
    if (px < 1.0f) px = 1.0f;
    return px;
}

static void drawGlyph(char ch, float x, float y, float px, u32 color)
{
    unsigned c = (unsigned char)ch;
    if (c < 32 || c > 126)
        c = '?';

    const uint8_t *glyph = ui_font6x8[c - 32];

    for (int gy = 0; gy < 8; gy++) {
        uint8_t bits = glyph[gy];
        int gx = 0;

        // Draw horizontal runs instead of one rectangle per pixel. This keeps
        // Citro2D object usage comfortably low even on the Debug/Rules pages.
        while (gx < 5) {
            while (gx < 5 && !(bits & (1u << (4 - gx))))
                gx++;
            if (gx >= 5)
                break;

            int start = gx;
            while (gx < 5 && (bits & (1u << (4 - gx))))
                gx++;

            C2D_DrawRectSolid(x + start * px, y + gy * px, 0.0f,
                              (gx - start) * px, px, color);
        }
    }
}

static float textWidth(const char *s, float scale)
{
    if (!s) return 0.0f;
    return strlen(s) * FONT_CELL_W * fontPixelSize(scale);
}

static void drawText(const char *s, float x, float y, float scale, u32 color)
{
    if (!s || !*s) return;

    float px = fontPixelSize(scale);
    float startX = x;

    for (; *s; s++) {
        if (*s == '\n') {
            x = startX;
            y += FONT_CELL_H * px;
            continue;
        }
        drawGlyph(*s, x, y, px, color);
        x += FONT_CELL_W * px;
    }
}

static void drawTextCentered(const char *s, float y, float scale, u32 color)
{
    if (!s || !*s) return;
    float w = textWidth(s, scale);
    drawText(s, (BOTTOM_W - w) * 0.5f, y, scale, color);
}

static void outline(float x, float y, float w, float h, float t, u32 color)
{
    C2D_DrawRectSolid(x, y, 0.0f, w, t, color);
    C2D_DrawRectSolid(x, y + h - t, 0.0f, w, t, color);
    C2D_DrawRectSolid(x, y, 0.0f, t, h, color);
    C2D_DrawRectSolid(x + w - t, y, 0.0f, t, h, color);
}

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool uiInit(void)
{
    // No external/system font resources are needed anymore.
    return true;
}

void uiExit(void)
{
}

static void drawMiniImage(C2D_Image image, float x, float y, float cell)
{
    float w = (float)image.subtex->width;
    float h = (float)image.subtex->height;
    float maxDim = w > h ? w : h;
    float scale = cell / maxDim;
    C2D_DrawImageAt(image, x, y, 0.0f, NULL, scale, scale);
}

void uiDrawMap(C3D_RenderTarget *target, const Game *g, int cameraX, int cameraY)
{
    begin(target);
    if (!g || g->levelWidth <= 0 || g->levelHeight <= 0)
        return;

    const int marginX = 24;
    const int marginY = 20;
    int cellX = (BOTTOM_W - marginX * 2) / g->levelWidth;
    int cellY = (BOTTOM_H - marginY * 2) / g->levelHeight;
    int cell = cellX < cellY ? cellX : cellY;
    if (cell > 14) cell = 14;
    if (cell < 4) cell = 4;

    int mapW = g->levelWidth * cell;
    int mapH = g->levelHeight * cell;
    int ox = (BOTTOM_W - mapW) / 2;
    int oy = (BOTTOM_H - mapH) / 2;

    outline((float)ox - 6, (float)oy - 6,
            (float)mapW + 12, (float)mapH + 12,
            2.0f, C2D_Color32(72, 68, 92, 255));

    const ObjectType passes[] = {
        OBJ_TILE, OBJ_WALL,
        OBJ_TEXT_WALL, OBJ_TEXT_ROCK, OBJ_TEXT_FLAG, OBJ_TEXT_BABA,
        OBJ_TEXT_IS, OBJ_TEXT_STOP, OBJ_TEXT_PUSH, OBJ_TEXT_WIN, OBJ_TEXT_YOU,
        OBJ_ROCK, OBJ_FLAG, OBJ_BABA
    };

    for (unsigned p = 0; p < sizeof(passes) / sizeof(passes[0]); p++) {
        ObjectType type = passes[p];
        C2D_Image image = assetsForObject(type);
        for (int i = 0; i < g->objectCount; i++) {
            const GameObject *obj = &g->objects[i];
            if (obj->type != type) continue;
            drawMiniImage(image,
                          (float)(ox + obj->gridX * cell),
                          (float)(oy + obj->gridY * cell),
                          (float)cell);
        }
    }

    int worldW = g->levelWidth * TILE_SIZE;
    int worldH = g->levelHeight * TILE_SIZE;
    int left = clampi(cameraX, 0, worldW);
    int top = clampi(cameraY, 0, worldH);
    int right = clampi(cameraX + TOP_SCREEN_W, 0, worldW);
    int bottom = clampi(cameraY + TOP_SCREEN_H, 0, worldH);

    float vx = (float)ox + ((float)left * cell) / TILE_SIZE;
    float vy = (float)oy + ((float)top * cell) / TILE_SIZE;
    float vw = ((float)(right - left) * cell) / TILE_SIZE;
    float vh = ((float)(bottom - top) * cell) / TILE_SIZE;
    if (vw > 1.0f && vh > 1.0f)
        outline(vx, vy, vw, vh, 1.5f, C2D_Color32(61, 200, 255, 255));

    char levelText[32];
    snprintf(levelText, sizeof(levelText), "LEVEL %d", g->level);
    drawText(levelText, 8.0f, 216.0f, 0.34f, C2D_Color32(200, 198, 214, 255));
    drawText("START: PAUSE", 222.0f, 216.0f, 0.27f,
             C2D_Color32(150, 147, 163, 255));
}

static const int buttonY[3] = {20, 95, 170};
static const char *buttonText[3] = {"CONTINUE", "DEBUG", "RULES"};

int uiPauseButtonAt(int x, int y)
{
    if (x < 20 || x >= 300) return -1;
    for (int i = 0; i < 3; i++)
        if (y >= buttonY[i] && y < buttonY[i] + 50)
            return i;
    return -1;
}

void uiDrawPauseMenu(C3D_RenderTarget *target, int selected)
{
    begin(target);
    for (int i = 0; i < 3; i++) {
        u32 fill = (i == selected)
                 ? C2D_Color32(105, 91, 238, 255)
                 : C2D_Color32(71, 63, 166, 255);
        u32 border = (i == selected)
                   ? C2D_Color32(74, 210, 255, 255)
                   : C2D_Color32(77, 73, 104, 255);
        C2D_DrawRectSolid(20.0f, (float)buttonY[i], 0.0f, 280.0f, 50.0f, fill);
        outline(20.0f, (float)buttonY[i], 280.0f, 50.0f, 2.0f, border);
        drawTextCentered(buttonText[i], (float)buttonY[i] + 13.0f,
                         0.58f, C2D_Color32(255, 255, 255, 255));
    }
}

static void formatRule(const Game *g, ObjectType type, char *out, size_t outSize)
{
    out[0] = '\0';
    uint8_t r = g->rules[type];
    if (!r) return;

    snprintf(out, outSize, "%s IS", gameObjectName(type));
    if ((r & PROP_YOU) && strlen(out) + 4 < outSize)
        strncat(out, " YOU", outSize - strlen(out) - 1);
    if ((r & PROP_STOP) && strlen(out) + 5 < outSize)
        strncat(out, " STOP", outSize - strlen(out) - 1);
    if ((r & PROP_PUSH) && strlen(out) + 5 < outSize)
        strncat(out, " PUSH", outSize - strlen(out) - 1);
    if ((r & PROP_WIN) && strlen(out) + 4 < outSize)
        strncat(out, " WIN", outSize - strlen(out) - 1);
}

void uiDrawRules(C3D_RenderTarget *target, const Game *g)
{
    begin(target);
    drawTextCentered("CURRENT RULES", 17.0f, 0.61f,
                     C2D_Color32(255, 255, 255, 255));
    C2D_DrawLine(30, 48, C2D_Color32(75, 200, 255, 255),
                 290, 48, C2D_Color32(75, 200, 255, 255), 2.0f, 0.0f);

    ObjectType types[] = {OBJ_BABA, OBJ_WALL, OBJ_ROCK, OBJ_FLAG};
    float y = 68.0f;
    int shown = 0;
    for (unsigned i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        char line[96];
        formatRule(g, types[i], line, sizeof(line));
        if (!line[0]) continue;
        drawTextCentered(line, y, 0.49f, C2D_Color32(255, 255, 255, 255));
        y += 33.0f;
        shown++;
    }
    if (!shown)
        drawTextCentered("NO ACTIVE RULES", 98.0f, 0.48f,
                         C2D_Color32(170, 166, 183, 255));

    drawTextCentered("B: BACK   START: RESUME", 214.0f, 0.31f,
                     C2D_Color32(165, 161, 178, 255));
}

void uiDrawDebug(C3D_RenderTarget *target, const Game *g,
                 bool musicEnabled, bool soundAvailable, float slider,
                 int cameraX, int cameraY)
{
    begin(target);
    drawTextCentered("DEBUG", 12.0f, 0.66f, C2D_Color32(255, 255, 255, 255));

    char s[128];
    float y = 50.0f;
#define DBG_LINE(...) do { \
        snprintf(s, sizeof(s), __VA_ARGS__); \
        drawText(s, 24.0f, y, 0.38f, C2D_Color32(230, 228, 238, 255)); \
        y += 24.0f; \
    } while (0)
    DBG_LINE("LEVEL: %d", g->level);
    DBG_LINE("MAP: %d X %d", g->levelWidth, g->levelHeight);
    DBG_LINE("OBJECTS: %d", g->objectCount);
    DBG_LINE("UNDO STATES: %d / %d", g->historyCount, MAX_HISTORY);
    DBG_LINE("CAMERA: %d, %d", cameraX, cameraY);
    DBG_LINE("3D SLIDER: %d%% (TABLETOP 3D)", (int)(slider * 100.0f + 0.5f));
    if (soundAvailable)
        DBG_LINE("MUSIC: %s", musicEnabled ? "ON" : "OFF");
    else
        DBG_LINE("AUDIO: NOT STARTED");
#undef DBG_LINE

    drawTextCentered(soundAvailable ? "X: MUSIC   B: BACK" : "Y: TRY AUDIO   B: BACK",
                     215.0f, 0.29f,
                     C2D_Color32(165, 161, 178, 255));
}

void uiDrawResetConfirm(C3D_RenderTarget *target)
{
    begin(target);
    drawTextCentered("RESET LEVEL?", 66.0f, 0.70f,
                     C2D_Color32(255, 255, 255, 255));
    drawTextCentered("A: YES", 127.0f, 0.56f,
                     C2D_Color32(115, 221, 130, 255));
    drawTextCentered("B: NO", 166.0f, 0.56f,
                     C2D_Color32(255, 120, 120, 255));
}

void uiDrawComplete(C3D_RenderTarget *target)
{
    begin(target);
    drawTextCentered("ALL LEVELS CLEAR!", 70.0f, 0.66f,
                     C2D_Color32(255, 255, 255, 255));
    drawTextCentered("A: RESTART FROM LEVEL 1", 129.0f, 0.36f,
                     C2D_Color32(130, 225, 148, 255));
    drawTextCentered("TINY BABA HAS REACHED THE 3DS :3", 182.0f, 0.28f,
                     C2D_Color32(176, 172, 190, 255));
}

void uiDrawBoot(C3D_RenderTarget *target, const char *stage, const char *detail)
{
    begin(target);
    drawTextCentered("BABA IS YOU 3DS", 62.0f, 0.58f,
                     C2D_Color32(255, 255, 255, 255));
    drawTextCentered(stage ? stage : "BOOTING", 112.0f, 0.46f,
                     C2D_Color32(110, 215, 255, 255));
    if (detail)
        drawTextCentered(detail, 151.0f, 0.30f,
                         C2D_Color32(185, 181, 200, 255));
}

void uiDrawFatal(C3D_RenderTarget *target, const char *title,
                 const char *line1, const char *line2)
{
    begin(target);
    drawTextCentered(title ? title : "ERROR", 50.0f, 0.66f,
                     C2D_Color32(255, 100, 100, 255));
    if (line1)
        drawTextCentered(line1, 106.0f, 0.33f,
                         C2D_Color32(255, 255, 255, 255));
    if (line2)
        drawTextCentered(line2, 140.0f, 0.29f,
                         C2D_Color32(190, 186, 202, 255));
    drawTextCentered("PRESS START", 205.0f, 0.32f,
                     C2D_Color32(190, 186, 202, 255));
}
