#ifndef GUARD_SA1_UI_OAM_H
#define GUARD_SA1_UI_OAM_H

// SA1's UI routines still assemble three GBA attribute words. In widescreen
// builds, OAM has separate signed positions and different attribute bitfields.
// Convert each completed UI entry once, leaving the local GBA-word templates
// intact for their existing character spacing and tile arithmetic.
static inline void Sa1_ConvertUiOam(OamData *entry)
{
#if EXTENDED_OAM
    const u16 attr0 = entry->all.attr0;
    const u16 attr1 = entry->all.attr1;
    const u16 attr2 = entry->all.attr2;
    OamData native = { 0 };
    const u16 x = attr1 & 0x1ff;
    const u16 y = attr0 & 0xff;
    native.split.x = x >= 256 ? (s16)x - 512 : x;
    native.split.y = y >= 160 ? (s16)y - 256 : y;
    native.split.affineMode = (attr0 >> 8) & 3;
    native.split.objMode = (attr0 >> 10) & 3;
    native.split.mosaic = (attr0 >> 12) & 1;
    native.split.bpp = (attr0 >> 13) & 1;
    native.split.shape = attr0 >> 14;
    native.split.matrixNum = (attr1 >> 9) & 31;
    native.split.size = attr1 >> 14;
    native.split.tileNum = attr2 & 0x3ff;
    native.split.priority = (attr2 >> 10) & 3;
    native.split.paletteNum = attr2 >> 12;
    native.all.affineParam = entry->all.affineParam;
    *entry = native;
#endif
}

#endif
