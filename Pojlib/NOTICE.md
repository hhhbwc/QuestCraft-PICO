QuestCraft-PICO — third-party components (Phase 0, 26.3 SDL port)
=================================================================

Amethyst-Android (LGPL-3.0)
  AngelAuraMC/Amethyst-Android, branch v3_openjdk, commit 330c6eae.
  - src/main/jni/native_hooks/{native_hooks.h,sdl_hook.c,dlopen_hook.c,
    exit_hook.c,chmod_hook.c} — ported and trimmed to Phase 0 scope
    (no SDL input mapping, no SDL2, no constructor preload, exit-code
    capture not ported); the JNI entry is renamed to Pojlib's package.
  - src/main/java/org/libsdl/app/*.java — SDL 3.4.12-era Android glue with
    Amethyst's launcher-integration patches (SDLActivity.onCreate throws,
    externalInitialize/getSDLSurface, static getNativeSurface, IME helpers).
    Only import lines were adapted to Pojlib's packages; see HANDOFF §4.
  - assets/components/lwjgl3/3.4.3/lwjgl-sdl-overlay-3.4.3.jar contains
    org.lwjgl.sdl.SDLInit/SDLMouse compiled from Amethyst's patched copies of
    the LWJGL 3.4.3 generated sources, plus a new minimal
    org.lwjgl.glfw.CallbackBridge (JRE side).
  - assets/components/lwjgl3/3.4.3/natives/arm64-v8a/liblwjgl.so,
    liblwjgl_opengl.so, liblwjgl_stb.so, liblwjgl_vma.so, libfreetype.so,
    libshaderc.so — LWJGL 3.4.1 Bionic natives built and re-distributed by
    Amethyst (LWJGL BSD-3 natives, LGPL-3.0 wrapper build).
  - src/main/jniLibs/arm64-v8a/libopenal.so is NOT used; the per-instance
    libopenal.so comes from Amethyst's openal-soft AAR (see below).

SDL3 (zlib)
  libsdl-org SDL 3.4.14 official android AAR.
  - src/main/jniLibs/arm64-v8a/libSDL3.so — extracted from
    SDL3-3.4.14.aar (prefab/modules/SDL3-shared/libs/android.arm64-v8a) and
    llvm-stripped.
  - src/main/jni/include/SDL3/ — headers from the same AAR
    (prefab/modules/SDL3-Headers).

LWJGL 3.4.3 (BSD-3)
  Official org.lwjgl:lwjgl*:3.4.3 jars in
  assets/components/lwjgl3/3.4.3/ (core, opengl, openal, stb, freetype,
  shaderc, spvc, vma, sdl). The overlay jar modifies SDLInit/SDLMouse only.

OpenAL Soft (LGPL-2.1)
  assets/components/lwjgl3/3.4.3/natives/arm64-v8a/libopenal.so — extracted
  from Amethyst's openal-soft release AAR (amethyst-openal-soft-release.aar).

SPIRV-Cross (Apache-2.0 / Khronos)
  assets/components/lwjgl3/3.4.3/natives/arm64-v8a/libspirv-cross-c-shared.so
  — extracted from Amethyst's spirv-cross natives AAR and llvm-stripped.

bytehook (Apache-2.0)
  com.bytedance:bytehook:1.1.1.
  - src/main/jniLibs/arm64-v8a/libbytehook.so — the stripped jni/ variant
    from the AAR, packaged as a plain jniLib and dlopen'd at runtime.
  - src/main/jni/include/bytehook.h — prefab header from the same AAR.

FCL / Zalith (GPL-3.0) were used as design references only; no GPL code was
copied.
