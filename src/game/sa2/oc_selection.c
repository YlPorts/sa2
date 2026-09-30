#include "global.h"
#include "core.h"
#include "sprite.h"
#include "background.h"
#include "task.h"
#include "malloc_vram.h"
#include "flags.h"
#include "trig.h"
#include "lib/m4a/m4a.h"
#include "game/sa2/oc_characters.h"
#include "data/sa2/oc_sprite_data.h"
#include "game/sa2/save.h"
#include "game/sa2/title_screen.h"
#include "game/sa2/options_screen.h"
#include "game/sa2/ui/course_select.h"
#include "game/sa2/ui/time_attack_lobby.h"
#include "game/sa2/ui/time_attack_mode_select.h"
#include "game/sa2/stage/screen_fade.h"
#include "game/shared/stage/stage.h"
#include "constants/sa2/animations.h"
#include "constants/sa2/songs.h"
#include "constants/sa2/text.h"
#include "constants/sa2/tilemaps.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __ANDROID__
extern void Platform_SetNativeUiCrop(bool8 enabled);
#endif

#define OC_MENU_ENTRIES (NUM_CHARACTERS + OC_CHARACTER_COUNT)
#define OC_MENU_SCROLL_FRAMES 12
#define OC_MENU_UI_PALETTE 14
#define OC_MENU_PREVIEW_PALETTE 15
/* The original selector's heap starts at OBJ+0x3A00. Its single-player cross-box
 * is unused, leaving this area for four circles and a small pixel font. */
#define OC_MENU_CIRCLE_TILES (OBJ_VRAM0 + 0x2800)
#define OC_MENU_FONT_TILES (OBJ_VRAM0 + 0x3000)

enum OcMenuPhase { OC_MENU_IN, OC_MENU_READY, OC_MENU_CONFIRM, OC_MENU_BACK };

typedef struct {
    ScreenFade fade;
    Background backgrounds[3];
    Sprite blobs[NUM_CHARACTERS];
    Sprite activeBlob;
    SpriteTransform activeTransform;
    Sprite character;
    Sprite cheese;
    Sprite title;
    Sprite name;
    Sprite nameLeft;
    Sprite nameRight;
    Sprite upArrow;
    Sprite downArrow;
    s32 wheelPosition;
    s32 scrollStart;
    s32 scrollDelta;
    u16 frame;
    u8 phase;
    u8 selection;
    u8 scrollFrame;
    u8 upFrames;
    u8 downFrames;
    u8 pulse;
    bool8 queuedConfirm;
    s8 previousOc;
    u8 available;
    u8 activeCircleTiles[2048] ALIGNED(4);
} OcMenu;

s8 gSelectedOc = -1;
static char sOcStoragePath[2048];

static const char *const sOcNames[OC_CHARACTER_COUNT] = { "ELIZABETH", "JUDE", "KIRO", "YULIANA" };
static const u16 sOcColors[OC_CHARACTER_COUNT] = {
    RGB16(24, 25, 27), RGB16(29, 23, 5), RGB16(15, 17, 7), RGB16(5, 11, 8),
};
static const u8 sSilhouettes[NUM_CHARACTERS] = { 8, 4, 7, 6, 5 };
static const u8 sCharacterVariants[NUM_CHARACTERS] = { 0, 2, 4, 6, 8 };
static const u8 sTitleVariants[NUM_CHARACTERS] = { 0, 8, 2, 4, 6 };
static const u16 sTitleAnimations[] = { 740, 743, 744, 748, 745, 746 };
static const u16 sCharacterVoices[NUM_CHARACTERS] = {
    VOICE__ANNOUNCER__SONIC, VOICE__ANNOUNCER__CREAM, VOICE__ANNOUNCER__TAILS,
    VOICE__ANNOUNCER__KNUCKLES, VOICE__ANNOUNCER__AMY,
};

/* Seven rows of a five-pixel alphabet. Native 8x8 OBJ tiles retain hard pixel
 * edges and the selector's shadowed text treatment at every display scale. */
static const u8 sFont[26][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,25,21,19,19,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
};

static void Task_OcCharacterSelect(void);
static void DestroyOcMenu(Task *task);
static void RenderOcMenu(OcMenu *menu);
static void RefreshSelection(OcMenu *menu, bool8 chosen);

