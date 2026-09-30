# Sonic Advance Android ports

Two separate native apps are built from the portable SDL game code. The ROM
importer loads remaining SA1 asset ranges; it does not execute a GBA ROM.

## Build

- `bash android/build-apk.sh sa1` -> Sonic Advance 1, 0.1.0-alpha2
- `bash android/build-apk.sh sa2` -> Sonic Advance 2, 0.1.0-beta5

Default: optimized release, `armeabi-v7a`, non-debuggable, Android 6+ (API 23).
Both builds use the existing 426×240 viewport, fixed 60 Hz game timing,
automatic landscape, cached multitouch controls and app-private saves.
Physical-device performance still needs validation; 60 Hz timing is not an FPS
guarantee. The existing upstream single-pak/chao-garden support is incomplete.

SA1 alpha2 restores its background-sprite map pass, which the portable engine
previously skipped. It also clears requested screen maps in virtual VRAM rather
than overwriting the clear-request table, and converts SA1 UI entries from GBA
attribute words to the widened native OAM format. Wide views of small level maps
also guard metatile reads at the map edges. Character-selector backgrounds,
character names, HUD digits and menu labels use their original assets. Android
shows SA1 menus through their original 240x160 viewport and fills the landscape
surface; live stages keep 426x240. These changes are conditional on SA1 and do
not change SA2's drawing or package version.

Beta5 keeps the fixed landscape orientation from beta4 and restores the earlier
full-screen landscape scaling requested by the user, with no side bars added by
the renderer. The internal gameplay viewport remains 426×240; existing 240×160
menu crops also fill the landscape surface. Only a temporary portrait surface
is aspect fitted while Android applies the orientation. Touch controls retain
the larger labels and menu targets, shoulder buttons below the score HUD, shared
draw/hit-test layout and cached textures.

The user's beta4 exit report identifies SIGBUS/BUS_ADRALN at relative PC
0x5f531c, InitWaterPalettes+724, build ID
99f2ba8669575030472d072c38fdace1dc989d5b. Matching symbols resolve it to the
inlined CopyPalette and its bulk sprite-palette copy. The ARM LDM instruction
was reading a two-byte-aligned asset through a u32 pointer. Beta5 uses memcpy
without word-aligned casts for portable palette copies and packed water-color
masking, retaining the original sizes and color arithmetic. The actual engine
routine with halfword-only aligned assets, background palettes and task data
reproduces a sanitizer failure and an optimized ARM32 SIGBUS before the fix;
both pass afterward. Device validation of the new APK is still pending.

After an abnormal exit, the app can show Android's retained process exit report
on the next launch, including reports from earlier versions. The user can copy
it or continue without copying. Native tombstones on Android 12+ are decoded
locally; no report is sent automatically. Unstripped build symbols are retained
separately. Beta3's background-copy bounds and stage-intro mask fixes remain.

Requirements: Java 17, Android SDK platform/build-tools 34, NDK r27 or newer,
Python 3, make, gcc/g++, curl, tar and libpng development headers. Set
`ANDROID_SDK_ROOT` and `ANDROID_NDK_HOME`. SDL 2.30.3 is pinned and downloaded
into ignored `android/.deps`. Outputs are in ignored `android/out`.

## Sonic Advance 1 import

SA1 now builds and packages without a baserom. On first launch select your own
`Sonic Advance (Europe).gba` through Android's document picker. The file must be
8 MiB, with SHA-1 `eb00f101af23d728075ac2117e27ecd8a4b4c3e9`. Truncated,
modified or different-region files are rejected before replacement. A failed
import preserves previously imported data. No storage permission is required.
Subsequent launches validate the private copy and go directly to the game.

At build time, `android/rom-data-asm.py` runs after C preprocessing and replaces
active SA1 baserom incbins with writable reserved ranges plus a native registry.
Native startup fills those exact ranges before `AgbMain`. Portable pointer tables
retain their existing relocations. Disabled GBA multiboot/demo blocks are not
imported. No baserom, generated imported assets or private key is committed.
The linked ELF is checked before packaging: imported ranges must be empty,
writable and outside GNU_RELRO. A read-only asset destination aborts the build.

