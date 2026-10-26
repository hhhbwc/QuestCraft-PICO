/*
 * QuestCraft-PICO native hooks (Phase 0) — dlsym/JNI_OnLoad guard.
 *
 * Ported from Amethyst-Android app_pojavlauncher/src/main/jni/native_hooks/dlopen_hook.c
 * (AngelAuraMC/Amethyst-Android, branch v3_openjdk, commit 330c6eae, LGPL-3.0).
 * Phase 0 scope changes vs the original:
 *  - only libSDL3.so is tracked (this build does not ship libSDL2.so);
 *  - the __attribute__((constructor)) preload was REMOVED on purpose: pre-26.3
 *    launches must not touch libSDL3.so at all, so the preload now happens
 *    inside Java_pojlib_util_JREUtils_initializeHooks (SDL versions only),
 *    before the game JVM is started;
 *  - hooking dlopen() itself stays disabled for the same reason as upstream:
 *    it conflicts with the Turnip loader.
 *
 * The dlsym hook guarantees that libSDL3.so's JNI_OnLoad can only ever run on
 * the Dalvik VM (pojav_environ->dalvikJavaVMPtr). If the game JVM ever tried to
 * System.load the library, SDL would cache the HotSpot JVM in its Android glue
 * and every org.libsdl.app callback would run on the wrong VM.
 */
#include "native_hooks.h"
#include "environ/environ.h"

#include <bytehook.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

typedef void *(*dlopen_func_t)(const char *, int);
typedef void *(*dlsym_func_t)(void *, const char *);
typedef jint (*JNI_OnLoad_t)(JavaVM *vm, void *reserved);

typedef struct {
    const char *library_name;
    void *handle;
    JNI_OnLoad_t jni_onload;
} LibraryRedirect;

static LibraryRedirect libRedirectInfo[] = {
        { "libSDL3.so", NULL, NULL },
        { NULL, NULL, NULL } // Null termination
};

void pojlib_preload_sdl3(void) {
    if (libRedirectInfo[0].handle != NULL) return;
    libRedirectInfo[0].handle = dlopen("libSDL3.so", RTLD_LOCAL | RTLD_LAZY);
    QCX_LOGI("Preloaded libSDL3.so: %p%s%s", libRedirectInfo[0].handle,
             libRedirectInfo[0].handle == NULL ? " (" : "",
             libRedirectInfo[0].handle == NULL ? dlerror() : ")");
}

// Loose matching of lib file name to hard coded list
static LibraryRedirect *get_library_redirect_if_needed(const char *filename) {
    if (filename == NULL) return NULL;
    const char *basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;
    for (size_t i = 0; libRedirectInfo[i].library_name != NULL; ++i) {
        if (strstr(basename, libRedirectInfo[i].library_name) != NULL) {
            return &libRedirectInfo[i];
        }
    }
    return NULL;
}

static LibraryRedirect *get_redirect_by_handle(void *handle) {
    if (handle == NULL) return NULL;
    for (size_t i = 0; libRedirectInfo[i].library_name != NULL; ++i) {
        if (libRedirectInfo[i].handle == handle) return &libRedirectInfo[i];
    }
    return NULL;
}

static jint rerouted_jni_onload(LibraryRedirect *redirect, JavaVM *vm, void *reserved) {
    if (redirect == NULL) return JNI_ERR;

    if (pojav_environ != NULL &&
            pojav_environ->dalvikJavaVMPtr == vm &&
            redirect->jni_onload != NULL) {
        return ((JNI_OnLoad_t) redirect->jni_onload)(vm, reserved);
    }

    // Any other VM must not initialize SDL's JNI glue.
    return JNI_VERSION_1_4;
}

static jint jni_onload_sdl3(JavaVM *vm, void *reserved) {
    return rerouted_jni_onload(&libRedirectInfo[0], vm, reserved);
}

void *custom_dlsym(void *handle, const char *symbol) {
    void *result = BYTEHOOK_CALL_PREV(custom_dlsym, dlsym_func_t, handle, symbol);

    LibraryRedirect *redirect = get_redirect_by_handle(handle);
    if ((redirect != NULL) && strcmp(symbol, "JNI_OnLoad") == 0) {
        if (strstr(redirect->library_name, "libSDL3") != NULL) {
            redirect->jni_onload = (JNI_OnLoad_t) result;
            // This outputs in the minecraft logs
            QCX_LOGI("Intercepted dlsym on SDL3 JNI_OnLoad: %p", result);
            BYTEHOOK_POP_STACK();
            return (void *) &jni_onload_sdl3;
        }
    }
    BYTEHOOK_POP_STACK();
    return result;
}

void create_dlopen_hooks(void *bytehook_hook_all_fn) {
    bytehook_hook_all_fn_t *hook_all = (bytehook_hook_all_fn_t *) bytehook_hook_all_fn;
    // FIXME (upstream): Hooking dlopen causes a crash with the Turnip loader, so let's
    //  stop doing that entirely. Maybe android_dlopen_ext would help.
    bytehook_stub_t stub_dlsym =
            hook_all(NULL, "dlsym", (void *) &custom_dlsym, NULL, NULL);
    QCX_LOGI("Successfully initialized dlopen hooks, dlsym stub: %p", stub_dlsym);
}
