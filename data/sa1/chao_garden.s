	.include "asm/macros/c_decl.inc"
.include "asm/macros/portable.inc"

    mSectionRodata

    .align 2 , 0
    .global C_DECL(gMultiBootProgram_TinyChaoGarden)
C_DECL(gMultiBootProgram_TinyChaoGarden):
    #ifdef __ANDROID__
    // Tiny Chao Garden transfer payload is not needed by the native Android
    // game. Keep the symbol/size valid without requiring a copyrighted ROM.
    .space 0x1EDD4, 0
#else
    .incbin "baserom_sa1.gba", 0x0009C170, 0x1EDD4
#endif