const char *OcCharacterName(u8 oc) { return oc < OC_CHARACTER_COUNT ? sOcNames[oc] : ""; }
u16 OcCharacterColor(u8 oc) { return oc < OC_CHARACTER_COUNT ? sOcColors[oc] : 0; }
void OcSetSelection(s8 oc) { gSelectedOc = (oc >= 0 && oc < OC_CHARACTER_COUNT) ? oc : -1; }

bool8 OcIdentityIsActive(void)
{
    return gSelectedOc >= 0 && gSelectedOc < OC_CHARACTER_COUNT && IS_SINGLE_PLAYER
        && gSelectedCharacter == CHARACTER_SONIC && !(gStageFlags & STAGE_FLAG__DEMO_RUNNING);
}

void OcSelectionSetStoragePath(const char *path)
{
    FILE *file;
    u8 record[5];
    sOcStoragePath[0] = 0;
    OcSetSelection(-1);
    if (path == NULL || strlen(path) >= sizeof(sOcStoragePath))
        return;
    memcpy(sOcStoragePath, path, strlen(path) + 1);
    file = fopen(sOcStoragePath, "rb");
    if (file == NULL)
        return;
    if (fread(record, 1, sizeof(record), file) == sizeof(record) && fgetc(file) == EOF
        && memcmp(record, "OCS1", 4) == 0 && (record[4] == 255 || record[4] < OC_CHARACTER_COUNT))
        OcSetSelection((s8)record[4]);
    fclose(file);
}

bool8 OcSaveSelection(void)
{
    char temporary[sizeof(sOcStoragePath) + 16];
    char directory[sizeof(sOcStoragePath)];
    u8 record[5] = { 'O', 'C', 'S', '1', (u8)gSelectedOc };
    size_t written = 0;
    int fd, directoryFd;
    char *lastSlash;
    if (!sOcStoragePath[0])
        return FALSE;
    snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", sOcStoragePath);
    fd = mkstemp(temporary);
    if (fd < 0)
        return FALSE;
    while (written < sizeof(record)) {
        ssize_t result = write(fd, record + written, sizeof(record) - written);
        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0) {
            close(fd);
            unlink(temporary);
            return FALSE;
        }
        written += result;
    }
    if (fsync(fd) != 0) {
        close(fd);
        unlink(temporary);
        return FALSE;
    }
    if (close(fd) != 0 || rename(temporary, sOcStoragePath) != 0) {
        unlink(temporary);
        return FALSE;
    }
    memcpy(directory, sOcStoragePath, strlen(sOcStoragePath) + 1);
    lastSlash = strrchr(directory, '/');
    if (lastSlash != NULL) {
        if (lastSlash == directory)
            lastSlash[1] = 0;
        else
            *lastSlash = 0;
    } else {
        strcpy(directory, ".");
    }
    directoryFd = open(directory, O_RDONLY | O_DIRECTORY);
    if (directoryFd >= 0) {
        fsync(directoryFd);
        close(directoryFd);
    }
    return TRUE;
}

static bool8 EntryAvailable(const OcMenu *menu)
{
    return menu->selection >= OC_SELECT_FIRST || (menu->available & CHARACTER_BIT(menu->selection));
}

static void InitSprite(Sprite *sprite, void *tiles, u16 animation, u8 variant, u8 order)
{
    memset(sprite, 0, sizeof(*sprite));
    sprite->graphics.dest = tiles;
    sprite->graphics.anim = animation;
    sprite->variant = variant;
    sprite->oamFlags = SPRITE_OAM_ORDER(order);
    sprite->prevVariant = -1;
    sprite->animSpeed = SPRITE_ANIM_SPEED(1);
    sprite->hitboxes[0].index = -1;
    UpdateSpriteAnimation(sprite);
}

static void SetTilePixel(u8 *tiles, unsigned width, unsigned x, unsigned y, u8 index)
{
    unsigned offset = ((y / 8) * (width / 8) + x / 8) * 32 + (y & 7) * 4 + (x & 7) / 2;
    if (x & 1)
        tiles[offset] = (tiles[offset] & 15) | (index << 4);
    else
        tiles[offset] = (tiles[offset] & 240) | index;
}

