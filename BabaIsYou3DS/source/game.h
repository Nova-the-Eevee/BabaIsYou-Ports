#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MAX_OBJECTS 256
#define MAX_HISTORY 64
#define TILE_SIZE 24
#define FIX_SHIFT 8
#define FIX_ONE (1 << FIX_SHIFT)

typedef enum {
    OBJ_WALL = 0,
    OBJ_ROCK,
    OBJ_FLAG,
    OBJ_BABA,
    OBJ_TILE,

    OBJ_TEXT_WALL,
    OBJ_TEXT_ROCK,
    OBJ_TEXT_FLAG,
    OBJ_TEXT_BABA,
    OBJ_TEXT_IS,
    OBJ_TEXT_STOP,
    OBJ_TEXT_PUSH,
    OBJ_TEXT_WIN,
    OBJ_TEXT_YOU,

    OBJ_TYPE_COUNT
} ObjectType;

typedef enum {
    PROP_NONE = 0,
    PROP_STOP = 1 << 0,
    PROP_PUSH = 1 << 1,
    PROP_WIN  = 1 << 2,
    PROP_YOU  = 1 << 3
} PropertyBits;

typedef enum {
    FACE_RIGHT = 0,
    FACE_UP = 1,
    FACE_LEFT = 2,
    FACE_DOWN = 3
} Facing;

typedef struct {
    ObjectType type;
    int16_t gridX;
    int16_t gridY;
    int32_t renderX; // 8.8 fixed point, in tiles
    int32_t renderY;
} GameObject;

typedef struct {
    int objectCount;
    Facing facing;
    int animation;
    struct {
        int16_t gridX, gridY;
        int32_t renderX, renderY;
    } objects[MAX_OBJECTS];
} HistoryState;

typedef struct {
    GameObject objects[MAX_OBJECTS];
    int objectCount;

    uint8_t rules[5]; // Rules for WALL, ROCK, FLAG, BABA, TILE
    bool rulesDirty;

    HistoryState history[MAX_HISTORY];
    int historyCount;

    int level;
    int levelWidth;
    int levelHeight;
    bool completed;

    Facing facing;
    int animation;
} Game;

void gameInit(Game *g);
bool gameLoadLevel(Game *g, int level);
bool gameReloadLevel(Game *g);
void gameUpdateRules(Game *g);
void gameUpdateAnimation(Game *g);
bool gameTryMove(Game *g, int dx, int dy);
bool gameUndo(Game *g);
bool gameCheckWinAndAdvance(Game *g);
bool gameHasProperty(const Game *g, ObjectType type, PropertyBits property);
bool gameIsText(ObjectType type);
bool gameAllYouIdle(const Game *g);
const char *gameObjectName(ObjectType type);
