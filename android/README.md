# Android port

This directory builds the portable SDL version as native Android shared libraries.
It does not embed or run a GBA emulator.

## Targets

- `./android/build-apk.sh sa1` -> Sonic Advance 1 APK
- `./android/build-apk.sh sa2` -> Sonic Advance 2 APK

Both APKs contain `arm64-v8a` and `armeabi-v7a` native libraries.

## Requirements

- Android SDK with platform 35
- Android NDK (r27 or newer recommended)
- Java 17
- curl, tar, make, gcc/g++, libpng development headers

Set `ANDROID_SDK_ROOT` and one of `ANDROID_NDK_HOME`, `ANDROID_NDK_ROOT`, or
`ANDROID_NDK_LATEST_HOME`.

The build script pins SDL to 2.30.3 to match the desktop port dependency already
used by this repository. SDL is downloaded into `android/.deps` and is not
committed to the repository.

Outputs are written to `android/out`.

## Current Android layer

- Native SDL2 rendering
- 60 Hz game timing inherited from the portable port
- Multitouch D-pad, A, B, L, R, Start and Select
- Per-app private save file (`sa1.sav` / `sa2.sav`)
- Landscape fullscreen
- Separate Android package IDs for SA1 and SA2

The first milestone is a bootable debug APK. Controller layout, visual polish,
resume/suspend behavior and release signing can then be refined from device logs.
