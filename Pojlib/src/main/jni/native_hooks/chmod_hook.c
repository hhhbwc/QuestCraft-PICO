/*
 * QuestCraft-PICO native hooks (Phase 0) — chmod/fchmod success hooks.
 *
 * Ported from Amethyst-Android app_pojavlauncher/src/main/jni/native_hooks/chmod_hook.c
 * (AngelAuraMC/Amethyst-Android, branch v3_openjdk, commit 330c6eae, LGPL-3.0),
 * byte-for-byte equivalent apart from the log tag/macros in native_hooks.h.
 *
 * Hooks chmod and fchmod to always return success so that Java NIO zipfs works
 * inside the launcher folder on older Android versions. Only installed below
 * API 29 (see exit_hook.c); this launcher's minSdk is 29, so in practice these
 * hooks stay dormant — the code is kept for parity with upstream.
 */
#include "native_hooks.h"

#include <bytehook.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

// Hooks for chmod and fchmod that always return success.
// This allows older Android versions to work with Java NIO zipfs inside of the launcher folder.
typedef int (*chmod_func)(const char *, mode_t);
typedef int (*fchmod_func)(int, mode_t);

#define TEMPLATE_HOOK(X, Y, Z, W) static int X(Y, mode_t mode) { \
    int result = BYTEHOOK_CALL_PREV(X, Z, W, mode); \
    if(result != 0) errno = 0; \
    BYTEHOOK_POP_STACK(); \
    return 0; \
} \

TEMPLATE_HOOK(custom_chmod, const char *filename, chmod_func, filename)
TEMPLATE_HOOK(custom_fchmod, int fd, fchmod_func, fd)

#undef TEMPLATE_HOOK

void create_chmod_hooks(void *bytehook_hook_all_fn) {
    bytehook_hook_all_fn_t *hook_all = (bytehook_hook_all_fn_t *) bytehook_hook_all_fn;
    bytehook_stub_t stub_chmod = hook_all(NULL, "chmod", &custom_chmod, NULL, NULL);
    bytehook_stub_t stub_fchmod = hook_all(NULL, "fchmod", &custom_fchmod, NULL, NULL);
    QCX_LOGI("Successfully initialized chmod hooks, stubs: %p %p", stub_chmod, stub_fchmod);
}
