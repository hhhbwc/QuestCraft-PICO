#pragma once
/*
 * QuestCraft-PICO native hooks (Phase 0) — shared header.
 *
 * Ported from Amethyst-Android app_pojavlauncher/src/main/jni/native_hooks/
 * (AngelAuraMC/Amethyst-Android, branch v3_openjdk, commit 330c6eae, LGPL-3.0),
 * trimmed to the Phase 0 scope:
 *  - no SDL gamepad/joystick tracking (SDL input mapping is out of scope),
 *  - no SDL2 entry (this build only ships libSDL3.so),
 *  - no constructor-time preload: libSDL3.so is preloaded from
 *    Java_pojlib_util_JREUtils_initializeHooks so that pre-26.3 launches
 *    never touch it.
 *
 * Hook installation is runtime-dlopen based (bytehook is packaged as a plain
 * jniLib and loaded with dlopen), exactly like Amethyst's exit_hook.c, so no
 * prefab/gradle wiring is needed for bytehook.
 */
#ifndef POJLIB_NATIVE_HOOKS_H
#define POJLIB_NATIVE_HOOKS_H

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <stdbool.h>
#include <stdint.h>

#define QCX_TAG "QCXNativeHooks"
#define QCX_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, QCX_TAG, __VA_ARGS__)
#define QCX_LOGI(...) __android_log_print(ANDROID_LOG_INFO,  QCX_TAG, __VA_ARGS__)

/*
 * bytehook_hook_all(caller_path_name, callee_path_name, new_func, stub, hooked_ctx)
 * The real type lives in bytehook.h; hooks are passed around as void* here so
 * this header does not force every translation unit to include bytehook.h.
 */
typedef void *bytehook_hook_all_fn_t(const char *caller_path_name, const char *callee_path_name,
                                     void *new_func, void *stub, void *hooked_ctx);

/* Declares a pointer typedef + slot for a libc/SDL symbol resolved via dlsym.
 * The typedef is a POINTER type because bytehook's BYTEHOOK_CALL_PREV casts
 * through it (((func_sig)bytehook_get_prev_func(...))(...)). */
#define DECL_DLSYM(NAME) \
    typedef __typeof__(NAME) *NAME##_t; \
    static NAME##_t NAME##_p = NULL;

#define SET_DLSYM_PTR(handle, NAME) \
    do { NAME##_p = (NAME##_t) dlsym((handle), #NAME); } while (0)

/*
 * Attaches the current thread to the given JavaVM. Amethyst-style macro with a
 * failure block; the attached flag is intentionally not detached, the game
 * threads are long-lived and Amethyst does the same.
 */
#define TRY_ATTACH_ENV(env_var, vm_ptr, fail_msg, fail_block) \
    JNIEnv *env_var = NULL; \
    bool env_var##_was_attached = false; \
    do { \
        if ((vm_ptr) == NULL) { \
            QCX_LOGE("%s", fail_msg); \
            fail_block; \
        } \
        if ((*vm_ptr)->GetEnv((vm_ptr), (void **) &(env_var), JNI_VERSION_1_4) != JNI_OK) { \
            if ((*vm_ptr)->AttachCurrentThread((vm_ptr), &(env_var), NULL) != JNI_OK) { \
                QCX_LOGE("%s", fail_msg); \
                fail_block; \
            } \
            env_var##_was_attached = true; \
        } \
    } while (0)

/* Implemented in input_bridge_v3.c: the Dalvik CallbackBridge.notifyLauncher method ID. */
jmethodID pojlib_getNotifyLauncherMethod(void);

/* Builds an int[] and calls Dalvik CallbackBridge.notifyLauncher(I[I)Z on the given env. */
bool notifyLauncher(JNIEnv *env, int type, const int *actions, jint count);

void create_sdl_hooks(void *bytehook_hook_all_fn);
void install_sdl_window_hooks(void *sdl_handle);
void create_dlopen_hooks(void *bytehook_hook_all_fn);
void create_chmod_hooks(void *bytehook_hook_all_fn);
void pojlib_preload_sdl3(void);

#endif /* POJLIB_NATIVE_HOOKS_H */
