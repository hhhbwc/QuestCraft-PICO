/*
 * QuestCraft-PICO native hooks (Phase 0) — SDL_InitSubSystem hook.
 *
 * Ported from Amethyst-Android app_pojavlauncher/src/main/jni/native_hooks/sdl_hook.c
 * (AngelAuraMC/Amethyst-Android, branch v3_openjdk, commit 330c6eae, LGPL-3.0).
 * Phase 0 scope changes vs the original:
 *  - the joystick/gamepad tracking section is NOT ported (SDL input mapping is
 *    explicitly out of scope for Phase 0);
 *  - constants are inlined (NOTIF_TYPE_SDL = 0, ACTION_INIT_LAUNCHER_INTEGRATION = 0)
 *    to match the Dalvik/JRE CallbackBridge values.
 *
 * This hook is a fallback: the primary SDL enable path is the patched
 * org.lwjgl.sdl.SDLInit (overlay jar) calling
 * CallbackBridge.nativeNotifyLauncher(SDL, INIT) before SDL_Init. LWJGL calls
 * SDL through dlsym'd pointers so a PLT hook may not fire for it, which is why
 * the overlay notify exists at all.
 */
#include "native_hooks.h"
#include "environ/environ.h"
#include "SDL3/SDL.h"

#include <bytehook.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

DECL_DLSYM(SDL_InitSubSystem)
DECL_DLSYM(SDL_CreateWindow)
DECL_DLSYM(SDL_DestroyWindow)
DECL_DLSYM(SDL_SetError)
DECL_DLSYM(SDL_SetHint)
DECL_DLSYM(SDL_GetError)

bool notifyLauncher(JNIEnv *env, int type, const int *actions, jint count) {
    jmethodID mid = pojlib_getNotifyLauncherMethod();
    if (mid == NULL || pojav_environ == NULL || pojav_environ->bridgeClazz == NULL) {
        QCX_LOGE("notifyLauncher: bridge not ready (mid=%p clazz=%p)", mid,
                 (pojav_environ ? (void *) pojav_environ->bridgeClazz : NULL));
        return false;
    }

    jintArray arr = (*env)->NewIntArray(env, count);
    if (arr == NULL) {
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        return false;
    }
    (*env)->SetIntArrayRegion(env, arr, 0, count, (const jint *) actions);
    jboolean r = (*env)->CallStaticBooleanMethod(env, pojav_environ->bridgeClazz, mid,
                                                 (jint) type, arr);
    (*env)->DeleteLocalRef(env, arr);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        QCX_LOGE("notifyLauncher: Dalvik CallbackBridge.notifyLauncher threw");
        return false;
    }
    return r == JNI_TRUE;
}

static bool custom_SDL_InitSubSystem_Func(SDL_InitFlags flags) {
    // Attach to the Dalvik VM first: the launcher-side integration
    // (System.loadLibrary("SDL3") + SDL.setupJNI()) must run there.
    TRY_ATTACH_ENV(dvm_env, pojav_environ->dalvikJavaVMPtr,
                   "SDL_InitSubSystem: could not attach to the Dalvik VM",
        SET_DLSYM_PTR(dlopen("libSDL3.so", RTLD_NOLOAD), SDL_SetError);
        if (SDL_SetError_p)
            SDL_SetError_p("Failed to load SDL launcher integration android-side. "
                           "This is not an SDL bug, please contact the launcher developer.");
        return false;
    );

    // Just in case of bozo
    jint safeFlags = ((SDL_InitFlags) flags > (SDL_InitFlags) INT32_MAX) ? (jint) -1 : (jint) flags;

    const int notifyArgs[2] = { 0 /* ACTION_INIT_LAUNCHER_INTEGRATION */, (int) safeFlags };
    notifyLauncher(dvm_env, 0 /* NOTIF_TYPE_SDL */, notifyArgs, 2);

    // The launcher defaults this to false; SDL's default is false too, but
    // set it explicitly the way the launcher expects.
    SET_DLSYM_PTR(dlopen("libSDL3.so", RTLD_NOLOAD), SDL_SetHint);
    if (SDL_SetHint_p) SDL_SetHint_p("SDL_RETURN_KEY_HIDES_IME", "true");

    // FIXME: MobileGlues has issues with passing in the proper EGL params to make this work
    const char *egl = getenv("POJAVEXEC_EGL");
    if (egl && strncmp(egl, "libmobileglues", strlen("libmobileglues")) == 0) {
        SDL_SetHint_p("SDL_OPENGL_FORCE_SRGB_FRAMEBUFFER", "0");
    }

    // Call original func after doing all the needed setup
    bool r = BYTEHOOK_CALL_PREV(custom_SDL_InitSubSystem_Func, SDL_InitSubSystem_t, flags);
    if (!r) {
        SET_DLSYM_PTR(dlopen("libSDL3.so", RTLD_NOLOAD), SDL_GetError);
        QCX_LOGI("SDL_InitSubSystem error: %s", SDL_GetError_p ? SDL_GetError_p() : "(null)");
    }
    BYTEHOOK_POP_STACK();
    return r;
}

/*
 * 26.3 window multiplexer.
 *
 * Minecraft's OpenGL backend creates three SDL windows: a hidden utility window
 * (with the first GL context), a hidden "test" window, and finally the real
 * window. SDL's Android driver allows exactly ONE window, so the second create
 * fails with "Android only supports one window".
 *
 * LWJGL calls SDL through dlsym'd function pointers, so PLT hooks never fire.
 * These are inline hooks (shadowhook) on the resolved SDL addresses:
 *  - first window (utility)  -> real, kept for the initial context;
 *  - further UTILITY windows -> placeholder; GlDevice only creates and
 *    immediately destroys the test window;
 *  - the real (non-utility) window -> destroy the utility window first, then
 *    create the real one.
 */
