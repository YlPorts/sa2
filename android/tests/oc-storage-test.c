#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "game/sa2/oc_characters.h"
static char storagePath[256];
static void corrupt(const unsigned char *record, int count) {
    FILE *file = fopen(storagePath, "wb");
    assert(file != NULL);
    assert(fwrite(record, 1, count, file) == (size_t)count);
    assert(fclose(file) == 0);
}
int main(void) {
    unsigned char record[8];
    struct stat info;
    FILE *file;
    char temporaryDirectory[] = "/tmp/sa2-oc-storage-XXXXXX";
    char missingPath[256];
    assert(NUM_CHARACTERS == 5 && OC_CHARACTER_COUNT == 5 && OC_SELECT_FIRST == 5);
    assert(OcBaseCharacter(OC_JUDE) == CHARACTER_AMY);
    assert(OcBaseCharacter(OC_ELIZABETH) == CHARACTER_SONIC);
    assert(OcBaseCharacter(OC_KIRO) == CHARACTER_SONIC);
    assert(OcBaseCharacter(OC_YULIANA) == CHARACTER_SONIC);
    assert(OcBaseCharacter(OC_KURA) == CHARACTER_SONIC);
    assert(strcmp(OcCharacterName(OC_KURA), "KURA") == 0);
    assert(mkdtemp(temporaryDirectory) != NULL);
    snprintf(storagePath, sizeof(storagePath), "%s/selected.bin", temporaryDirectory);
    OcSelectionSetStoragePath(storagePath);
    assert(gSelectedOc == -1);
    OcSetSelection(2);
    assert(OcSaveSelection());
    assert(stat(storagePath, &info) == 0 && info.st_size == 5 && (info.st_mode & 0777) == 0600);
    file = fopen(storagePath, "rb");
    assert(file != NULL && fread(record, 1, 8, file) == 5);
    fclose(file);
    assert(record[0] == 'O' && record[1] == 'C' && record[2] == 'S' && record[3] == '1' && record[4] == 2);
    OcSetSelection(1);
    OcSelectionSetStoragePath(storagePath);
    assert(gSelectedOc == 2);
    OcSetSelection(-1);
    assert(OcSaveSelection());
    OcSetSelection(3);
    OcSelectionSetStoragePath(storagePath);
    assert(gSelectedOc == -1);
    OcSetSelection(OC_KURA);
    assert(OcSaveSelection());
    OcSetSelection(-1);
    OcSelectionSetStoragePath(storagePath);
    assert(gSelectedOc == OC_KURA);
    corrupt((const unsigned char *)"OCS1\5", 5);
    OcSelectionSetStoragePath(storagePath);
    assert(gSelectedOc == -1);
    corrupt((const unsigned char *)"OCS1\2x", 6);
    OcSelectionSetStoragePath(storagePath);
    assert(gSelectedOc == -1);
    corrupt((const unsigned char *)"OCS0\2", 5);
    OcSelectionSetStoragePath(storagePath);
    assert(gSelectedOc == -1);
    corrupt((const unsigned char *)"OC", 2);
    OcSelectionSetStoragePath(storagePath);
    assert(gSelectedOc == -1);
    snprintf(missingPath, sizeof(missingPath), "%s/missing/selected.bin", temporaryDirectory);
    OcSelectionSetStoragePath(missingPath);
    assert(!OcSaveSelection());
    OcSelectionSetStoragePath(NULL);
    assert(!OcSaveSelection());
    assert(unlink(storagePath) == 0);
    assert(rmdir(temporaryDirectory) == 0);
    puts("OC storage checks passed");
    return 0;
}
