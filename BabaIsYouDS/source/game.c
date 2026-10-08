#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ObjectType charToType(char c)
{
    switch (c) {
        case 'W': return OBJ_WALL;
        case 'R': return OBJ_ROCK;
        case 'F': return OBJ_FLAG;
        case 'B': return OBJ_BABA;
        case 'E': return OBJ_TILE;
        case 'T': return OBJ_TEXT_WALL;
        case 'G': return OBJ_TEXT_ROCK;
        case 'Q': return OBJ_TEXT_FLAG;
        case 'A': return OBJ_TEXT_BABA;
        case 'I': return OBJ_TEXT_IS;
        case 'S': return OBJ_TEXT_STOP;
        case 'P': return OBJ_TEXT_PUSH;
        case 'V': return OBJ_TEXT_WIN;
        case 'Y': return OBJ_TEXT_YOU;
        default: return OBJ_TYPE_COUNT;
    }
}

bool gameIsText(ObjectType type)
{
    return type >= OBJ_TEXT_WALL && type < OBJ_TYPE_COUNT;
}

const char *gameObjectName(ObjectType type)
{
    switch (type) {
        case OBJ_WALL: return "WALL";
        case OBJ_ROCK: return "ROCK";
        case OBJ_FLAG: return "FLAG";
        case OBJ_BABA: return "BABA";
        case OBJ_TILE: return "TILE";
        default: return "?";
    }
}

static int subjectFromText(ObjectType type)
{
    switch (type) {
        case OBJ_TEXT_WALL: return OBJ_WALL;
        case OBJ_TEXT_ROCK: return OBJ_ROCK;
        case OBJ_TEXT_FLAG: return OBJ_FLAG;
        case OBJ_TEXT_BABA: return OBJ_BABA;
        default: return -1;
    }
}

static PropertyBits propertyFromText(ObjectType type)
{
    switch (type) {
        case OBJ_TEXT_STOP: return PROP_STOP;
        case OBJ_TEXT_PUSH: return PROP_PUSH;
        case OBJ_TEXT_WIN: return PROP_WIN;
        case OBJ_TEXT_YOU: return PROP_YOU;
        default: return PROP_NONE;
    }
}

void gameInit(Game *g)
{
    memset(g, 0, sizeof(*g));
    g->facing = FACE_DOWN;
    g->rulesDirty = true;
}

static bool spawnObject(Game *g, int x, int y, ObjectType type)
{
    if (g->objectCount >= MAX_OBJECTS)
        return false;

    GameObject *obj = &g->objects[g->objectCount++];
    obj->type = type;
    obj->gridX = (int16_t)x;
    obj->gridY = (int16_t)y;
    obj->renderX = x << FIX_SHIFT;
    obj->renderY = y << FIX_SHIFT;
    return true;
}

bool gameLoadLevel(Game *g, int level)
{
    char path[64];
    snprintf(path, sizeof(path), "nitro:/levels/level%d.txt", level);
    FILE *f = fopen(path, "r");

#ifdef HOST_TEST
    if (!f) {
        snprintf(path, sizeof(path), "nitrofs/levels/level%d.txt", level);
        f = fopen(path, "r");
    }
#endif

    if (!f)
        return false;

    g->objectCount = 0;
    g->historyCount = 0;
    g->levelWidth = 0;
    g->levelHeight = 0;
    g->level = level;
    g->completed = false;
    g->facing = FACE_DOWN;
    g->animation = 0;

    char line[256];
    int y = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strcspn(line, "\r\n");
        line[len] = '\0';
        if ((int)len > g->levelWidth)
            g->levelWidth = (int)len;

        for (int x = 0; x < (int)len; x++) {
            if (line[x] == '.')
                continue;
            ObjectType type = charToType(line[x]);
            if (type != OBJ_TYPE_COUNT)
                spawnObject(g, x, y, type);
        }
        y++;
    }
    fclose(f);

    g->levelHeight = y;
    g->rulesDirty = true;
    gameUpdateRules(g);
    return true;
}

bool gameReloadLevel(Game *g)
{
    return gameLoadLevel(g, g->level);
}