void OcBuildIdentityIcon(u8 *tiles, u8 oc)
{
    unsigned x, y;
    memset(tiles, 0, 128);
    if (oc >= OC_CHARACTER_COUNT)
        return;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            unsigned sourceX = 22 + x * 24 / 16;
            unsigned sourceY = 12 + y * 20 / 16;
            unsigned offset = ((sourceY / 8) * 8 + sourceX / 8) * 32 + (sourceY & 7) * 4 + (sourceX & 7) / 2;
            u8 packed = gOcFrameTiles[oc][0][offset];
            u8 index = (sourceX & 1) ? packed >> 4 : packed & 15;
            SetTilePixel(tiles, 16, x, y, index);
        }
    }
}

u8 OcBuildIdentityName(u8 *tiles, u8 oc)
{
    unsigned c, x, y;
    u8 length;
    if (oc >= OC_CHARACTER_COUNT)
        return 0;
    length = strlen(sOcNames[oc]);
    memset(tiles, 0, length * 32);
    for (c = 0; c < length; c++) {
        unsigned glyph = sOcNames[oc][c] - 'A';
        for (y = 0; y < 7; y++)
            for (x = 0; x < 5; x++)
                if (sFont[glyph][y] & (1 << (4 - x)))
                    SetTilePixel(tiles + c * 32, 8, x + 2, y + 1, 1);
        for (y = 0; y < 7; y++)
            for (x = 0; x < 5; x++)
                if (sFont[glyph][y] & (1 << (4 - x)))
                    SetTilePixel(tiles + c * 32, 8, x + 1, y, 2);
    }
    return length;
}

static void GenerateCircle(u8 *tiles, u8 oc, unsigned size, bool8 active)
{
    unsigned x, y;
    s32 center = (s32)size - 1;
    s32 radius = (s32)size - 3;
    memset(tiles, 0, size * size / 2);
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            s32 dx = (s32)x * 2 - center, dy = (s32)y * 2 - center;
            s32 distance = dx * dx + dy * dy;
            u8 pixel;
            if (distance > radius * radius)
                continue;
            if (distance > (radius - 2) * (radius - 2))
                pixel = 1;
            else if (active && distance > (radius - 8) * (radius - 8))
                pixel = 3;
            else if (distance > (radius - (active ? 10 : 4)) * (radius - (active ? 10 : 4)))
                pixel = 1;
            else
                pixel = 4 + oc * 3 + ((x + y < size * 3 / 4) ? 2 : ((x + y > size * 5 / 4) ? 0 : 1));
            SetTilePixel(tiles, size, x, y, pixel);
        }
    }
}

static void GenerateMenuTiles(void)
{
    unsigned c, x, y;
    u8 *font = OC_MENU_FONT_TILES;
    for (c = 0; c < OC_CHARACTER_COUNT; c++)
        GenerateCircle(OC_MENU_CIRCLE_TILES + c * 512, c, 32, FALSE);
    memset(font, 0, 26 * 32);
    for (c = 0; c < 26; c++) {
        for (y = 0; y < 7; y++) {
            for (x = 0; x < 5; x++) {
                if (sFont[c][y] & (1 << (4 - x)))
                    SetTilePixel(font + c * 32, 8, x + 2, y + 1, 1);
            }
        }
        for (y = 0; y < 7; y++) {
            for (x = 0; x < 5; x++) {
                if (sFont[c][y] & (1 << (4 - x)))
                    SetTilePixel(font + c * 32, 8, x + 1, y, 2);
            }
        }
    }
}

static void RenderNativeTile(const void *tiles, s16 x, s16 y, u8 size, u8 palette, u8 order)
{
    OamData *oam;
    s16 width = 8 << size;
    if (x <= -width || x >= 240 || y <= -width || y >= 160)
        return;
    oam = OamMalloc(order);
    if (oam == (OamData *)iwram_end)
        return;
#if !EXTENDED_OAM
    oam->all.attr0 = y & 255;
    oam->all.attr1 = (x & 511) | (size << 14);
    oam->all.attr2 = GET_TILE_NUM_COMMON(tiles, TILE_SIZE_4BPP) | (palette << 12);
#else
    oam->split.x = x;
    oam->split.y = y;
    oam->split.affineMode = 0;
    oam->split.objMode = 0;
    oam->split.mosaic = 0;
    oam->split.bpp = 0;
    oam->split.shape = 0;
    oam->split.matrixNum = 0;
    oam->split.size = size;
    oam->split.tileNum = GET_TILE_NUM_COMMON(tiles, TILE_SIZE_4BPP);
    oam->split.priority = 0;
    oam->split.paletteNum = palette;
#endif
}

