#ifdef SA1_RUNTIME_IMPORT
#include "platform/shared/rom_assets.h"
#include <stdio.h>

extern const Sa1RomAsset __start_sa1_rom_assets[];
extern const Sa1RomAsset __stop_sa1_rom_assets[];

int Sa1_LoadRomAssets(const char *path, char *error, size_t errorSize)
{
    const unsigned long romSize = 8 * 1024 * 1024;
    const Sa1RomAsset *asset;
    FILE *rom = fopen(path, "rb");
    const char *failure = NULL;

    if (rom == NULL) {
        snprintf(error, errorSize, "No se pudieron abrir los datos de Sonic Advance.");
        return 0;
    }
    if (fseek(rom, 0, SEEK_END) != 0 || ftell(rom) != (long)romSize
        || (uintptr_t)__start_sa1_rom_assets == (uintptr_t)__stop_sa1_rom_assets) {
        failure = "Los datos de Sonic Advance no son validos.";
    }
    for (asset = __start_sa1_rom_assets; failure == NULL && asset < __stop_sa1_rom_assets; ++asset) {
        if (asset->destination == NULL || asset->size == 0 || asset->offset > romSize
            || asset->size > romSize - asset->offset) {
            failure = "El port contiene un rango de datos invalido.";
        } else if (fseek(rom, asset->offset, SEEK_SET) != 0
                   || fread(asset->destination, 1, asset->size, rom) != asset->size) {
            failure = "No se pudieron leer todos los datos del juego.";
        }
    }
    fclose(rom);
    if (failure != NULL) {
        snprintf(error, errorSize, "%s", failure);
        return 0;
    }
    return 1;
}
#endif