static int getObjectsAt(const Game *g, int x, int y, int *out, int maxOut)
{
    int count = 0;
    for (int i = 0; i < g->objectCount; i++) {
        if (g->objects[i].gridX == x && g->objects[i].gridY == y) {
            if (out && count < maxOut)
                out[count] = i;
            count++;
        }
    }
    return count;
}

bool gameHasProperty(const Game *g, ObjectType type, PropertyBits property)
{
    if (type < OBJ_WALL || type > OBJ_TILE)
        return false;
    return (g->rules[type] & property) != 0;
}

void gameUpdateRules(Game *g)
{
    memset(g->rules, 0, sizeof(g->rules));

    int mid[16], rhs[16];
    for (int i = 0; i < g->objectCount; i++) {
        const GameObject *subjectObj = &g->objects[i];
        int subject = subjectFromText(subjectObj->type);
        if (subject < 0)
            continue;

        const int dirs[2][2] = {{1, 0}, {0, 1}};
        for (int d = 0; d < 2; d++) {
            int dx = dirs[d][0], dy = dirs[d][1];
            int nmid = getObjectsAt(g, subjectObj->gridX + dx,
                                    subjectObj->gridY + dy, mid, 16);
            int nrhs = getObjectsAt(g, subjectObj->gridX + 2 * dx,
                                    subjectObj->gridY + 2 * dy, rhs, 16);
            if (nmid > 16) nmid = 16;
            if (nrhs > 16) nrhs = 16;

            bool hasIs = false;
            for (int m = 0; m < nmid; m++) {
                if (g->objects[mid[m]].type == OBJ_TEXT_IS) {
                    hasIs = true;
                    break;
                }
            }
            if (!hasIs)
                continue;

            for (int r = 0; r < nrhs; r++) {
                PropertyBits p = propertyFromText(g->objects[rhs[r]].type);
                if (p != PROP_NONE)
                    g->rules[subject] |= p;
            }
        }
    }

    g->rulesDirty = false;
}

static void saveState(Game *g)
{
    if (g->historyCount == MAX_HISTORY) {
        memmove(&g->history[0], &g->history[1],
                sizeof(g->history[0]) * (MAX_HISTORY - 1));
        g->historyCount--;
    }

    HistoryState *s = &g->history[g->historyCount++];
    s->objectCount = g->objectCount;
    s->facing = g->facing;
    s->animation = g->animation;
    for (int i = 0; i < g->objectCount; i++) {
        s->objects[i].gridX = g->objects[i].gridX;
        s->objects[i].gridY = g->objects[i].gridY;
        s->objects[i].renderX = g->objects[i].renderX;
        s->objects[i].renderY = g->objects[i].renderY;
    }
}

bool gameUndo(Game *g)
{
    if (g->historyCount <= 0)
        return false;

    HistoryState *s = &g->history[--g->historyCount];
    if (s->objectCount != g->objectCount)
        return false;

    g->facing = s->facing;
    g->animation = s->animation;
    for (int i = 0; i < g->objectCount; i++) {
        g->objects[i].gridX = s->objects[i].gridX;
        g->objects[i].gridY = s->objects[i].gridY;
        g->objects[i].renderX = s->objects[i].renderX;
        g->objects[i].renderY = s->objects[i].renderY;
    }
    g->rulesDirty = true;
    gameUpdateRules(g);
    return true;
}

static bool listContains(const int *list, int count, int value)
{
    for (int i = 0; i < count; i++)
        if (list[i] == value)
            return true;
    return false;
}

// Checks a push chain. When commit is true, pushable objects are moved.
static bool canMoveTo(Game *g, int x, int y, int dx, int dy, bool commit)
{
    int chain[MAX_OBJECTS];
    int chainCount = 0;
    int at[32];
    int cx = x, cy = y;

    for (int safety = 0; safety < MAX_OBJECTS + 4; safety++) {
        int count = getObjectsAt(g, cx, cy, at, 32);
        if (count == 0)
            break;
        if (count > 32)
            count = 32;

        bool hasPushable = false;
        for (int i = 0; i < count; i++) {
            GameObject *obj = &g->objects[at[i]];

            if (!gameIsText(obj->type) && gameHasProperty(g, obj->type, PROP_STOP))
                return false;

            if (gameIsText(obj->type) || gameHasProperty(g, obj->type, PROP_PUSH)) {
                if (!listContains(chain, chainCount, at[i]))
                    chain[chainCount++] = at[i];
                hasPushable = true;
            }
        }

        if (!hasPushable)
            break;

        cx += dx;
        cy += dy;
    }

    if (commit) {
        for (int i = 0; i < chainCount; i++) {
            GameObject *obj = &g->objects[chain[i]];
            obj->gridX += dx;
            obj->gridY += dy;
            if (gameIsText(obj->type))
                g->rulesDirty = true;
        }
    }

    return true;
}