static void RenderName(const char *text, s16 x, s16 y, u8 palette)
{
    while (*text) {
        u8 c = *text++;
        if (c >= 'A' && c <= 'Z')
            RenderNativeTile(OC_MENU_FONT_TILES + (c - 'A') * 32, x, y, 0, palette, 2);
        x += 7;
    }
}

static void RenderOcPreview(OcMenu *menu, s16 slide)
{
    /* Nearest-neighbour 2x in the selector matches the original portrait height.
     * Source anchor (32,48) stays at (173,130); gameplay keeps native pixel size. */
    OamData *oam = OamMalloc(4);
    u8 matrix;
    if (oam == (OamData *)iwram_end)
        return;
    matrix = gOamMatrixIndex++ & 31;
#if !EXTENDED_OAM
    oam->all.attr0 = 34 | (ST_OAM_AFFINE_DOUBLE << 8);
    oam->all.attr1 = ((109 + slide) & 511) | (matrix << 9) | (3 << 14);
    oam->all.attr2 = GET_TILE_NUM_COMMON(menu->character.graphics.dest, TILE_SIZE_4BPP) | (OC_MENU_PREVIEW_PALETTE << 12);
#else
    oam->split.x = 109 + slide;
    oam->split.y = 34;
    oam->split.affineMode = ST_OAM_AFFINE_DOUBLE;
    oam->split.objMode = 0;
    oam->split.mosaic = 0;
    oam->split.bpp = 0;
    oam->split.shape = 0;
    oam->split.matrixNum = matrix;
    oam->split.size = 3;
    oam->split.tileNum = GET_TILE_NUM_COMMON(menu->character.graphics.dest, TILE_SIZE_4BPP);
    oam->split.priority = 0;
    oam->split.paletteNum = OC_MENU_PREVIEW_PALETTE;
#endif
    gOamBuffer[matrix * 4 + 0].all.affineParam = 128;
    gOamBuffer[matrix * 4 + 1].all.affineParam = 0;
    gOamBuffer[matrix * 4 + 2].all.affineParam = 0;
    gOamBuffer[matrix * 4 + 3].all.affineParam = 128;
}

static void RefreshSelection(OcMenu *menu, bool8 chosen)
{
    u8 entry = menu->selection;
    Sprite *s = &menu->character;
    if (entry >= OC_SELECT_FIRST) {
        u8 oc = entry - OC_SELECT_FIRST;
        s->graphics.src = gOcFrameTiles[oc][chosen ? 28 : 0];
        s->graphics.size = 2048;
        ADD_TO_GRAPHICS_QUEUE(&s->graphics);
        GenerateCircle(menu->activeCircleTiles, oc, 64, TRUE);
        menu->activeBlob.graphics.src = menu->activeCircleTiles;
        menu->activeBlob.graphics.size = sizeof(menu->activeCircleTiles);
        ADD_TO_GRAPHICS_QUEUE(&menu->activeBlob.graphics);
        return;
    }
    s->graphics.anim = SA2_ANIM_CHAR_SELECT_CHARACTER;
    s->variant = sCharacterVariants[entry] + chosen;
    s->prevVariant = -1;
    s->frameFlags = 0;
    s->palId = 0;
    UpdateSpriteAnimation(s);
    if (!EntryAvailable(menu)) {
        s->frameFlags |= 0x40000;
        s->palId = sSilhouettes[entry];
    }
    menu->name.graphics.anim = LOADED_SAVE->language == LANG_JAPANESE ? 741 : 747;
    menu->name.variant = entry;
    menu->name.prevVariant = -1;
    menu->nameLeft.variant = sTitleVariants[entry];
    menu->nameLeft.prevVariant = -1;
    menu->nameRight.variant = sTitleVariants[entry] + 1;
    menu->nameRight.prevVariant = -1;
    if (!EntryAvailable(menu)) {
        menu->name.graphics.anim = 741;
        menu->name.variant = 5;
        menu->nameLeft.variant = 10;
        menu->nameRight.variant = 11;
    }
    UpdateSpriteAnimation(&menu->name);
    UpdateSpriteAnimation(&menu->nameLeft);
    UpdateSpriteAnimation(&menu->nameRight);
    menu->activeBlob.variant = entry + SA2_ANIM_VARIANT_CHAR_SELECT_CIRCLE_ACTIVE;
    menu->activeBlob.prevVariant = -1;
    UpdateSpriteAnimation(&menu->activeBlob);
    menu->cheese.variant = SA2_ANIM_VARIANT_SELECT_CHARACTER(SA2_ANIM_CHAR_ID_CHEESE, chosen);
    menu->cheese.prevVariant = -1;
    UpdateSpriteAnimation(&menu->cheese);
}

