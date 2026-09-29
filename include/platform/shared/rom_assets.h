#ifndef GUARD_PLATFORM_ROM_ASSETS_H
#define GUARD_PLATFORM_ROM_ASSETS_H

#include <stddef.h>
#include <stdint.h>

typedef struct Sa1RomAsset {
    unsigned char *destination;
    uint32_t offset;
    uint32_t size;
} Sa1RomAsset;

/* The Android launcher validates the full ROM SHA-1 before native startup. */
int Sa1_LoadRomAssets(const char *path, char *error, size_t errorSize);

#endif
