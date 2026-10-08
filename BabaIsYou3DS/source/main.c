#include <3ds.h>
#include <citro2d.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "assets3ds.h"
#include "audio.h"
#include "game.h"
#include "render.h"
#include "ui.h"

typedef enum {
    PAUSE_MENU = 0,
    PAUSE_DEBUG,
    PAUSE_RULES
} PausePage;

static C3D_RenderTarget *topLeft;
static C3D_RenderTarget *topRight;
static C3D_RenderTarget *bottom;

// IMPORTANT: Game is ~197 KiB because it contains the undo history.
// Never put this on the 3DS main thread stack. Keep it in .bss instead.
static Game game;

// Give the remaining call stack some breathing room too. The giant Game state
// is no longer on it, but Citro2D/libctru plus our helpers benefit from more
// than the very small default.
u32 __stacksize__ = 128 * 1024;

static float sliderValue(void)
{
    float v = osGet3DSliderState();
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

// Draw a frame before touching optional subsystems. This is deliberately kept
// stereo-free and asset-free so startup hangs can be localized on emulators.
static void bootFrame(const char *stage, const char *detail)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(topLeft, C2D_Color32(8, 8, 11, 255));
    C2D_SceneBegin(topLeft);
    C2D_DrawRectSolid(70.0f, 107.0f, 0.0f, 260.0f, 26.0f,
                      C2D_Color32(42, 39, 63, 255));
    C2D_DrawRectSolid(74.0f, 111.0f, 0.0f, 252.0f, 18.0f,
                      C2D_Color32(85, 74, 201, 255));

    uiDrawBoot(bottom, stage, detail);
    C3D_FrameEnd(0);
}

static void drawBottom(const Game *game, bool paused, bool resetConfirm,
                       PausePage pausePage, int pauseSelection)
{
    if (game->completed) {
        uiDrawComplete(bottom);
        return;
    }

    if (paused) {
        if (pausePage == PAUSE_RULES) {
            uiDrawRules(bottom, game);
        } else if (pausePage == PAUSE_DEBUG) {
            int cx, cy;
            rendererGetCamera(&cx, &cy);
            uiDrawDebug(bottom, game, audioMusicEnabled(), audioAvailable(),
                        sliderValue(), cx, cy);
        } else {
            uiDrawPauseMenu(bottom, pauseSelection);
        }
        return;
    }

    if (resetConfirm) {
        uiDrawResetConfirm(bottom);
        return;
    }

    int cameraX, cameraY;
    rendererGetCamera(&cameraX, &cameraY);
    uiDrawMap(bottom, game, cameraX, cameraY);
}

static void renderFrame(const Game *game, bool paused, bool resetConfirm,
                        PausePage pausePage, int pauseSelection)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    const float slider = sliderValue();

    // Real stereoscopic rendering. The tabletop projection is applied to both
    // eyes; only the stereo disparity changes sign between them.
    rendererDrawTop(game, topLeft, -1.0f, slider);
    if (topRight && slider > 0.001f)
        rendererDrawTop(game, topRight, +1.0f, slider);

    drawBottom(game, paused, resetConfirm, pausePage, pauseSelection);

    C3D_FrameEnd(0);
}

static void fatalLoop(const char *title, const char *line1, const char *line2)
{
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & (KEY_START | KEY_B))
            break;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(topLeft, C2D_Color32(8, 8, 11, 255));
        C2D_SceneBegin(topLeft);
        uiDrawFatal(bottom, title, line1, line2);
        C3D_FrameEnd(0);
    }
}