static void InitMenuBackground(Background *bg, u16 tilemap, u8 id, u8 graphicsBlock, u8 layoutBlock, u8 width, u8 height)
{
    memset(bg, 0, sizeof(*bg));
    bg->graphics.dest = (void *)BG_SCREEN_ADDR(graphicsBlock);
    bg->layoutVram = (void *)BG_SCREEN_ADDR(layoutBlock);
    bg->tilemapId = tilemap;
    bg->targetTilesX = width;
    bg->targetTilesY = height;
    bg->flags = BACKGROUND_FLAGS_BG_ID(id);
    DrawBackground(bg);
}

void CreateOcCharacterSelectionScreen(u8 initialSelection, bool8 allUnlocked)
{
    Task *task;
    OcMenu *menu;
    u8 i, lang = LOADED_SAVE->language;
    (void)allUnlocked;
#ifdef __ANDROID__
    Platform_SetNativeUiCrop(TRUE);
#endif
    task = TaskCreate(Task_OcCharacterSelect, sizeof(OcMenu), 0x4100, 0, DestroyOcMenu);
    menu = TASK_DATA(task);
    memset(menu, 0, sizeof(*menu));
    OcSetSelection(gSelectedOc);
    menu->previousOc = gSelectedOc;
    menu->selection = gSelectedOc >= 0 ? OC_SELECT_FIRST + gSelectedOc
                                      : (initialSelection < NUM_CHARACTERS ? initialSelection : CHARACTER_SONIC);
    menu->available = LOADED_SAVE->unlockedCharacters;
    menu->wheelPosition = -(s32)menu->selection * (1024 * 256) / OC_MENU_ENTRIES;
    menu->scrollFrame = OC_MENU_SCROLL_FRAMES;
    gDispCnt = 0x1740;
    gBgCntRegs[0] = 0x1403;
    gBgCntRegs[1] = 0x160E;
    gBgCntRegs[2] = 0x1507;
    for (i = 0; i < 3; i++)
        gBgScrollRegs[i][0] = gBgScrollRegs[i][1] = 0;
    InitMenuBackground(&menu->backgrounds[0], TM_CHARACTER_SELECT_BACKGROUND_0, 0, 0, 20, 32, 32);
    InitMenuBackground(&menu->backgrounds[1], TM_CHARACTER_SELECT_WHEEL, 1, 24, 22, 30, 20);
    InitMenuBackground(&menu->backgrounds[2], TM_CHARACTER_SELECT_BACKGROUND_1, 2, 8, 21, 32, 32);
    for (i = 0; i < NUM_CHARACTERS; i++)
        InitSprite(&menu->blobs[i], VramMalloc(16), SA2_ANIM_CHAR_SELECT_CIRCLE, i, 4);
    InitSprite(&menu->activeBlob, VramMalloc(64), SA2_ANIM_CHAR_SELECT_CIRCLE, 5, 3);
    InitSprite(&menu->name, VramMalloc(36), 741, 0, 4);
    InitSprite(&menu->nameLeft, VramMalloc(64), 739, 0, 4);
    InitSprite(&menu->nameRight, VramMalloc(64), 739, 1, 4);
    if (lang < LANG_JAPANESE || lang > LANG_ITALIAN)
        lang = LANG_ENGLISH;
    InitSprite(&menu->title, VramMalloc(54), sTitleAnimations[lang - 1], 0, 4);
    InitSprite(&menu->upArrow, VramMalloc(24), SA2_ANIM_CHAR_SELECT_ARROW, 0, 4);
    InitSprite(&menu->downArrow, VramMalloc(24), SA2_ANIM_CHAR_SELECT_ARROW, 0, 4);
    menu->upArrow.x = menu->downArrow.x = 17;
    menu->upArrow.y = 18;
    menu->downArrow.y = 142;
    menu->downArrow.frameFlags = SPRITE_FLAG_MASK_Y_FLIP;
    InitSprite(&menu->cheese, OBJ_VRAM0 + 0x400, SA2_ANIM_CHAR_SELECT_CHARACTER,
               SA2_ANIM_VARIANT_SELECT_CHARACTER(SA2_ANIM_CHAR_ID_CHEESE, 0), 4);
    InitSprite(&menu->character, OBJ_VRAM0 + 0x1000, SA2_ANIM_CHAR_SELECT_CHARACTER, 0, 4);
    GenerateMenuTiles();
    RefreshSelection(menu, FALSE);
    menu->fade.window = SCREEN_FADE_USE_WINDOW_0;
    menu->fade.flags = SCREEN_FADE_FLAG_DARKEN | SCREEN_FADE_FLAG_2;
    menu->fade.speed = 0x180;
    menu->fade.bldCnt = BLDCNT_EFFECT_DARKEN | BLDCNT_TGT1_ALL;
    UpdateScreenFade(&menu->fade);
    m4aSongNumStart(MUS_CHARACTER_SELECTION);
}

