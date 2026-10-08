#include "render.h"
#include "assets3ds.h"

#include <math.h>

static int cameraX;
static int cameraY;
static bool cameraValid;

/*
 * Tabletop / receding-plane effect.
 *
 * This version deliberately maps the playfield to a TRUE straight-sided
 * trapezoid. The previous renderer curved depth independently by Y, which
 * made the left/right borders bow outward and also caused strange vertical
 * spacing.
 *
 * We instead compute one shared perspective parameter `t` and use it for
 * BOTH vertical position and horizontal convergence. Because X and Y are
 * both linear in the same `t`, every vertical column remains a straight
 * line on-screen.
 *
 * At slider=0: ordinary flat 2D.
 * At slider=1: the top of the screen is pulled down toward the horizon,
 *              narrowed, and pushed behind the screen stereoscopically.
 */
#define PLANE_TOP_SCALE       0.52f
#define PLANE_FAR_DROP_PX    96.0f
#define PLANE_ROW_CURVE       1.12f
#define PLANE_MAX_EYE_SHIFT   10.0f
#define PLANE_CENTER_X      (TOP_SCREEN_W * 0.5f)

static float clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
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

    const int worldW = g->levelWidth * TILE_SIZE;
    const int worldH = g->levelHeight * TILE_SIZE;

    int tx;
    if (worldW <= TOP_SCREEN_W) {
        tx = -(TOP_SCREEN_W - worldW) / 2;
    } else {
        tx = focusX - TOP_SCREEN_W / 2;
        if (tx < 0) tx = 0;
        int maxX = worldW - TOP_SCREEN_W;
        if (tx > maxX) tx = maxX;
    }

    int ty;
    if (worldH <= TOP_SCREEN_H) {
        ty = -(TOP_SCREEN_H - worldH) / 2;
    } else {
        ty = focusY - TOP_SCREEN_H / 2;
        if (ty < 0) ty = 0;
        int maxY = worldH - TOP_SCREEN_H;
        if (ty > maxY) ty = maxY;
    }

    *outX = tx;
    *outY = ty;
}

void rendererResetCamera(const Game *g)
{
    cameraTarget(g, &cameraX, &cameraY);
    cameraValid = true;
}

void rendererUpdateCamera(const Game *g)
{
    int targetX, targetY;
    cameraTarget(g, &targetX, &targetY);

    if (!cameraValid) {
        cameraX = targetX;
        cameraY = targetY;
        cameraValid = true;
        return;
    }

    cameraX += (targetX - cameraX) / 4;
    cameraY += (targetY - cameraY) / 4;
}

void rendererGetCamera(int *x, int *y)
{
    if (x) *x = cameraX;
    if (y) *y = cameraY;
}

/*
 * Convert a normal top-screen point into a straight trapezoid.
 *
 * v is source position from top (0) to bottom (1).
 * A mild power curve makes distant rows pack closer together, like real
 * perspective. Crucially, the SAME t is used for X convergence and Y, so
 * the sides remain perfectly straight instead of becoming curved.
 */
static void projectPoint(float x, float y, float slider,
                         float *outX, float *outY,
                         float *outScale, float *outDepth)
{
    slider = clamp01(slider);

    /* Use the tile anchor itself so grid columns stay geometrically stable. */
    float v = clamp01(y / TOP_SCREEN_H);

    /* No perspective warp at slider zero; increasingly perspective-like rows
       as the physical 3D slider is raised. */
    float perspectiveV = powf(v, PLANE_ROW_CURVE);
    float t = v + (perspectiveV - v) * slider;

    /* Define the target trapezoid. */
    float topY = PLANE_FAR_DROP_PX * slider;
    float topScale = 1.0f - (1.0f - PLANE_TOP_SCALE) * slider;

    /* Interpolate along ONE common parameter. This is what keeps the sides
       straight: x convergence and y position agree about where each row is. */
    float scale = topScale + (1.0f - topScale) * t;
    float px = PLANE_CENTER_X + (x - PLANE_CENTER_X) * scale;
    float py = topY + (TOP_SCREEN_H - topY) * t;

    *outX = px;
    *outY = py;
    *outScale = scale;
    *outDepth = 1.0f - t;
}

static bool visible(C2D_Image img, float x, float y, float scale)
{
    const float w = img.subtex->width * scale;
    const float h = img.subtex->height * scale;
    return x < TOP_SCREEN_W && y < TOP_SCREEN_H && x + w > 0.0f && y + h > 0.0f;
}

static void drawObject(C2D_Image img, int worldX, int worldY,
                       float eyeSign, float slider)
{
    float x = (float)(worldX - cameraX);
    float y = (float)(worldY - cameraY);
    float scale, depth;

    projectPoint(x, y, slider, &x, &y, &scale, &depth);

    /*
     * Positive parallax = behind the screen:
     * left eye shifts left, right eye shifts right.
     * Near rows stay almost at the screen plane; far rows recede.
     */
    x += eyeSign * slider * powf(depth, PLANE_ROW_CURVE) * PLANE_MAX_EYE_SHIFT;

    if (visible(img, x, y, scale))
        C2D_DrawImageAt(img, x, y, 0.0f, NULL, scale, scale);
}

/* Draw from far/top rows toward near/bottom rows so overlaps look natural. */
static void drawPassSortedByY(const Game *g, bool tilesOnly,
                              bool animatedBabaOnly,
                              float eyeSign, float slider)
{
    int order[MAX_OBJECTS];
    int count = 0;

    for (int i = 0; i < g->objectCount; i++) {
        const GameObject *obj = &g->objects[i];

        if (tilesOnly) {
            if (obj->type != OBJ_TILE) continue;
        } else if (animatedBabaOnly) {
            if (obj->type != OBJ_BABA || !gameHasProperty(g, OBJ_BABA, PROP_YOU))
                continue;
        } else {
            if (obj->type == OBJ_TILE) continue;
            if (obj->type == OBJ_BABA && gameHasProperty(g, OBJ_BABA, PROP_YOU))
                continue;
        }

        int pos = count;
        while (pos > 0) {
            const GameObject *prev = &g->objects[order[pos - 1]];
            if (prev->renderY <= obj->renderY)
                break;
            order[pos] = order[pos - 1];
            pos--;
        }
        order[pos] = i;
        count++;
    }

    for (int n = 0; n < count; n++) {
        const GameObject *obj = &g->objects[order[n]];
        int x = (obj->renderX * TILE_SIZE) >> FIX_SHIFT;
        int y = (obj->renderY * TILE_SIZE) >> FIX_SHIFT;

        C2D_Image img;
        if (animatedBabaOnly)
            img = assetsBabaFrame(g->facing, g->animation);
        else if (obj->type == OBJ_TILE)
            img = assetsImage(SPR_TILE);
        else
            img = assetsForObject(obj->type);

        drawObject(img, x, y, eyeSign, slider);
    }
}

void rendererDrawTop(const Game *g, C3D_RenderTarget *target,
                     float eyeSign, float slider)
{
    const u32 clear = C2D_Color32(8, 8, 11, 255);
    C2D_TargetClear(target, clear);
    C2D_SceneBegin(target);

    drawPassSortedByY(g, true,  false, eyeSign, slider); /* floor */
    drawPassSortedByY(g, false, false, eyeSign, slider); /* objects/text */
    drawPassSortedByY(g, false, true,  eyeSign, slider); /* animated Baba */
}
