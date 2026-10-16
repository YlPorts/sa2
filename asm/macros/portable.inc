.macro mSectionRodata
#if defined(__ANDROID__)
  // Android shared libraries cannot carry absolute pointer relocations in
  // .rodata. Keep relocation-bearing portable tables in RELRO instead.
  .section .data.rel.ro,"aw",%progbits
#elif (!CPU_ARCH_X86 && defined(__APPLE__)) // On macos, native compilation uses this name for the rodata section
  .section __DATA,__data
#else
  .section .rodata
#endif
.endm

// Used so that we can have cross platform (32bit or 64bit) pointers
.macro mPtr value
#if defined(__aarch64__) || defined(__x86_64__)
    .quad \value
#elif defined(__mips__)
    .4byte \value
#else
    .int \value
#endif
.endm

// Used for cross platform aligning of labels
.macro mAlignWord
// .align uses bytes on x86 ELF but powers of two on ARM/AArch64. Using
// .align 8 here inserted 256-byte gaps into ARM64 C arrays (not 8 bytes),
// leaving the second music-player pointer null at the C array stride.
// .balign has the same byte-based meaning on every supported target.
#if defined(__aarch64__) || defined(__x86_64__)
    .balign 8
#else
    .balign 4
#endif
.endm
