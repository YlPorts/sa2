# Sonic Advance Android ports

Two separate native apps are built from the portable SDL game code. The ROM
importer loads remaining SA1 asset ranges; it does not execute a GBA ROM.

## Build

- `bash android/build-apk.sh sa1` -> Sonic Advance 1, 0.1.0-alpha1
- `bash android/build-apk.sh sa2` -> Sonic Advance 2, 0.1.0-beta4

Default: optimized release, `armeabi-v7a`, non-debuggable, Android 6+ (API 23).
Both builds use the existing 426×240 viewport, fixed 60 Hz game timing,
automatic landscape, cached multitouch controls and app-private saves.
Physical-device performance still needs validation; 60 Hz timing is not an FPS
guarantee. The existing upstream single-pak/chao-garden support is incomplete.

Beta4 requests a fixed landscape orientation in the activity and SDL. The game
image is centered with its aspect ratio preserved even during a transient
portrait surface. Touch controls have larger labels and menu targets; shoulder
buttons sit below the score HUD. Drawing and hit testing share one layout, and
the silhouettes and pressed highlights remain cached textures.

The intermittent first-level closure reported on a Samsung A15 with beta3 has
not been reproduced or confirmed fixed. After an abnormal exit, beta4 can show
Android's retained process exit report on the next launch, including a retained
beta3 report. The user can copy the report to the clipboard or continue without
copying it. Native tombstones on Android 12+ are decoded locally to show the
crashing thread and frame addresses; no report is sent automatically. Unstripped
native build symbols are retained separately to resolve those addresses. Beta3's
background-copy bounds and stage-intro mask fixes remain in place.

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
atomic-save failure behavior, plus native tombstone decoding and malformed input.
GitHub Actions builds both ROM-free APKs and keeps
the separate SA1 native compile/link check. These checks do not certify gameplay
with the original SA1 assets or Samsung A15 frame rates.

`bash android/test-runtime.sh` builds a native SDL test executable with
AddressSanitizer. A test-only linker harness first follows the complete boot and
menu path into Leaf Forest, then starts Acts 1 and 2 with each of the five
characters. Each run executes 1,200 level frames with movement, real tasks,
audio and the 426×240 renderer. Controls and aspect fitting are checked at four
surface sizes. Neither the harness nor its test input is packaged in APKs.
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
