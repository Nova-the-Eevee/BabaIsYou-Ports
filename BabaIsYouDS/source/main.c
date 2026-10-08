#include <filesystem.h>
#include <nds.h>
#include <stdint.h>

#include "audio.h"
#include "game.h"
#include "render.h"
#include "ui.h"

typedef enum {
    PAUSE_MENU = 0,
    PAUSE_DEBUG,
    PAUSE_RULES
} PausePage;

static void fatalScreen(const char *title, const char *line1, const char *line2)
{
    uiDrawFatal(title, line1, line2);
    while (1) {
        swiWaitForVBlank();
        uiPresent();
        scanKeys();
        if (keysDown() & KEY_START)
            return;
    }
}

static void drawPausePage(const Game *game, PausePage page, int selection)
{
    if (page == PAUSE_RULES)
        uiDrawRules(game);
    else if (page == PAUSE_DEBUG)
        uiDrawDebug(game, audioMusicEnabled());
    else
        uiDrawPauseMenu(selection);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    lcdMainOnTop();

    // Top LCD: real hardware sprites for the game world.
    videoSetMode(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_SPRITE);
    BG_PALETTE[0] = RGB15(1, 1, 1);

    // Bottom LCD: 8-bit bitmap UI. This lets us draw a full minimap and the
    // touch-friendly pause screens without consuming any top-screen OAM.
    videoSetModeSub(MODE_5_2D);
    vramSetBankC(VRAM_C_SUB_BG);
    if (!uiInit()) {
        while (1) swiWaitForVBlank();
    }

    if (!nitroFSInit(NULL)) {
        fatalScreen("NITROFS ERROR",
                    "COULD NOT OPEN GAME FILES",
                    "TRY NDS-HB-MENU OR DLDI LOADER");
        return 1;
    }

    Game game;
    gameInit(&game);
    if (!gameLoadLevel(&game, 1)) {
        fatalScreen("LEVEL ERROR",
                    "COULD NOT OPEN LEVEL 1",
                    "CHECK NITROFS/LEVELS");
        return 1;
    }

    if (!rendererInit()) {
        fatalScreen("SPRITE ERROR",
                    "SPRITE VRAM ALLOCATION FAILED",
                    NULL);
        return 1;
    }

    rendererResetCamera(&game);
    audioInit();

    bool paused = false;
    bool resetConfirm = false;
    PausePage pausePage = PAUSE_MENU;
    int pauseSelection = 0;

    while (1) {
        swiWaitForVBlank();
        uiPresent();
        scanKeys();
        uint32_t down = keysDown();
        uint32_t held = keysHeld();

        // SELECT is now the always-available quit button; START belongs to the
        // pause menu, which is a much more DS-like use of it.
        if (down & KEY_SELECT)
            break;

        if (paused) {
            bool resume = false;

            if (down & KEY_START) {
                resume = true;
            } else if (pausePage == PAUSE_MENU) {
                if (down & KEY_B) {
                    resume = true;
                }

                if (down & KEY_UP) {
                    pauseSelection = (pauseSelection + 2) % 3;
                } else if (down & KEY_DOWN) {
                    pauseSelection = (pauseSelection + 1) % 3;
                }

                int activated = -1;
                if (down & KEY_A)
                    activated = pauseSelection;

                if (down & KEY_TOUCH) {
                    touchPosition touch;
                    touchRead(&touch);
                    int hit = uiPauseButtonAt(touch.px, touch.py);
                    if (hit >= 0) {
                        pauseSelection = hit;
                        activated = hit;
                    }
                }

                if (activated == 0) {
                    resume = true;
                } else if (activated == 1) {
                    pausePage = PAUSE_DEBUG;
                } else if (activated == 2) {
                    pausePage = PAUSE_RULES;
                }
            } else {
                // Rules/debug sub-pages: B goes back to the three-button menu.
                if (down & KEY_B)
                    pausePage = PAUSE_MENU;

                if (pausePage == PAUSE_DEBUG && (down & KEY_X))
                    audioToggleMusic();
            }

            if (resume) {
                paused = false;
                pausePage = PAUSE_MENU;
                rendererDraw(&game);
                int cameraX, cameraY;
                rendererGetCamera(&cameraX, &cameraY);
                uiDrawMap(&game, cameraX, cameraY);
                continue;
            } else {
                drawPausePage(&game, pausePage, pauseSelection);
                continue;
            }
        }

        if (down & KEY_START) {
            paused = true;
            pausePage = PAUSE_MENU;
            pauseSelection = 0;
            uiDrawPauseMenu(pauseSelection);
            continue;
        }

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

                int dx = 0, dy = 0;
                if (held & KEY_RIGHT) dx = 1;
                else if (held & KEY_LEFT) dx = -1;
                else if (held & KEY_UP) dy = -1;
                else if (held & KEY_DOWN) dy = 1;

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

        rendererDraw(&game);

        if (game.completed) {
            uiDrawComplete();
        } else if (resetConfirm) {
            uiDrawResetConfirm();
        } else {
            int cameraX, cameraY;
            rendererGetCamera(&cameraX, &cameraY);
            uiDrawMap(&game, cameraX, cameraY);
        }
    }

    audioShutdown();
    return 0;
}
