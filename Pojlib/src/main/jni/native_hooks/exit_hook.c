/*
 * QuestCraft-PICO native hooks (Phase 0) — hook bootstrap entry.
 *
 * Ported from Amethyst-Android app_pojavlauncher/src/main/jni/native_hooks/exit_hook.c
 * (AngelAuraMC/Amethyst-Android, branch v3_openjdk, commit 330c6eae, LGPL-3.0).
 * Phase 0 scope changes vs the original:
 *  - the JNI entry is Java_pojlib_util_JREUtils_initializeHooks (Pojlib's
 *    package layout), called from JREUtils.launchJavaVM for SDL versions only;
 *  - the exit()/atexit hook is NOT ported: Pojlib's stdio_is.c has no
 *    nominal_exit(), and changing the process-exit semantics of pre-26.3
 *    launches is out of scope. Revisit when SDL input / lifecycle work lands;
 *  - libSDL3.so is preloaded here (SDL versions only) instead of in a library
 *    constructor, so 26.2 and older instances never load it.
 *
 * chmod hooks are only installed below API 29 (same as upstream), which in
 * practice means never for this launcher (minSdk 29); kept for parity.
 */
#include "native_hooks.h"

#include <android/api-level.h>
#include <bytehook.h>
#include <dlfcn.h>
#include <stdlib.h>

typedef int (*bytehook_init_fn_t)(int mode, bool debug);

static bool init_hooks(void) {
    void *bytehook_handle = dlopen("libbytehook.so", RTLD_NOW);
    if (bytehook_handle == NULL) {
        QCX_LOGE("Failed to load hook library: %s", dlerror());
        return false;
    }

    bytehook_hook_all_fn_t *bytehook_hook_all_p =
            (bytehook_hook_all_fn_t *) dlsym(bytehook_handle, "bytehook_hook_all");
    bytehook_init_fn_t bytehook_init_p =
            (bytehook_init_fn_t) dlsym(bytehook_handle, "bytehook_init");

    if (bytehook_hook_all_p == NULL || bytehook_init_p == NULL) {
        QCX_LOGE("bytehook symbols missing: %s", dlerror());
        dlclose(bytehook_handle);
        return false;
    }

    int bhook_status = bytehook_init_p(BYTEHOOK_MODE_AUTOMATIC, false);
    if (bhook_status == BYTEHOOK_STATUS_CODE_OK) {
        if (android_get_device_api_level() < 29) {
            create_chmod_hooks(bytehook_hook_all_p);
        }
        create_dlopen_hooks(bytehook_hook_all_p);
        create_sdl_hooks(bytehook_hook_all_p);
        // 26.3 loads SDL from the game JVM's namespace (nativeInitSdl); a Dalvik
    // preload here would create a second instance and split EGL/GL state.
        return true;
    }

    QCX_LOGE("bytehook_init failed (%i)", bhook_status);
    dlclose(bytehook_handle);
    return false;
}

JNIEXPORT void JNICALL
Java_pojlib_util_JREUtils_initializeHooks(JNIEnv *env, jclass clazz) {
    bool hooks_ready = init_hooks();
    if (!hooks_ready) {
        QCX_LOGE("Failed to initialize native hooks!");
    }
}