static void BeginExitFade(OcMenu *menu)
{
    menu->frame = 0;
    menu->fade.brightness = 0;
    menu->fade.flags = SCREEN_FADE_FLAG_LIGHTEN;
    menu->fade.speed = 0x180;
    menu->fade.bldCnt = BLDCNT_EFFECT_DARKEN | BLDCNT_TGT1_ALL;
}

static void StartSelectedGame(void)
{
#ifdef __ANDROID__
    Platform_SetNativeUiCrop(FALSE);
#endif
    TaskDestroy(gCurTask);
    if (gGameMode != GAME_MODE_SINGLE_PLAYER) {
        CreateTimeAttackLevelSelectScreen((gGameMode & GAME_MODE_BOSS_TIME_ATTACK) ? 1 : 0, gSelectedCharacter, gCurrentLevel);
    } else if (LOADED_SAVE->unlockedLevels[gSelectedCharacter] <= LEVEL_INDEX(ZONE_1, ACT_BOSS)) {
        gCurrentLevel = LEVEL_INDEX(ZONE_1, ACT_1);
        GameStageStart();
    } else if (LOADED_SAVE->extraZoneStatus == 1 && gSelectedCharacter == CHARACTER_SONIC) {
        CreateCourseSelectionScreen(LEVEL_INDEX(ZONE_1, ACT_1), LOADED_SAVE->unlockedLevels[gSelectedCharacter],
                                    CUT_SCENE_UNLOCK_TRUE_AREA_53);
    } else {
        CreateCourseSelectionScreen(LEVEL_INDEX(ZONE_1, ACT_1), LOADED_SAVE->unlockedLevels[gSelectedCharacter],
                                    COURSE_SELECT_CUT_SCENE_NONE);
    }
}

