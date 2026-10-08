#include <assert.h>
#include <stdio.h>
#include "game.h"

int main(void) {
    Game g;
    gameInit(&g);
    assert(gameLoadLevel(&g, 1));
    assert(gameHasProperty(&g, OBJ_BABA, PROP_YOU));
    assert(gameHasProperty(&g, OBJ_WALL, PROP_STOP));
    assert(gameHasProperty(&g, OBJ_ROCK, PROP_PUSH));
    assert(gameHasProperty(&g, OBJ_FLAG, PROP_WIN));

    int baba = -1;
    for (int i = 0; i < g.objectCount; i++)
        if (g.objects[i].type == OBJ_BABA) { baba = i; break; }
    assert(baba >= 0);
    int oldX = g.objects[baba].gridX;
    int oldY = g.objects[baba].gridY;
    assert(gameTryMove(&g, 1, 0));
    assert(g.objects[baba].gridX == oldX + 1);
    assert(g.objects[baba].gridY == oldY);
    assert(gameUndo(&g));
    assert(g.objects[baba].gridX == oldX);
    assert(g.objects[baba].gridY == oldY);

    // Regression test: the PSP Lua code could double-push during its dry-run.
    Game p = {0};
    p.objectCount = 2;
    p.objects[0] = (GameObject){OBJ_BABA, 0, 0, 0, 0};
    p.objects[1] = (GameObject){OBJ_ROCK, 1, 0, 1 << FIX_SHIFT, 0};
    p.rules[OBJ_BABA] = PROP_YOU;
    p.rules[OBJ_ROCK] = PROP_PUSH;
    p.rulesDirty = false;
    p.facing = FACE_DOWN;
    assert(gameTryMove(&p, 1, 0));
    assert(p.objects[0].gridX == 1);
    assert(p.objects[1].gridX == 2); // Exactly one tile, not two.

    Game stop = {0};
    stop.objectCount = 2;
    stop.objects[0] = (GameObject){OBJ_BABA, 0, 0, 0, 0};
    stop.objects[1] = (GameObject){OBJ_WALL, 1, 0, 1 << FIX_SHIFT, 0};
    stop.rules[OBJ_BABA] = PROP_YOU;
    stop.rules[OBJ_WALL] = PROP_STOP;
    stop.rulesDirty = false;
    assert(!gameTryMove(&stop, 1, 0));
    assert(stop.objects[0].gridX == 0);

    printf("level=%d size=%dx%d objects=%d rules/move/undo/push/stop OK\n",
           g.level, g.levelWidth, g.levelHeight, g.objectCount);
    return 0;
}
