    	.include "asm/macros/c_decl.inc"
.include "asm/macros/portable.inc"

	mSectionRodata

    .global C_DECL(gMultiboot_087C0258)
C_DECL(gMultiboot_087C0258):
#ifdef __ANDROID__
    .space 0x20A0, 0
#else
    .incbin "baserom_sa1.gba", 0x007C0258, 0x20A0
#endif

    .global C_DECL(gMultiboot_087C22F8)
C_DECL(gMultiboot_087C22F8):
#ifdef __ANDROID__
    .space 0x8000, 0
#else
    .incbin "baserom_sa1.gba", 0x007C22F8, 0x8000
#endif

    .global C_DECL(gUnknown_087CA2F8)
C_DECL(gUnknown_087CA2F8):
#ifdef __ANDROID__
    .space 0x8000, 0
#else
    .incbin "baserom_sa1.gba", 0x007CA2F8, 0x8000
#endif
    
    .global C_DECL(gUnknown_087D22F8)
C_DECL(gUnknown_087D22F8):
#ifdef __ANDROID__
    .space 0x5B88, 0
#else
    .incbin "baserom_sa1.gba", 0x007D22F8, 0x5B88
#endif

    .global C_DECL(gCollectRingsBgStageTileset)
C_DECL(gCollectRingsBgStageTileset):
#ifdef __ANDROID__
    .space 0x8000, 0
#else
    .incbin "baserom_sa1.gba", 0x007D7E80, 0x8000
#endif

    .global C_DECL(gUnknown_087DFE80)
C_DECL(gUnknown_087DFE80):
#ifdef __ANDROID__
    .space 0x3790, 0
#else
    .incbin "baserom_sa1.gba", 0x007DFE80, 0x3790
#endif

    .global C_DECL(gCollectRingsTilemaps)
C_DECL(gCollectRingsTilemaps):
#ifdef __ANDROID__
    .space 0x8000, 0
#else
    .incbin "baserom_sa1.gba", 0x007E3610, 0x8000
#endif

    .global C_DECL(gUnknown_087EB610)
C_DECL(gUnknown_087EB610):
#ifdef __ANDROID__
    .space 0x6B18, 0
#else
    .incbin "baserom_sa1.gba", 0x007EB610, 0x6B18
#endif

    .global C_DECL(gUnknown_087F2128)
C_DECL(gUnknown_087F2128):
#ifdef __ANDROID__
    .space 0x67C8, 0
#else
    .incbin "baserom_sa1.gba", 0x007F2128, 0x67C8
#endif

    .global C_DECL(gUnknown_087F88F0)
C_DECL(gUnknown_087F88F0):
#ifdef __ANDROID__
    .space 0x673C, 0
#else
    .incbin "baserom_sa1.gba", 0x007F88F0, 0x673C
#endif