static SDL_Window *sdl_real_window = NULL;
static char sdl_fake_window_storage;
#define SDL_FAKE_WINDOW ((SDL_Window *) &sdl_fake_window_storage)

static SDL_CreateWindow_t sdl_create_window_orig = NULL;
static SDL_DestroyWindow_t sdl_destroy_window_orig = NULL;

static SDL_Window *custom_SDL_CreateWindow(const char *title, int w, int h, SDL_WindowFlags flags) {
    if (sdl_create_window_orig == NULL) return NULL;

    if (sdl_real_window == NULL) {
        sdl_real_window = sdl_create_window_orig(title, w, h, flags);
        QCX_LOGI("SDL_CreateWindow(%s): first window %p", title ? title : "", sdl_real_window);
        return sdl_real_window;
    }

    if ((flags & SDL_WINDOW_UTILITY) != 0) {
        QCX_LOGI("SDL_CreateWindow(%s): placeholder window", title ? title : "");
        return SDL_FAKE_WINDOW;
    }

    if (sdl_destroy_window_orig != NULL) {
        sdl_destroy_window_orig(sdl_real_window);
    }
    sdl_real_window = NULL;

    sdl_real_window = sdl_create_window_orig(title, w, h, flags);
    QCX_LOGI("SDL_CreateWindow(%s): real window %p", title ? title : "", sdl_real_window);
    return sdl_real_window;
}

static void custom_SDL_DestroyWindow(SDL_Window *window) {
    if (window == SDL_FAKE_WINDOW) return;
    if (sdl_destroy_window_orig == NULL) return;
    if (window == sdl_real_window) sdl_real_window = NULL;
    sdl_destroy_window_orig(window);
}

typedef int (*shadowhook_init_fn_t)(int mode, bool debug);
typedef void *(*shadowhook_hook_sym_addr_fn_t)(void *sym_addr, void *new_addr, void **orig_addr);
typedef int (*shadowhook_get_errno_fn_t)(void);
typedef const char *(*shadowhook_to_errmsg_fn_t)(int);

/* Called from the game JVM after libSDL3.so is loaded there. sdl_handle is the
 * SharedLibrary handle, so the addresses belong to the instance the game uses. */
void install_sdl_window_hooks(void *sdl_handle) {
    if (sdl_handle == NULL) {
        QCX_LOGE("SDL window hooks: null SDL handle");
        return;
    }
    void *create_addr = dlsym(sdl_handle, "SDL_CreateWindow");
    void *destroy_addr = dlsym(sdl_handle, "SDL_DestroyWindow");
    if (create_addr == NULL || destroy_addr == NULL) {
        QCX_LOGE("SDL window hooks: symbols missing: %s", dlerror());
        return;
    }

    void *sh = dlopen("libshadowhook.so", RTLD_NOW);
    if (sh == NULL) {
        QCX_LOGE("SDL window hooks: libshadowhook missing: %s", dlerror());
        return;
    }
    shadowhook_init_fn_t sh_init = (shadowhook_init_fn_t) dlsym(sh, "shadowhook_init");
    shadowhook_hook_sym_addr_fn_t sh_hook =
            (shadowhook_hook_sym_addr_fn_t) dlsym(sh, "shadowhook_hook_sym_addr");
    if (sh_init == NULL || sh_hook == NULL) {
        QCX_LOGE("SDL window hooks: shadowhook API missing: %s", dlerror());
        return;
    }

    static bool shadowhook_ready = false;
    if (!shadowhook_ready) {
        int r = sh_init(1 /* SHADOWHOOK_MODE_UNIQUE */, false);
        QCX_LOGI("shadowhook_init -> %d", r);
        shadowhook_ready = true;
    }

    shadowhook_get_errno_fn_t sh_errno = (shadowhook_get_errno_fn_t) dlsym(sh, "shadowhook_get_errno");
    shadowhook_to_errmsg_fn_t sh_errmsg = (shadowhook_to_errmsg_fn_t) dlsym(sh, "shadowhook_to_errmsg");

    void *stub1 = sh_hook(create_addr, (void *) &custom_SDL_CreateWindow,
                          (void **) &sdl_create_window_orig);
    int e1 = sh_errno ? sh_errno() : 0;
    void *stub2 = sh_hook(destroy_addr, (void *) &custom_SDL_DestroyWindow,
                          (void **) &sdl_destroy_window_orig);
    int e2 = sh_errno ? sh_errno() : 0;
    QCX_LOGI("SDL inline hooks: create stub=%p errno=%d (%s) orig=%p; destroy stub=%p errno=%d (%s) orig=%p",
             stub1, e1, sh_errmsg ? sh_errmsg(e1) : "?", sdl_create_window_orig,
             stub2, e2, sh_errmsg ? sh_errmsg(e2) : "?", sdl_destroy_window_orig);
}

void create_sdl_hooks(void *bytehook_hook_all_fn) {
    bytehook_hook_all_fn_t *hook_all = (bytehook_hook_all_fn_t *) bytehook_hook_all_fn;
    // Don't set callee_path_name to anything besides NULL or else it won't be able to find the symbol
    bytehook_stub_t stub_SDL_InitSubSystem =
            hook_all(NULL, "SDL_InitSubSystem", (void *) &custom_SDL_InitSubSystem_Func, NULL, NULL);
    QCX_LOGI("Successfully initialized SDL hook, stub: %p", stub_SDL_InitSubSystem);
}
