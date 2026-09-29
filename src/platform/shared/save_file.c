#if defined(__ANDROID__) || defined(SAVE_FILE_TEST)
#include "platform/shared/save_file.h"
#include <stdio.h>
#include <unistd.h>

int Platform_WriteSaveAtomically(const char *path, const void *data, size_t size)
{
    char temporary[1024];
    FILE *file;
    int ok;
    int pathSize = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (pathSize < 0 || pathSize >= (int)sizeof(temporary))
        return 0;
    file = fopen(temporary, "wb");
    if (file == NULL)
        return 0;
    ok = fwrite(data, 1, size, file) == size && fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file) != 0)
        ok = 0;
    if (ok && rename(temporary, path) == 0)
        return 1;
    remove(temporary);
    return 0;
}
#endif