static void movementFromInput(u32 held, int *dx, int *dy)
{
    *dx = 0;
    *dy = 0;

    if (held & KEY_DRIGHT) *dx = 1;
    else if (held & KEY_DLEFT) *dx = -1;
    else if (held & KEY_DUP) *dy = -1;
    else if (held & KEY_DDOWN) *dy = 1;
    else {
        circlePosition circle;
        hidCircleRead(&circle);
        const int dead = 70;

        if (abs(circle.dx) > abs(circle.dy)) {
            if (circle.dx > dead) *dx = 1;
            else if (circle.dx < -dead) *dx = -1;
        } else {
            if (circle.dy > dead) *dy = -1;
            else if (circle.dy < -dead) *dy = 1;
        }
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // The stack issue is fixed, so stereoscopic output can safely be enabled.
    // Audio remains opt-in from Debug so it stays isolated from graphics boot.
    gfxInitDefault();
    gfxSet3D(true);

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return 1;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C3D_Fini();
        gfxExit();
        return 1;
    }
    C2D_Prepare();

    topLeft = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    topRight = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    if (!topLeft || !topRight || !bottom || !uiInit()) {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }

    bootFrame("GPU OK", "NEXT: ROMFS");

    Result romfsResult = romfsInit();
    if (R_FAILED(romfsResult)) {
        fatalLoop("ROMFS ERROR", "COULD NOT MOUNT GAME DATA",
                  "BUILD THE .3DSX WITH ROMFS ENABLED");
        uiExit();
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }

    bootFrame("ROMFS OK", "NEXT: SPRITES");

    if (!assetsInit()) {
        fatalLoop("GRAPHICS ERROR", "COULD NOT LOAD SPRITES.T3X",
                  "CHECK ROMFS:/GFX/SPRITES.T3X");
        romfsExit();
        uiExit();
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }

    bootFrame("SPRITES OK", "NEXT: LEVEL 1");

    gameInit(&game);
    if (!gameLoadLevel(&game, 1)) {
        fatalLoop("LEVEL ERROR", "COULD NOT OPEN LEVEL 1",
                  "CHECK ROMFS:/LEVELS");
        assetsExit();
        romfsExit();
        uiExit();
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }

    rendererResetCamera(&game);
    bootFrame("LEVEL OK", "STARTING GAME - AUDIO SAFE MODE");

    bool paused = false;
    bool resetConfirm = false;
    PausePage pausePage = PAUSE_MENU;
    int pauseSelection = 0;

    // IMPORTANT: do not call ndspInit() during startup. Some emulator/service
    // configurations can block there before the first game frame is presented.
    // Audio can be attempted manually from Debug with Y after the game is alive.

    // Present one complete game frame before entering the regular loop.
    renderFrame(&game, false, false, PAUSE_MENU, 0);

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();

        if (down & KEY_SELECT)
            break;

        if (paused) {
            bool resume = false;

            if (down & KEY_START) {
                resume = true;
            } else if (pausePage == PAUSE_MENU) {
                if (down & KEY_B)
                    resume = true;

                if (down & KEY_DUP)
                    pauseSelection = (pauseSelection + 2) % 3;
                else if (down & KEY_DDOWN)
                    pauseSelection = (pauseSelection + 1) % 3;

                int activated = -1;
                if (down & KEY_A)
                    activated = pauseSelection;

                if (down & KEY_TOUCH) {
                    touchPosition touch;
                    hidTouchRead(&touch);
                    int hit = uiPauseButtonAt(touch.px, touch.py);
                    if (hit >= 0) {
                        pauseSelection = hit;
                        activated = hit;
                    }
                }

                if (activated == 0)
                    resume = true;
                else if (activated == 1)
                    pausePage = PAUSE_DEBUG;
                else if (activated == 2)
                    pausePage = PAUSE_RULES;
            } else {
                if (down & KEY_B)
                    pausePage = PAUSE_MENU;

                if (pausePage == PAUSE_DEBUG) {
                    if (down & KEY_X)
                        audioToggleMusic();
                    if ((down & KEY_Y) && !audioAvailable())
                        audioInit();
                }
            }

            if (resume) {
                paused = false;
                pausePage = PAUSE_MENU;
            }
        } else if (down & KEY_START) {
            paused = true;
            pausePage = PAUSE_MENU;
            pauseSelection = 0;
        } else {
            if (down & KEY_X)
                audioToggleMusic();

            if (game.completed) {
                if (down & KEY_A) {
                    if (gameLoadLevel(&game, 1)) {
                        rendererResetCamera(&game);
                        resetConfirm = false;
                    }
                }
            } else if (resetConfirm) {
                if (down & KEY_A) {
                    if (gameReloadLevel(&game))
                        rendererResetCamera(&game);
                    resetConfirm = false;
                } else if (down & KEY_B) {
                    resetConfirm = false;
                }
            } else {
                if (down & KEY_R) {
                    resetConfirm = true;
                } else {
                    if (down & KEY_L)
                        gameUndo(&game);

                    int dx, dy;
                    movementFromInput(held, &dx, &dy);
                    if ((dx || dy) && gameTryMove(&game, dx, dy))
                        audioPlayMove();
                }
            }

            gameUpdateAnimation(&game);

            if (!resetConfirm) {
                int oldLevel = game.level;
                if (gameCheckWinAndAdvance(&game)) {
                    audioPlayWin();
                    if (game.level != oldLevel)
                        rendererResetCamera(&game);
                }
            }

            rendererUpdateCamera(&game);
        }

        renderFrame(&game, paused, resetConfirm, pausePage, pauseSelection);
    }

    audioShutdown();
    assetsExit();
    romfsExit();
    uiExit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
