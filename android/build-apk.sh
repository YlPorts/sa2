#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANDROID_DIR="$ROOT/android"
GAME="${1:-sa2}"
SA1_ROM_ARG="${2:-}"
SDL_VERSION="2.30.3"
ANDROID_API="${ANDROID_API:-23}"
read -r -a ABIS <<< "${ANDROID_ABIS:-armeabi-v7a}"
BUILD_TYPE="${BUILD_TYPE:-release}"
SA1_RUNTIME_IMPORT="${SA1_RUNTIME_IMPORT:-1}"

case "$GAME" in
    sa1)
        APP_NAME="Sonic Advance"
        APK_NAME="SonicAdvance1-android-release.apk"
        ;;
    sa2)
        APP_NAME="Sonic Advance 2"
        APK_NAME="SonicAdvance2-android-release.apk"
        ;;
    *)
        echo "Usage: $0 [sa1|sa2]" >&2
        exit 2
        ;;
esac

TEMP_SA1_BASEROM=0
cleanup() {
    if [[ "$TEMP_SA1_BASEROM" == "1" ]]; then
        rm -f "$ROOT/baserom_sa1.gba"
    fi
}
trap cleanup EXIT

if [[ "$GAME" == "sa1" ]]; then
    EXPECTED_SA1_SHA1="eb00f101af23d728075ac2117e27ecd8a4b4c3e9"

    if [[ "${SA1_COMPILE_CHECK:-0}" == "1" ]]; then
        SA1_RUNTIME_IMPORT=1
        COMPILE_CHECK_ONLY=1
    fi
    if [[ "$SA1_RUNTIME_IMPORT" == "1" ]]; then
        echo "[android] SA1 runtime import: no baserom needed to build"
    else
        if [[ ! -f "$ROOT/baserom_sa1.gba" ]]; then
            if [[ -z "$SA1_ROM_ARG" || ! -f "$SA1_ROM_ARG" ]]; then
                echo "SA1 still depends on data extracted from the original European ROM." >&2
                echo "Usage: $0 sa1 /path/to/SonicAdvance-Europe.gba" >&2
                exit 1
            fi

            cp "$SA1_ROM_ARG" "$ROOT/baserom_sa1.gba"
            TEMP_SA1_BASEROM=1
        fi

        SA1_SHA1="$(sha1sum "$ROOT/baserom_sa1.gba" | awk '{print $1}')"
        if [[ "$SA1_SHA1" != "$EXPECTED_SA1_SHA1" ]]; then
            echo "Wrong Sonic Advance 1 ROM revision." >&2
            echo "Expected SHA-1: $EXPECTED_SA1_SHA1" >&2
            echo "Actual SHA-1:   $SA1_SHA1" >&2
            exit 1
        fi
    fi
fi

NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-${ANDROID_NDK_LATEST_HOME:-}}}"
if [[ -z "$NDK_HOME" || ! -x "$NDK_HOME/ndk-build" ]]; then
    echo "Android NDK not found. Set ANDROID_NDK_HOME, ANDROID_NDK_ROOT, or ANDROID_NDK_LATEST_HOME." >&2
    exit 1
fi

if [[ -z "${ANDROID_SDK_ROOT:-}" ]]; then
    echo "ANDROID_SDK_ROOT must point to the Android SDK." >&2
    exit 1
fi

HOST_TOOLCHAIN="$(find "$NDK_HOME/toolchains/llvm/prebuilt" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
if [[ -z "$HOST_TOOLCHAIN" ]]; then
    echo "Could not locate the NDK LLVM prebuilt toolchain." >&2
    exit 1
fi
TOOLBIN="$HOST_TOOLCHAIN/bin"
LLVM_AR="$TOOLBIN/llvm-ar"

JOBS="${JOBS:-}"
if [[ -z "$JOBS" ]]; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
fi

DEPS_DIR="$ANDROID_DIR/.deps"
WORK_DIR="$ANDROID_DIR/.work/$GAME"
OUT_DIR="$ANDROID_DIR/out"
SDL_ARCHIVE="$DEPS_DIR/SDL2-$SDL_VERSION.tar.gz"
SDL_SRC="$DEPS_DIR/SDL-release-$SDL_VERSION"
SDL_LIBS="$WORK_DIR/sdl-libs"
PROJECT_DIR="$WORK_DIR/project"