bool gameAllYouIdle(const Game *g)
{
    for (int i = 0; i < g->objectCount; i++) {
        const GameObject *obj = &g->objects[i];
        if (!gameHasProperty(g, obj->type, PROP_YOU))
            continue;
        int32_t tx = obj->gridX << FIX_SHIFT;
        int32_t ty = obj->gridY << FIX_SHIFT;
        if (abs((int)(obj->renderX - tx)) > 2 || abs((int)(obj->renderY - ty)) > 2)
            return false;
    }
    return true;
}

bool gameTryMove(Game *g, int dx, int dy)
{
    if (g->completed || (dx == 0 && dy == 0))
        return false;
    if (g->rulesDirty)
        gameUpdateRules(g);
    if (!gameAllYouIdle(g))
        return false;

    int you[MAX_OBJECTS];
    int youCount = 0;
    bool canMoveAny = false;

    for (int i = 0; i < g->objectCount; i++) {
        if (gameHasProperty(g, g->objects[i].type, PROP_YOU)) {
            you[youCount++] = i;
            int tx = g->objects[i].gridX + dx;
            int ty = g->objects[i].gridY + dy;
            if (canMoveTo(g, tx, ty, dx, dy, false))
                canMoveAny = true;
        }
    }

    if (!canMoveAny)
        return false;

    saveState(g);
    bool moved = false;
    for (int i = 0; i < youCount; i++) {
        GameObject *obj = &g->objects[you[i]];
        int tx = obj->gridX + dx;
        int ty = obj->gridY + dy;
        if (canMoveTo(g, tx, ty, dx, dy, true)) {
            obj->gridX += dx;
            obj->gridY += dy;
            moved = true;
        }
    }

    if (!moved) {
        g->historyCount--;
        return false;
    }

    if (dx > 0) g->facing = FACE_RIGHT;
    else if (dx < 0) g->facing = FACE_LEFT;
    else if (dy < 0) g->facing = FACE_UP;
    else g->facing = FACE_DOWN;

    g->animation = (g->animation + 1) % 3;
    if (g->rulesDirty)
        gameUpdateRules(g);
    return true;
}

void gameUpdateAnimation(Game *g)
{
    for (int i = 0; i < g->objectCount; i++) {
        GameObject *obj = &g->objects[i];
        int32_t tx = obj->gridX << FIX_SHIFT;
        int32_t ty = obj->gridY << FIX_SHIFT;
        int32_t dx = tx - obj->renderX;
        int32_t dy = ty - obj->renderY;

        if (abs((int)dx) <= 2) obj->renderX = tx;
        else obj->renderX += dx / 3;

        if (abs((int)dy) <= 2) obj->renderY = ty;
        else obj->renderY += dy / 3;
    }
}

bool gameCheckWinAndAdvance(Game *g)
{
    if (g->completed)
        return false;
    if (g->rulesDirty)
        gameUpdateRules(g);

    for (int i = 0; i < g->objectCount; i++) {
        GameObject *you = &g->objects[i];
        if (!gameHasProperty(g, you->type, PROP_YOU))
            continue;

        int at[32];
        int count = getObjectsAt(g, you->gridX, you->gridY, at, 32);
        if (count > 32) count = 32;
        for (int n = 0; n < count; n++) {
            if (at[n] == i)
                continue;
            GameObject *target = &g->objects[at[n]];
            if (gameHasProperty(g, target->type, PROP_WIN)) {
                int next = g->level + 1;
                if (!gameLoadLevel(g, next))
                    g->completed = true;
                return true;
            }
        }
    }
    return false;
}