static void Task_OcCharacterSelect(void)
{
    OcMenu *menu = TASK_DATA(gCurTask);
    menu->pulse++;
    menu->frame++;
    if (menu->phase == OC_MENU_IN) {
        if (UpdateScreenFade(&menu->fade) == SCREEN_FADE_COMPLETE) {
            menu->phase = OC_MENU_READY;
            menu->frame = 0;
        }
    } else if (menu->phase == OC_MENU_READY) {
        if (gPressedKeys & B_BUTTON) {
            OcSetSelection(menu->previousOc);
            menu->phase = OC_MENU_BACK;
            BeginExitFade(menu);
            m4aSongNumStart(SE_RETURN);
        } else {
            if (menu->scrollFrame < OC_MENU_SCROLL_FRAMES) {
                s32 u, smooth;
                menu->scrollFrame++;
                u = (s32)menu->scrollFrame * 256 / OC_MENU_SCROLL_FRAMES;
                smooth = (3 * u * u - 2 * u * u * u / 256) / 256;
                menu->wheelPosition = menu->scrollStart + menu->scrollDelta * smooth / 256;
                if (menu->scrollFrame == OC_MENU_SCROLL_FRAMES)
                    menu->wheelPosition = -(s32)menu->selection * (1024 * 256) / OC_MENU_ENTRIES;
            } else if (gInput & (DPAD_UP | DPAD_LEFT | DPAD_DOWN | DPAD_RIGHT)) {
                bool8 down = (gInput & (DPAD_DOWN | DPAD_RIGHT)) != 0;
                menu->selection = (menu->selection + (down ? 1 : OC_MENU_ENTRIES - 1)) % OC_MENU_ENTRIES;
                menu->scrollStart = menu->wheelPosition;
                menu->scrollDelta = (down ? -1 : 1) * (1024 * 256 / OC_MENU_ENTRIES);
                menu->scrollFrame = 0;
                if (down)
                    menu->downFrames = OC_MENU_SCROLL_FRAMES;
                else
                    menu->upFrames = OC_MENU_SCROLL_FRAMES;
                RefreshSelection(menu, FALSE);
                m4aSongNumStart(SE_SHIFT);
            }
            if ((gPressedKeys & A_BUTTON) && EntryAvailable(menu))
                menu->queuedConfirm = TRUE;
            if (menu->queuedConfirm && EntryAvailable(menu) && menu->scrollFrame == OC_MENU_SCROLL_FRAMES) {
                if (menu->selection >= OC_SELECT_FIRST) {
                    OcSetSelection(menu->selection - OC_SELECT_FIRST);
                    gSelectedCharacter = CHARACTER_SONIC;
                    m4aSongNumStart(SE_SELECT);
                } else {
                    OcSetSelection(-1);
                    gSelectedCharacter = menu->selection;
                    m4aSongNumStart(sCharacterVoices[menu->selection]);
                }
                menu->phase = OC_MENU_CONFIRM;
                OcSaveSelection();
                BeginExitFade(menu);
                RefreshSelection(menu, TRUE);
            }
        }
    } else if (menu->phase == OC_MENU_CONFIRM) {
        if (menu->frame >= 30 && UpdateScreenFade(&menu->fade) == SCREEN_FADE_COMPLETE) {
            StartSelectedGame();
            return;
        }
    } else if (UpdateScreenFade(&menu->fade) == SCREEN_FADE_COMPLETE) {
#ifdef __ANDROID__
        Platform_SetNativeUiCrop(FALSE);
#endif
        TasksDestroyAll();
        PAUSE_BACKGROUNDS_QUEUE();
        gBgSpritesCount = 0;
        PAUSE_GRAPHICS_QUEUE();
        if (gGameMode != GAME_MODE_SINGLE_PLAYER)
            CreateTimeAttackModeSelectionScreen();
        else
            CreateTitleScreenAtSinglePlayerMenu();
        return;
    }
    RenderOcMenu(menu);
    gBgScrollRegs[0][1] = (gBgScrollRegs[0][1] - 1) & 255;
    gBgScrollRegs[2][0] = (gBgScrollRegs[2][0] - 1) & 255;
    gBgScrollRegs[2][1] = (gBgScrollRegs[2][1] + 1) & 255;
}