mkdir -p "$DEPS_DIR" "$OUT_DIR"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

if [[ ! -d "$SDL_SRC" ]]; then
    echo "[android] Downloading SDL $SDL_VERSION"
    if [[ ! -f "$SDL_ARCHIVE" ]]; then
        curl -L --fail --retry 3 \
            "https://github.com/libsdl-org/SDL/archive/refs/tags/release-$SDL_VERSION.tar.gz" \
            -o "$SDL_ARCHIVE"
    fi
    tar -xzf "$SDL_ARCHIVE" -C "$DEPS_DIR"
fi

# NDK 29 marks ALooper_pollAll as unavailable. SDL 2.30.3 only uses it as a
# zero-timeout sensor poll here, so pollOnce is the compatible replacement.
SDL_SENSOR_ANDROID="$SDL_SRC/src/sensor/android/SDL_androidsensor.c"
if grep -q "ALooper_pollAll" "$SDL_SENSOR_ANDROID"; then
    sed -i 's/ALooper_pollAll/ALooper_pollOnce/g' "$SDL_SENSOR_ANDROID"
fi

echo "[android] Building host preprocessing tools"
make -C "$ROOT" -j"$JOBS" tools

compiler_for_abi() {
    case "$1" in
        arm64-v8a)
            echo "$TOOLBIN/aarch64-linux-android${ANDROID_API}-clang"
            ;;
        armeabi-v7a)
            echo "$TOOLBIN/armv7a-linux-androideabi${ANDROID_API}-clang"
            ;;
        *)
            return 1
            ;;
    esac
}

cxx_for_abi() {
    case "$1" in
        arm64-v8a)
            echo "$TOOLBIN/aarch64-linux-android${ANDROID_API}-clang++"
            ;;
        armeabi-v7a)
            echo "$TOOLBIN/armv7a-linux-androideabi${ANDROID_API}-clang++"
            ;;
        *)
            return 1
            ;;
    esac
}

for ABI in "${ABIS[@]}"; do
    echo "[android] Building SDL2 for $ABI"
    "$NDK_HOME/ndk-build" \
        NDK_PROJECT_PATH=null \
        APP_BUILD_SCRIPT="$SDL_SRC/Android.mk" \
        APP_PLATFORM="android-$ANDROID_API" \
        APP_ABI="$ABI" \
        NDK_OUT="$WORK_DIR/sdl-obj/$ABI" \
        NDK_LIBS_OUT="$SDL_LIBS"

    CC="$(compiler_for_abi "$ABI")"
    CXX="$(cxx_for_abi "$ABI")"

    if [[ ! -x "$CC" || ! -x "$CXX" ]]; then
        echo "Missing NDK compiler for $ABI" >&2
        exit 1
    fi

    echo "[android] Building $GAME native game library for $ABI"
    rm -rf "$ROOT/build/android/$ABI/$GAME" "$ROOT/libagbsyscall/build/android/$ABI"

    make -C "$ROOT" -j"$JOBS" \
        PLATFORM=android \
        GAME_NAME="$GAME" \
        SA1_RUNTIME_IMPORT="$SA1_RUNTIME_IMPORT" \
        CPU_ARCH=arm \
        ANDROID_ABI="$ABI" \
        ANDROID_API="$ANDROID_API" \
        SDL_ANDROID_ROOT="$SDL_SRC" \
        SDL_ANDROID_LIB="$SDL_LIBS/$ABI" \
        CC1="$CC" \
        CXX="$CXX" \
        AS="$CC -c -x assembler" \
        AR="$LLVM_AR"

    GAME_LIB="$ROOT/build/android/$ABI/$GAME/libmain.so"
    SDL_LIB="$SDL_LIBS/$ABI/libSDL2.so"

    if [[ ! -f "$GAME_LIB" || ! -f "$SDL_LIB" ]]; then
        echo "Native build did not produce the expected libraries for $ABI." >&2
        exit 1
    fi

    # __sF is the pre-API-23 stdio backing symbol. Modern Android no longer
    # exports it, so allowing it into libmain.so causes an immediate dlopen
    # failure before SDL can create a window.
    if "$TOOLBIN/llvm-readelf" -Ws "$GAME_LIB" | grep -qE ' UND .*__sF(@|$)'; then
        echo "Fatal: $ABI libmain.so still depends on removed Android symbol __sF." >&2
        exit 1
    fi

    if [[ "$GAME" == "sa1" && "$SA1_RUNTIME_IMPORT" == "1" ]]; then
        python3 "$ANDROID_DIR/verify-native.py" "$GAME_LIB"
    fi

    if [[ "$BUILD_TYPE" == "release" ]]; then
        "$TOOLBIN/llvm-strip" --strip-unneeded "$GAME_LIB"
        "$TOOLBIN/llvm-strip" --strip-unneeded "$SDL_LIB"
    fi
