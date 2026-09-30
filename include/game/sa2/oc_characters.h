#ifndef GUARD_SA2_OC_CHARACTERS_H
#define GUARD_SA2_OC_CHARACTERS_H

#include "global.h"
#include "constants/sa2/characters.h"

/* These are presentation identities. Original character/save arrays remain five entries. */
#define OC_CHARACTER_COUNT 4
#define OC_SELECT_FIRST NUM_CHARACTERS

enum OcCharacterId {
    OC_ELIZABETH,
    OC_JUDE,
    OC_KIRO,
    OC_YULIANA,
};

extern s8 gSelectedOc; /* -1 means an original character. */

const char *OcCharacterName(u8 oc);
u16 OcCharacterColor(u8 oc);
void OcSetSelection(s8 oc);
bool8 OcIdentityIsActive(void);
void OcBuildIdentityIcon(u8 *tiles, u8 oc);
u8 OcBuildIdentityName(u8 *tiles, u8 oc);
void OcSelectionSetStoragePath(const char *path);
bool8 OcSaveSelection(void);
void CreateOcCharacterSelectionScreen(u8 initialSelection, bool8 allUnlocked);

#endif