static void RenderOcMenu(OcMenu *menu)
{
    unsigned i;
    s16 slide = menu->scrollFrame < OC_MENU_SCROLL_FRAMES ? (OC_MENU_SCROLL_FRAMES - menu->scrollFrame) * 8 : 0;
    Sprite *s;
    for (i = 0; i < OC_MENU_ENTRIES; i++) {
        s32 angle = ((menu->wheelPosition + (s32)i * (1024 * 256) / OC_MENU_ENTRIES) >> 8) & 1023;
        s16 x = Q_2_14_TO_INT(COS(angle) * 92) + 10;
        s16 y = Q_2_14_TO_INT(SIN(angle) * 92) + 80;
        if (i == menu->selection && menu->scrollFrame == OC_MENU_SCROLL_FRAMES)
            continue;
        if (i < NUM_CHARACTERS) {
            menu->blobs[i].x = x;
            menu->blobs[i].y = y;
            DisplaySprite(&menu->blobs[i]);
        } else {
            RenderNativeTile(OC_MENU_CIRCLE_TILES + (i - OC_SELECT_FIRST) * 512, x - 16, y - 16, 2, OC_MENU_UI_PALETTE, 4);
        }
    }
    if (menu->scrollFrame == OC_MENU_SCROLL_FRAMES) {
        if (menu->selection >= OC_SELECT_FIRST) {
            RenderNativeTile(menu->activeBlob.graphics.dest, 69, 47, 3, OC_MENU_UI_PALETTE, 3);
        } else {
            s = &menu->activeBlob;
            s->x = 101;
            s->y = 79;
            menu->activeTransform.rotation = 0;
            menu->activeTransform.qScaleX = menu->activeTransform.qScaleY
                = (gSineTable[((menu->pulse & 63) * 16 + 256) & 1023] >> 8) + 192;
            menu->activeTransform.x = s->x;
            menu->activeTransform.y = s->y;
            s->frameFlags = gOamMatrixIndex++ | 0x60;
            TransformSprite(s, &menu->activeTransform);
            DisplaySprite(s);
        }
    }
    if (menu->selection >= OC_SELECT_FIRST) {
        u8 oc = menu->selection - OC_SELECT_FIRST;
        s16 nameWidth = strlen(sOcNames[oc]) * 7;
        RenderOcPreview(menu, slide);
        RenderName(sOcNames[oc], 32, 75, 13);
        RenderName(sOcNames[oc], 173 - nameWidth / 2 + slide, 143, OC_MENU_UI_PALETTE);
        for (i = 0; i < 16; i++)
            SET_PALETTE_COLOR_OBJ(OC_MENU_PREVIEW_PALETTE, i, gOcPalettes[oc][i]);
    } else {
        s = &menu->character;
        s->x = 166 + slide;
        s->y = 130;
        if (menu->phase == OC_MENU_CONFIRM)
            UpdateSpriteAnimation(s);
        DisplaySprite(s);
        if (menu->selection == CHARACTER_CREAM) {
            menu->cheese.x = 166 + slide;
            menu->cheese.y = 130;
            if (menu->phase == OC_MENU_CONFIRM)
                UpdateSpriteAnimation(&menu->cheese);
            DisplaySprite(&menu->cheese);
        }
        menu->name.x = 166 + slide;
        menu->name.y = 144;
        DisplaySprite(&menu->name);
        menu->nameLeft.x = menu->nameRight.x = 40;
        menu->nameLeft.y = menu->nameRight.y = 79;
        DisplaySprite(&menu->nameLeft);
        DisplaySprite(&menu->nameRight);
    }
    menu->title.x = 240;
    menu->title.y = 16;
    DisplaySprite(&menu->title);
    menu->upArrow.variant = menu->upFrames ? 1 : 0;
    menu->downArrow.variant = menu->downFrames ? 1 : 0;
    if (menu->upFrames)
        menu->upFrames--;
    if (menu->downFrames)
        menu->downFrames--;
    UpdateSpriteAnimation(&menu->upArrow);
    UpdateSpriteAnimation(&menu->downArrow);
    DisplaySprite(&menu->upArrow);
    DisplaySprite(&menu->downArrow);
    SET_PALETTE_COLOR_OBJ(OC_MENU_UI_PALETTE, 0, 0);
    SET_PALETTE_COLOR_OBJ(OC_MENU_UI_PALETTE, 1, RGB16(3, 4, 7));
    SET_PALETTE_COLOR_OBJ(OC_MENU_UI_PALETTE, 2, RGB_WHITE);
    SET_PALETTE_COLOR_OBJ(OC_MENU_UI_PALETTE, 3, RGB16(31, 26 + ((menu->pulse >> 3) & 3), 6));
    SET_PALETTE_COLOR_OBJ(13, 0, 0);
    SET_PALETTE_COLOR_OBJ(13, 1, RGB_WHITE);
    SET_PALETTE_COLOR_OBJ(13, 2, RGB16(3, 4, 7));
    for (i = 0; i < OC_CHARACTER_COUNT; i++) {
        u16 color = sOcColors[i];
        u8 r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
        SET_PALETTE_COLOR_OBJ(OC_MENU_UI_PALETTE, 4 + i * 3, RGB16(r * 3 / 4, g * 3 / 4, b * 3 / 4));
        SET_PALETTE_COLOR_OBJ(OC_MENU_UI_PALETTE, 5 + i * 3, color);
        SET_PALETTE_COLOR_OBJ(OC_MENU_UI_PALETTE, 6 + i * 3, RGB16(r + (31 - r) / 3, g + (31 - g) / 3, b + (31 - b) / 3));
    }
    gFlags |= FLAGS_UPDATE_SPRITE_PALETTES;
}

static void DestroyOcMenu(Task *task)
{
    OcMenu *menu = TASK_DATA(task);
    unsigned i;
    for (i = 0; i < NUM_CHARACTERS; i++)
        VramFree(menu->blobs[i].graphics.dest);
    VramFree(menu->activeBlob.graphics.dest);
    VramFree(menu->name.graphics.dest);
    VramFree(menu->nameLeft.graphics.dest);
    VramFree(menu->nameRight.graphics.dest);
    VramFree(menu->title.graphics.dest);
    VramFree(menu->upArrow.graphics.dest);
    VramFree(menu->downArrow.graphics.dest);
}