done

if [[ "${COMPILE_CHECK_ONLY:-0}" == "1" ]]; then
    echo "[android] Native compile/link check completed successfully; no APK will be packaged."
    exit 0
fi

echo "[android] Preparing Gradle package"
cp -R "$SDL_SRC/android-project" "$PROJECT_DIR"
cp "$ANDROID_DIR/template/app/build.gradle" "$PROJECT_DIR/app/build.gradle"
cp "$ANDROID_DIR/template/app/src/main/AndroidManifest.xml" "$PROJECT_DIR/app/src/main/AndroidManifest.xml"
if [[ -d "$ANDROID_DIR/template/app/src/main/java" ]]; then
    mkdir -p "$PROJECT_DIR/app/src/main/java"
    cp -R "$ANDROID_DIR/template/app/src/main/java/." "$PROJECT_DIR/app/src/main/java/"
fi
mkdir -p "$PROJECT_DIR/app/src/main/res/values"
cat > "$PROJECT_DIR/app/src/main/res/values/strings.xml" <<EOF
<resources>
    <string name="app_name">$APP_NAME</string>
</resources>
EOF

rm -rf "$PROJECT_DIR/app/src/main/jniLibs"
for ABI in "${ABIS[@]}"; do
    mkdir -p "$PROJECT_DIR/app/src/main/jniLibs/$ABI"
    cp "$SDL_LIBS/$ABI/libSDL2.so" "$PROJECT_DIR/app/src/main/jniLibs/$ABI/"
    cp "$ROOT/build/android/$ABI/$GAME/libmain.so" "$PROJECT_DIR/app/src/main/jniLibs/$ABI/"
done

cat > "$PROJECT_DIR/local.properties" <<EOF
sdk.dir=$ANDROID_SDK_ROOT
EOF

echo "[android] Packaging $APP_NAME ($BUILD_TYPE)"
case "$BUILD_TYPE" in
    release)
        GRADLE_TASK="assembleRelease"
        APK="$PROJECT_DIR/app/build/outputs/apk/release/app-release.apk"
        if [[ "${SA_UNSIGNED_RELEASE:-0}" == "1" ]]; then
            APK="$PROJECT_DIR/app/build/outputs/apk/release/app-release-unsigned.apk"
            APK_NAME="${APK_NAME%.apk}-unsigned.apk"
        fi
        ;;
    debug)
        GRADLE_TASK="assembleDebug"
        APK="$PROJECT_DIR/app/build/outputs/apk/debug/app-debug.apk"
        ;;
    *)
        echo "BUILD_TYPE must be release or debug." >&2
        exit 2
        ;;
esac

(
    cd "$PROJECT_DIR"
    chmod +x ./gradlew
    ./gradlew --no-daemon "$GRADLE_TASK" -PSA_GAME="$GAME"
)

if [[ ! -f "$APK" ]]; then
    echo "Gradle completed without producing $APK" >&2
    exit 1
fi

cp "$APK" "$OUT_DIR/$APK_NAME"
echo "[android] APK: $OUT_DIR/$APK_NAME"
