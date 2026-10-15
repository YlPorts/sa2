#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "gba/types.h"
#include "gba/multiboot.h"
#include "gba/syscall.h"

int main(void)
{
    _Alignas(16) uint8_t source[192], actual[192], expected[192];
    const unsigned counts[] = {0,1,7,8,9,16,24};
    for (unsigned src=0; src<16; src+=4) for (unsigned dst=0; dst<16; dst+=4) {
        for (unsigned c=0; c<sizeof(counts)/sizeof(counts[0]); c++) for (unsigned fill=0; fill<2; fill++) {
            for (unsigned i=0;i<sizeof(source);i++) source[i]=(uint8_t)(i*13+7);
            memset(actual,0xa5,sizeof(actual));memcpy(expected,actual,sizeof(actual));
            unsigned bytes=((counts[c]+7)&~7)*4;
            if(fill) for(unsigned i=0;i<bytes;i+=4)memcpy(expected+dst+i,source+src,4);
            else memcpy(expected+dst,source+src,bytes);
            CpuFastSet(source+src,actual+dst,counts[c] | (fill?CPU_FAST_SET_SRC_FIXED:0));
            assert(!memcmp(actual,expected,sizeof(actual)));
        }
    }
    puts("PASS: CpuFastSet copy/fill at every 4-byte alignment, BIOS block rounding and untouched boundaries");
    return 0;
}