SA2 starts directly and requires no ROM import.

## Save and resume changes

Both games persist completed game saves immediately. A complete temporary save
is flushed and synced before replacing the previous file by rename. Failed
writes leave the previous complete save intact. Unchanged data is not rewritten
on duplicate background events. Touch and keyboard state and frame backlog are
cleared on background/foreground transitions; cached controls are recreated on
renderer reset.

## Signing

For repeatable updates, set these private build environment variables:

- `SA_SIGNING_KEYSTORE`: absolute path to your existing key
- `SA_SIGNING_STORE_PASSWORD`
- `SA_SIGNING_KEY_ALIAS`
- `SA_SIGNING_KEY_PASSWORD`

Without those variables, Gradle uses its local debug signing key for an optimized,
non-debuggable release APK. A fresh CI runner generates a different signing key;
CI artifacts alone do not guarantee installation over an earlier APK. Signing a
new APK with an unrelated key cannot preserve compatibility with an old one.
Keys must stay outside the public repository and be retained for future builds.

## Validation

`python3 -m unittest discover -s android/tests -v` checks import validation,
stream interruption, preservation of previous data, writable native ranges and
atomic-save failure behavior, native tombstone decoding and malformed input,
and InitWaterPalettes with deliberately halfword-only aligned data under UBSan.
GitHub Actions builds both ROM-free APKs and keeps
the separate SA1 native compile/link check. These checks do not certify gameplay
with the original SA1 assets or Samsung A15 frame rates.

`bash android/test-runtime.sh` builds a native SDL test executable with
AddressSanitizer. A test-only linker harness first follows the complete boot and
menu path into Leaf Forest, then starts Acts 1 and 2 with each of the five
characters. Each run executes 1,200 level frames with movement, real tasks,
audio and the 426×240 renderer. Controls and aspect fitting are checked at four
surface sizes, including the 1536×709 landscape shown in the user's screenshot.
Neither the harness nor its test input is packaged in APKs.
Earlier tests caught the 30-row background overread and the stage-intro write
before the buffer.

`SA_RUNTIME_ANDROID_BACKEND=1 bash android/test-runtime.sh` tests the Android SDL
backend on the host with dummy video/audio and a test-only JNI storage bridge.
Set `SA_TEST_FRAMES=3600` for a longer full-boot run. This exercises the Android
rendering, timing, storage and controls code, but not Android's actual JNI,
graphics drivers or activity lifecycle.
The matching native 32-bit smoke test can be run with `SA_RUNTIME_BITS=32`
(requires multilib and the i386 SDL runtime); add `SA_TEST_RUNNER=qemu-i386`
on hosts without direct i386 execution. QEMU is used only for host tests; the
Android APK remains a native ARM build. Hardware rotation, drivers and device
frame rates require a physical-device test.

`bash android/test-sa1-graphics.sh` runs SA1's four selectors, the first two
stage graphics and The Moon through the actual engine and software renderer under
AddressSanitizer. Set `SA_RUNTIME_BITS=32` for the native 32-bit host build, or
`SA_RUNTIME_ANDROID_BACKEND=1` to exercise Android's SDL code with dummy drivers
and assert the menu crop and full stage viewport. Set `SA_TEST_LEVELS` and
`SA_TEST_FRAMES` to select additional stages and longer runs. Without
`SA_TEST_ROM`, remaining ROM-only tables are empty and the harness deliberately
limits these checks to graphics and memory safety. With `SA_TEST_ROM` set to
the user's European ROM, it imports the real tables before the same checks.
The test harness and its optional ROM input are never packaged in APKs. The
focused unit suite also checks text/affine map clears, tile flips, palette banks,
VRAM bounds and UI OAM on the host and optimized ARM32 when its test tools exist.
