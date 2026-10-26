/*
 * V3 input bridge implementation.
 *
 * Status:
 * - Active development
 * - Works with some bugs:
 *  + Modded versions gives broken stuff..
 *
 * 
 * - Implements glfwSetCursorPos() to handle grab camera pos correctly.
 */

#include <assert.h>
#include <dlfcn.h>
#include <jni.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>

#include "log.h"
#include "utils.h"
#include "environ/environ.h"
#include "native_hooks/native_hooks.h"

#define EVENT_TYPE_CHAR 1000
#define EVENT_TYPE_CHAR_MODS 1001
#define EVENT_TYPE_CURSOR_ENTER 1002
#define EVENT_TYPE_FRAMEBUFFER_SIZE 1004
#define EVENT_TYPE_KEY 1005
#define EVENT_TYPE_MOUSE_BUTTON 1006
#define EVENT_TYPE_SCROLL 1007
#define EVENT_TYPE_WINDOW_SIZE 1008

jint (*orig_ProcessImpl_forkAndExec)(JNIEnv *env, jobject process, jint mode, jbyteArray helperpath, jbyteArray prog, jbyteArray argBlock, jint argc, jbyteArray envBlock, jint envc, jbyteArray dir, jintArray std_fds, jboolean redirectErrorStream);

static void registerFunctions(JNIEnv *env);

// 26.3 (SDL) support:
// - s_runtimeHasGlfw: the game JVM may run Minecraft 26.3, whose classpath has NO
//   org.lwjgl.glfw classes at all; every GLFW-class dependent lookup below is gated on it.
// - method_notifyLauncher: Dalvik-side CallbackBridge.notifyLauncher(I[I)Z, used both by
//   the SDL notification native below and by the native_hooks sdl_hook.
static bool s_runtimeHasGlfw = false;
static jmethodID method_notifyLauncher = NULL;

jmethodID pojlib_getNotifyLauncherMethod(void) {
    return method_notifyLauncher;
}

jint JNI_OnLoad(JavaVM* vm, __attribute__((unused)) void* reserved) {
     if (pojav_environ->dalvikJavaVMPtr == NULL) {
        __android_log_print(ANDROID_LOG_INFO, "Native", "Saving DVM environ...");
        //Save dalvik global JavaVM pointer
        pojav_environ->dalvikJavaVMPtr = vm;
        (*vm)->GetEnv(vm, (void**) &pojav_environ->dalvikJNIEnvPtr_ANDROID, JNI_VERSION_1_4);
        pojav_environ->bridgeClazz = (*pojav_environ->dalvikJNIEnvPtr_ANDROID)->NewGlobalRef(pojav_environ->dalvikJNIEnvPtr_ANDROID,(*pojav_environ->dalvikJNIEnvPtr_ANDROID) ->FindClass(pojav_environ->dalvikJNIEnvPtr_ANDROID,"org/lwjgl/glfw/CallbackBridge"));
        pojav_environ->apiClass = (*pojav_environ->dalvikJNIEnvPtr_ANDROID)->NewGlobalRef(pojav_environ->dalvikJNIEnvPtr_ANDROID,(*pojav_environ->dalvikJNIEnvPtr_ANDROID) ->FindClass(pojav_environ->dalvikJNIEnvPtr_ANDROID,"pojlib/API"));
        pojav_environ->method_accessAndroidClipboard = (*pojav_environ->dalvikJNIEnvPtr_ANDROID)->GetStaticMethodID(pojav_environ->dalvikJNIEnvPtr_ANDROID, pojav_environ->bridgeClazz, "accessAndroidClipboard", "(ILjava/lang/String;)Ljava/lang/String;");
        pojav_environ->method_onGrabStateChanged = (*pojav_environ->dalvikJNIEnvPtr_ANDROID)->GetStaticMethodID(pojav_environ->dalvikJNIEnvPtr_ANDROID, pojav_environ->bridgeClazz, "onGrabStateChanged", "(Z)V");
        pojav_environ->method_restartUnity = (*pojav_environ->dalvikJNIEnvPtr_ANDROID)->GetStaticMethodID(pojav_environ->dalvikJNIEnvPtr_ANDROID, pojav_environ->bridgeClazz, "restartUnitySession", "(Landroid/app/Activity;)V");
        // SDL notification path (26.3+): the Dalvik CallbackBridge carries notifyLauncher.
        // Its absence must not break 26.2, so just log and continue without it.
        method_notifyLauncher = (*pojav_environ->dalvikJNIEnvPtr_ANDROID)->GetStaticMethodID(pojav_environ->dalvikJNIEnvPtr_ANDROID, pojav_environ->bridgeClazz, "notifyLauncher", "(I[I)Z");
        if (method_notifyLauncher == NULL) {
            if ((*pojav_environ->dalvikJNIEnvPtr_ANDROID)->ExceptionCheck(pojav_environ->dalvikJNIEnvPtr_ANDROID)) {
                (*pojav_environ->dalvikJNIEnvPtr_ANDROID)->ExceptionClear(pojav_environ->dalvikJNIEnvPtr_ANDROID);
            }
            __android_log_print(ANDROID_LOG_INFO, "Native", "CallbackBridge.notifyLauncher not found; SDL notify disabled");
        }
        pojav_environ->isUseStackQueueCall = JNI_FALSE;
    } else if (pojav_environ->dalvikJavaVMPtr != vm) {
        __android_log_print(ANDROID_LOG_INFO, "Native", "Saving JVM environ...");
        pojav_environ->runtimeJavaVMPtr = vm;
        (*vm)->GetEnv(vm, (void**) &pojav_environ->runtimeJNIEnvPtr_JRE, JNI_VERSION_1_4);
        // Minecraft 26.3 uses LWJGL 3.4.3 + SDL and has NO org.lwjgl.glfw classes.
        // Resolving GLFW unconditionally here used to fail the whole JNI_OnLoad, which
        // took down libpojavexec/vloader for every 26.3 launch. Resolve it conditionally:
        // 26.2 and older still get the full GLFW bridge, 26.3 skips it and talks to the
        // launcher through the SDL glue instead.
        jclass glfwClass = (*pojav_environ->runtimeJNIEnvPtr_JRE)->FindClass(pojav_environ->runtimeJNIEnvPtr_JRE, "org/lwjgl/glfw/GLFW");
        if ((*pojav_environ->runtimeJNIEnvPtr_JRE)->ExceptionCheck(pojav_environ->runtimeJNIEnvPtr_JRE)) {
            (*pojav_environ->runtimeJNIEnvPtr_JRE)->ExceptionClear(pojav_environ->runtimeJNIEnvPtr_JRE);
            glfwClass = NULL;
        }
        s_runtimeHasGlfw = glfwClass != NULL;
        if (s_runtimeHasGlfw) {
            pojav_environ->vmGlfwClass = (*pojav_environ->runtimeJNIEnvPtr_JRE)->NewGlobalRef(pojav_environ->runtimeJNIEnvPtr_JRE, glfwClass);
            pojav_environ->method_glftSetWindowAttrib = (*pojav_environ->runtimeJNIEnvPtr_JRE)->GetStaticMethodID(pojav_environ->runtimeJNIEnvPtr_JRE, pojav_environ->vmGlfwClass, "glfwSetWindowAttrib", "(JII)V");
            pojav_environ->method_internalWindowSizeChanged = (*pojav_environ->runtimeJNIEnvPtr_JRE)->GetStaticMethodID(pojav_environ->runtimeJNIEnvPtr_JRE, pojav_environ->vmGlfwClass, "internalWindowSizeChanged", "(JII)V");
            jfieldID field_keyDownBuffer = (*pojav_environ->runtimeJNIEnvPtr_JRE)->GetStaticFieldID(pojav_environ->runtimeJNIEnvPtr_JRE, pojav_environ->vmGlfwClass, "keyDownBuffer", "Ljava/nio/ByteBuffer;");
            jobject keyDownBufferJ = (*pojav_environ->runtimeJNIEnvPtr_JRE)->GetStaticObjectField(pojav_environ->runtimeJNIEnvPtr_JRE, pojav_environ->vmGlfwClass, field_keyDownBuffer);
            pojav_environ->keyDownBuffer = (*pojav_environ->runtimeJNIEnvPtr_JRE)->GetDirectBufferAddress(pojav_environ->runtimeJNIEnvPtr_JRE, keyDownBufferJ);
            jfieldID field_mouseDownBuffer = (*pojav_environ->runtimeJNIEnvPtr_JRE)->GetStaticFieldID(pojav_environ->runtimeJNIEnvPtr_JRE, pojav_environ->vmGlfwClass, "mouseDownBuffer", "Ljava/nio/ByteBuffer;");
            jobject mouseDownBufferJ = (*pojav_environ->runtimeJNIEnvPtr_JRE)->GetStaticObjectField(pojav_environ->runtimeJNIEnvPtr_JRE, pojav_environ->vmGlfwClass, field_mouseDownBuffer);
            pojav_environ->mouseDownBuffer = (*pojav_environ->runtimeJNIEnvPtr_JRE)->GetDirectBufferAddress(pojav_environ->runtimeJNIEnvPtr_JRE, mouseDownBufferJ);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "Native", "No org.lwjgl.glfw.GLFW on the runtime classpath; skipping GLFW bridge (SDL mode)");
        }
        hookExec();
        installLinkerBugMitigation();
        installEMUIIteratorMititgation();
    }

    if(pojav_environ->dalvikJavaVMPtr == vm) {
        //perform in all DVM instances, not only during first ever set up
        JNIEnv *env;
        (*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4);
        registerFunctions(env);
    }
    pojav_environ->isGrabbing = JNI_FALSE;

    return JNI_VERSION_1_4;
}

#define ADD_CALLBACK_WWIN(NAME) \
JNIEXPORT jlong JNICALL Java_org_lwjgl_glfw_GLFW_nglfwSet##NAME##Callback(JNIEnv * env, jclass cls, jlong window, jlong callbackptr) { \
    void** oldCallback = (void**) &pojav_environ->GLFW_invoke_##NAME; \
    pojav_environ->GLFW_invoke_##NAME = (GLFW_invoke_##NAME##_func*) (uintptr_t) callbackptr; \
    return (jlong) (uintptr_t) *oldCallback; \
}

ADD_CALLBACK_WWIN(Char)
ADD_CALLBACK_WWIN(CharMods)
ADD_CALLBACK_WWIN(CursorEnter)
ADD_CALLBACK_WWIN(CursorPos)
ADD_CALLBACK_WWIN(FramebufferSize)
ADD_CALLBACK_WWIN(Key)
ADD_CALLBACK_WWIN(MouseButton)
ADD_CALLBACK_WWIN(Scroll)
ADD_CALLBACK_WWIN(WindowSize)

#undef ADD_CALLBACK_WWIN

void handleFramebufferSizeJava(long window, int w, int h) {
    if (!s_runtimeHasGlfw) return;
    (*pojav_environ->runtimeJNIEnvPtr_JRE)->CallStaticVoidMethod(pojav_environ->runtimeJNIEnvPtr_JRE, pojav_environ->vmGlfwClass, pojav_environ->method_internalWindowSizeChanged, (long)window, w, h);
}

void pojavPumpEvents(void* window) {
    if(pojav_environ->shouldUpdateMouse) {
        pojav_environ->GLFW_invoke_CursorPos(window, floor(pojav_environ->cursorX),
                                             floor(pojav_environ->cursorY));
    }

    size_t index = pojav_environ->outEventIndex;
    size_t targetIndex = pojav_environ->outTargetIndex;

    while (targetIndex != index) {
        GLFWInputEvent event = pojav_environ->events[index];
        switch (event.type) {
            case EVENT_TYPE_CHAR:
                if(pojav_environ->GLFW_invoke_Char) pojav_environ->GLFW_invoke_Char(window, event.i1);
                break;
            case EVENT_TYPE_CHAR_MODS:
                if(pojav_environ->GLFW_invoke_CharMods) pojav_environ->GLFW_invoke_CharMods(window, event.i1, event.i2);
                break;
            case EVENT_TYPE_KEY:
                if(pojav_environ->GLFW_invoke_Key) pojav_environ->GLFW_invoke_Key(window, event.i1, event.i2, event.i3, event.i4);
                break;
            case EVENT_TYPE_MOUSE_BUTTON:
                if(pojav_environ->GLFW_invoke_MouseButton) pojav_environ->GLFW_invoke_MouseButton(window, event.i1, event.i2, event.i3);
                break;
            case EVENT_TYPE_SCROLL:
                if(pojav_environ->GLFW_invoke_Scroll) pojav_environ->GLFW_invoke_Scroll(window, event.i1, event.i2);
                break;
            case EVENT_TYPE_FRAMEBUFFER_SIZE:
                handleFramebufferSizeJava(pojav_environ->showingWindow, event.i1, event.i2);
                if(pojav_environ->GLFW_invoke_FramebufferSize) pojav_environ->GLFW_invoke_FramebufferSize(window, event.i1, event.i2);
                break;
            case EVENT_TYPE_WINDOW_SIZE:
                handleFramebufferSizeJava(pojav_environ->showingWindow, event.i1, event.i2);
                if(pojav_environ->GLFW_invoke_WindowSize) pojav_environ->GLFW_invoke_WindowSize(window, event.i1, event.i2);
                break;
        }

        index++;
        if (index >= EVENT_WINDOW_SIZE)
            index -= EVENT_WINDOW_SIZE;
    }

    // The out target index is updated by the rewinder
}

/** Prepare the library for sending out callbacks to all windows */
void pojavStartPumping() {
    size_t counter = atomic_load_explicit(&pojav_environ->eventCounter, memory_order_acquire);
    size_t index = pojav_environ->outEventIndex;

    unsigned targetIndex = index + counter;
    if (targetIndex >= EVENT_WINDOW_SIZE)
        targetIndex -= EVENT_WINDOW_SIZE;

    // Only accessed by one unique thread, no need for atomic store
    pojav_environ->inEventCount = counter;
    pojav_environ->outTargetIndex = targetIndex;

    //PumpEvents is called for every window, so this logic should be there in order to correctly distribute events to all windows.
    if((pojav_environ->cLastX != pojav_environ->cursorX || pojav_environ->cLastY != pojav_environ->cursorY) && pojav_environ->GLFW_invoke_CursorPos) {
        pojav_environ->cLastX = pojav_environ->cursorX;
        pojav_environ->cLastY = pojav_environ->cursorY;
        pojav_environ->shouldUpdateMouse = true;
    }
}

/** Prepare the library for the next round of new events */
void pojavStopPumping() {
    pojav_environ->outEventIndex = pojav_environ->outTargetIndex;

    // New events may have arrived while pumping, so remove only the difference before the start and end of execution
    atomic_fetch_sub_explicit(&pojav_environ->eventCounter, pojav_environ->inEventCount, memory_order_acquire);
    // Make sure the next frame won't send mouse updates if it's unnecessary
    pojav_environ->shouldUpdateMouse = false;
}

JNIEXPORT void JNICALL
Java_org_lwjgl_glfw_GLFW_nglfwGetCursorPos(JNIEnv *env, __attribute__((unused)) jclass clazz, __attribute__((unused)) jlong window, jobject xpos,
                                           jobject ypos) {
    *(double*)(*env)->GetDirectBufferAddress(env, xpos) = pojav_environ->cursorX;
    *(double*)(*env)->GetDirectBufferAddress(env, ypos) = pojav_environ->cursorY;
}

JNIEXPORT void JNICALL JavaCritical_org_lwjgl_glfw_GLFW_nglfwGetCursorPosA(__attribute__((unused)) jlong window, jint lengthx, jdouble* xpos, jint lengthy, jdouble* ypos) {
    *xpos = pojav_environ->cursorX;
    *ypos = pojav_environ->cursorY;
}

JNIEXPORT void JNICALL
Java_org_lwjgl_glfw_GLFW_nglfwGetCursorPosA(JNIEnv *env, __attribute__((unused)) jclass clazz, __attribute__((unused)) jlong window,
                                            jdoubleArray xpos, jdoubleArray ypos) {
    (*env)->SetDoubleArrayRegion(env, xpos, 0,1, &pojav_environ->cursorX);
    (*env)->SetDoubleArrayRegion(env, ypos, 0,1, &pojav_environ->cursorY);
}

JNIEXPORT void JNICALL JavaCritical_org_lwjgl_glfw_GLFW_glfwSetCursorPos(__attribute__((unused)) jlong window, jdouble xpos,
                                                                         jdouble ypos) {
    pojav_environ->cLastX = pojav_environ->cursorX = xpos;
    pojav_environ->cLastY = pojav_environ->cursorY = ypos;
}

JNIEXPORT void JNICALL
Java_org_lwjgl_glfw_GLFW_glfwSetCursorPos(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jclass clazz, __attribute__((unused)) jlong window, jdouble xpos,
                                          jdouble ypos) {
    JavaCritical_org_lwjgl_glfw_GLFW_glfwSetCursorPos(window, xpos, ypos);
}



void sendData(int type, int i1, int i2, int i3, int i4) {
    GLFWInputEvent *event = &pojav_environ->events[pojav_environ->inEventIndex];
    event->type = type;
    event->i1 = i1;
    event->i2 = i2;
    event->i3 = i3;
    event->i4 = i4;

    if (++pojav_environ->inEventIndex >= EVENT_WINDOW_SIZE)
        pojav_environ->inEventIndex -= EVENT_WINDOW_SIZE;

    atomic_fetch_add_explicit(&pojav_environ->eventCounter, 1, memory_order_acquire);
}

/**
 * Hooked version of java.lang.UNIXProcess.forkAndExec()
 * which is used to handle the "open" command.
 */
jint
hooked_ProcessImpl_forkAndExec(JNIEnv *env, jobject process, jint mode, jbyteArray helperpath, jbyteArray prog, jbyteArray argBlock, jint argc, jbyteArray envBlock, jint envc, jbyteArray dir, jintArray std_fds, jboolean redirectErrorStream) {
    char *pProg = (char *)((*env)->GetByteArrayElements(env, prog, NULL));

    // Here we only handle the "xdg-open" command
    if (strcmp(basename(pProg), "xdg-open") != 0) {
        (*env)->ReleaseByteArrayElements(env, prog, (jbyte *)pProg, 0);
        return orig_ProcessImpl_forkAndExec(env, process, mode, helperpath, prog, argBlock, argc, envBlock, envc, dir, std_fds, redirectErrorStream);
    }
    (*env)->ReleaseByteArrayElements(env, prog, (jbyte *)pProg, 0);

    Java_org_lwjgl_glfw_CallbackBridge_nativeClipboard(env, NULL, /* CLIPBOARD_OPEN */ 2002, argBlock);
    return 0;
}

void hookExec() {
    jclass cls;
    orig_ProcessImpl_forkAndExec = dlsym(RTLD_DEFAULT, "Java_java_lang_UNIXProcess_forkAndExec");
    if (!orig_ProcessImpl_forkAndExec) {
        orig_ProcessImpl_forkAndExec = dlsym(RTLD_DEFAULT, "Java_java_lang_ProcessImpl_forkAndExec");
        cls = (*pojav_environ->runtimeJNIEnvPtr_JRE)->FindClass(pojav_environ->runtimeJNIEnvPtr_JRE, "java/lang/ProcessImpl");
    } else {
        cls = (*pojav_environ->runtimeJNIEnvPtr_JRE)->FindClass(pojav_environ->runtimeJNIEnvPtr_JRE, "java/lang/UNIXProcess");
    }
    JNINativeMethod methods[] = {
            {"forkAndExec", "(I[B[B[BI[BI[B[IZ)I", (void *)&hooked_ProcessImpl_forkAndExec}
    };
    (*pojav_environ->runtimeJNIEnvPtr_JRE)->RegisterNatives(pojav_environ->runtimeJNIEnvPtr_JRE, cls, methods, 1);
    printf("Registered forkAndExec\n");
}

/**
 * Basically a verbatim implementation of ndlopen(), found at
 * https://github.com/PojavLauncherTeam/lwjgl3/blob/3.3.1/modules/lwjgl/core/src/generated/c/linux/org_lwjgl_system_linux_DynamicLinkLoader.c#L11
 * The idea is that since, on Android 10 and earlier, the linker doesn't really do namespace nesting.
 * It is not a problem as most of the libraries are in the launcher path, but when you try to run
 * VulkanMod which loads shaderc outside of the default jni libs directory through this method,
 * it can't load it because the path is not in the allowed paths for the anonymous namesapce.
 * This method fixes the issue by being in libpojavexec, and thus being in the classloader namespace
 */
jlong ndlopen_bugfix(__attribute__((unused)) JNIEnv *env,
                     __attribute__((unused)) jclass class,
                     jlong filename_ptr,
                     jint jmode) {
    const char* filename = (const char*) filename_ptr;
    int mode = (int)jmode;
    return (jlong) dlopen(filename, mode);
}

/**
 * Install the linker bug mitigation for Android 10 and lower. Fixes VulkanMod crashing on these
 * Android versions due to missing namespace nesting.
 */
void installLinkerBugMitigation() {
    if(android_get_device_api_level() >= 30) return;
    __android_log_print(ANDROID_LOG_INFO, "Api29LinkerFix", "API < 30 detected, installing linker bug mitigation");
    JNIEnv* env = pojav_environ->runtimeJNIEnvPtr_JRE;
    jclass dynamicLinkLoader = (*env)->FindClass(env, "org/lwjgl/system/linux/DynamicLinkLoader");
    if(dynamicLinkLoader == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "Api29LinkerFix", "Failed to find the target class");
        (*env)->ExceptionClear(env);
        return;
    }
    JNINativeMethod ndlopenMethod[] = {
            {"ndlopen", "(JI)J", &ndlopen_bugfix}
    };
    if((*env)->RegisterNatives(env, dynamicLinkLoader, ndlopenMethod, 1) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "Api29LinkerFix", "Failed to register the bugfix method");
        (*env)->ExceptionClear(env);
    }
}

/**
 * This function is meant as a substitute for SharedLibraryUtil.getLibraryPath() that just returns 0
 * (thus making the parent Java function return null). This is done to avoid using the LWJGL's default function,
 * which will hang the crappy EMUI linker by dlopen()ing inside of dl_iterate_phdr().
 * @return 0, to make the parent Java function return null immediately.
 * For reference: https://github.com/PojavLauncherTeam/lwjgl3/blob/fix_huawei_hang/modules/lwjgl/core/src/main/java/org/lwjgl/system/SharedLibraryUtil.java
 */
jint getLibraryPath_fix(__attribute__((unused)) JNIEnv *env,
                        __attribute__((unused)) jclass class,
                        __attribute__((unused)) jlong pLibAddress,
                        __attribute__((unused)) jlong sOutAddress,
                        __attribute__((unused)) jint bufSize){
    return 0;
}

/**
 * Install the linker hang mitigation that is meant to prevent linker hangs on old EMUI firmware.
 */
void installEMUIIteratorMititgation() {
    if(getenv("POJAV_EMUI_ITERATOR_MITIGATE") == NULL) return;
    __android_log_print(ANDROID_LOG_INFO, "EMUIIteratorFix", "Installing...");
    JNIEnv* env = pojav_environ->runtimeJNIEnvPtr_JRE;
    jclass sharedLibraryUtil = (*env)->FindClass(env, "org/lwjgl/system/SharedLibraryUtil");
    if(sharedLibraryUtil == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "EMUIIteratorFix", "Failed to find the target class");
        (*env)->ExceptionClear(env);
        return;
    }
    JNINativeMethod getLibraryPathMethod[] = {
            {"getLibraryPath", "(JJI)I", &getLibraryPath_fix}
    };
    if((*env)->RegisterNatives(env, sharedLibraryUtil, getLibraryPathMethod, 1) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "EMUIIteratorFix", "Failed to register the mitigation method");
        (*env)->ExceptionClear(env);
    }
}

void critical_set_stackqueue(jboolean use_input_stack_queue) {
    pojav_environ->isUseStackQueueCall = (int) use_input_stack_queue;
}

void noncritical_set_stackqueue(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jclass clazz, jboolean use_input_stack_queue) {
    critical_set_stackqueue(use_input_stack_queue);
}

/**
 * Attaches the current thread to the Dalvik VM only if needed.
 * Returns true when this call performed the attach (and therefore owns the
 * matching detach). Never detach a thread that was already attached: SDL
 * caches JNIEnv pointers in pthread TLS, and detaching underneath it leaves
 * dangling env pointers.
 */
static bool attach_dalvik(JNIEnv **out) {
    JavaVM *vm = pojav_environ->dalvikJavaVMPtr;
    if (vm == NULL) { *out = NULL; return false; }
    if ((*vm)->GetEnv(vm, (void **) out, JNI_VERSION_1_4) == JNI_OK) {
        return false;
    }
    if ((*vm)->AttachCurrentThread(vm, out, NULL) != JNI_OK) {
        *out = NULL;
        return false;
    }
    return true;
}

JNIEXPORT jstring JNICALL Java_org_lwjgl_glfw_CallbackBridge_nativeClipboard(JNIEnv* env, __attribute__((unused)) jclass clazz, jint action, jbyteArray copySrc) {
#ifdef DEBUG
    LOGD("Debug: Clipboard access is going on\n", pojav_environ->isUseStackQueueCall);
#endif

    JNIEnv *dalvikEnv;
    bool detach = attach_dalvik(&dalvikEnv);
    assert(dalvikEnv != NULL);
    assert(pojav_environ->bridgeClazz != NULL);

    LOGD("Clipboard: Converting string\n");
    char *copySrcC;
    jstring copyDst = NULL;
    if (copySrc) {
        copySrcC = (char *)((*env)->GetByteArrayElements(env, copySrc, NULL));
        copyDst = (*dalvikEnv)->NewStringUTF(dalvikEnv, copySrcC);
    }

    LOGD("Clipboard: Calling 2nd\n");
    jstring pasteDst = convertStringJVM(dalvikEnv, env, (jstring) (*dalvikEnv)->CallStaticObjectMethod(dalvikEnv, pojav_environ->bridgeClazz, pojav_environ->method_accessAndroidClipboard, action, copyDst));

    if (copySrc) {
        (*dalvikEnv)->DeleteLocalRef(dalvikEnv, copyDst);
        (*env)->ReleaseByteArrayElements(env, copySrc, (jbyte *)copySrcC, 0);
    }
    if (detach) (*pojav_environ->dalvikJavaVMPtr)->DetachCurrentThread(pojav_environ->dalvikJavaVMPtr);
    return pasteDst;
}

JNIEXPORT jboolean JNICALL JavaCritical_org_lwjgl_glfw_CallbackBridge_nativeSetInputReady(jboolean inputReady) {
#ifdef DEBUG
    LOGD("Debug: Changing input state, isReady=%d, pojav_environ->isUseStackQueueCall=%d\n", inputReady, pojav_environ->isUseStackQueueCall);
#endif
    __android_log_print(ANDROID_LOG_INFO, "NativeInput", "Input ready: %i", inputReady);
    pojav_environ->isInputReady = inputReady;
    return pojav_environ->isUseStackQueueCall;
}

JNIEXPORT jboolean JNICALL Java_org_lwjgl_glfw_CallbackBridge_nativeSetInputReady(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jboolean inputReady) {
    return JavaCritical_org_lwjgl_glfw_CallbackBridge_nativeSetInputReady(inputReady);
}


JNIEXPORT void JNICALL
Java_org_lwjgl_glfw_CallbackBridge_restartUnitySession(JNIEnv *env, jclass clazz) {
    JNIEnv *dalvikEnv;
    bool detach = attach_dalvik(&dalvikEnv);
    (*dalvikEnv)->CallStaticVoidMethod(dalvikEnv, pojav_environ->bridgeClazz, pojav_environ->method_restartUnity, pojav_environ->activity);
    if (detach) (*pojav_environ->dalvikJavaVMPtr)->DetachCurrentThread(pojav_environ->dalvikJavaVMPtr);
}

JNIEXPORT void JNICALL Java_org_lwjgl_glfw_CallbackBridge_nativeSetGrabbing(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jboolean grabbing) {
    JNIEnv *dalvikEnv;
    bool detach = attach_dalvik(&dalvikEnv);
    (*dalvikEnv)->CallStaticVoidMethod(dalvikEnv, pojav_environ->bridgeClazz, pojav_environ->method_onGrabStateChanged, grabbing);
    if (detach) (*pojav_environ->dalvikJavaVMPtr)->DetachCurrentThread(pojav_environ->dalvikJavaVMPtr);
    pojav_environ->isGrabbing = grabbing;
}

/**
 * SDL notification path (MC 26.3): the game JVM calls this on the overlay's
 * org.lwjgl.glfw.CallbackBridge; we forward to the Dalvik-side CallbackBridge
 * which performs System.loadLibrary("SDL3"), SDL.setupJNI() and flips
 * MinecraftGLSurface.sdlEnabled. Signature matches Amethyst:
 * notifyLauncher(int type, int... actions) => JNI (I[I)Z.
 */
JNIEXPORT jboolean JNICALL
Java_org_lwjgl_glfw_CallbackBridge_nativeNotifyLauncher(JNIEnv *env, __attribute__((unused)) jclass clazz, jint type, jintArray actions) {
    if (pojav_environ->dalvikJavaVMPtr == NULL || pojav_environ->bridgeClazz == NULL || method_notifyLauncher == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "Native", "nativeNotifyLauncher: bridge not ready");
        return JNI_FALSE;
    }

    JNIEnv *dalvikEnv;
    (*pojav_environ->dalvikJavaVMPtr)->AttachCurrentThread(pojav_environ->dalvikJavaVMPtr, &dalvikEnv, NULL);

    jsize len = actions != NULL ? (*env)->GetArrayLength(env, actions) : 0;
    jint *elems = actions != NULL ? (*env)->GetIntArrayElements(env, actions, NULL) : NULL;
    jintArray dalvikActions = len > 0 ? (*dalvikEnv)->NewIntArray(dalvikEnv, len) : NULL;
    if (dalvikActions != NULL && elems != NULL) {
        (*dalvikEnv)->SetIntArrayRegion(dalvikEnv, dalvikActions, 0, len, elems);
    }

    jboolean r = (*dalvikEnv)->CallStaticBooleanMethod(dalvikEnv, pojav_environ->bridgeClazz, method_notifyLauncher, type, dalvikActions);

    if (dalvikActions != NULL) (*dalvikEnv)->DeleteLocalRef(dalvikEnv, dalvikActions);
    if (elems != NULL) (*env)->ReleaseIntArrayElements(env, actions, elems, 0);
    if ((*dalvikEnv)->ExceptionCheck(dalvikEnv)) {
        (*dalvikEnv)->ExceptionClear(dalvikEnv);
        __android_log_print(ANDROID_LOG_ERROR, "Native", "Dalvik notifyLauncher threw");
        r = JNI_FALSE;
    }
    // Deliberately NOT detaching: SDL caches this thread's JNIEnv in pthread
    // TLS (Android_JNI_GetEnv), so a detach here leaves SDL with a freed env
    // and the next Java callback jumps through a dead function table.
    return r;
}

static void *sdl_handle = NULL;
typedef jint (*jni_onload_func_t)(JavaVM *, void *);

/**
 * 26.3 SDL bring-up (called from the game JVM's overlay SDL class):
 *  - dlopen libSDL3.so from the CALLER's namespace (the JVM's), so SDL, EGL and
 *    MobileGlues all end up in one linker namespace and share one instance;
 *  - invoke SDL's JNI_OnLoad with the DALVIK VM (after attaching this thread),
 *    so SDL's Android glue caches the right VM and registers SDLActivity's
 *    native methods on the Dalvik classes.
 * HotSpot must never call JNI_OnLoad here (it would cache the JRE VM).
 */
JNIEXPORT jboolean JNICALL
Java_org_lwjgl_glfw_CallbackBridge_nativeInitSdl(JNIEnv *env, __attribute__((unused)) jclass clazz, jstring path) {
    if (path == NULL) return JNI_FALSE;
    const char *cpath = (*env)->GetStringUTFChars(env, path, NULL);
    if (cpath == NULL) return JNI_FALSE;

    if (sdl_handle == NULL) {
        sdl_handle = dlopen(cpath, RTLD_NOW | RTLD_LOCAL);
        if (sdl_handle == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "QCXSDL", "dlopen(%s) failed: %s", cpath, dlerror());
        }
    }
    (*env)->ReleaseStringUTFChars(env, path, cpath);
    if (sdl_handle == NULL) return JNI_FALSE;

    jni_onload_func_t onload = (jni_onload_func_t) dlsym(sdl_handle, "JNI_OnLoad");
    if (onload == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "QCXSDL", "libSDL3 has no JNI_OnLoad");
        return JNI_FALSE;
    }

    JavaVM *dalvik = pojav_environ != NULL ? pojav_environ->dalvikJavaVMPtr : NULL;
    if (dalvik == NULL || pojav_environ->bridgeClazz == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "QCXSDL", "Dalvik VM/bridge not registered yet");
        return JNI_FALSE;
    }

    JNIEnv *dalvikEnv = NULL;
    if ((*dalvik)->GetEnv(dalvik, (void **) &dalvikEnv, JNI_VERSION_1_4) != JNI_OK) {
        if ((*dalvik)->AttachCurrentThread(dalvik, &dalvikEnv, NULL) != JNI_OK) {
            __android_log_print(ANDROID_LOG_ERROR, "QCXSDL", "attach to Dalvik failed");
            return JNI_FALSE;
        }
    }

    // FindClass inside SDL's JNI_OnLoad must run with an app-classloader Java
    // frame; a bare native-attached thread only sees the boot classloader.
    jmethodID mid = (*dalvikEnv)->GetStaticMethodID(
            dalvikEnv, pojav_environ->bridgeClazz, "runSdlJniOnLoad", "(J)Z");
    if (mid == NULL) {
        if ((*dalvikEnv)->ExceptionCheck(dalvikEnv)) (*dalvikEnv)->ExceptionClear(dalvikEnv);
        __android_log_print(ANDROID_LOG_ERROR, "QCXSDL", "CallbackBridge.runSdlJniOnLoad missing");
        return JNI_FALSE;
    }

    jboolean ok = (*dalvikEnv)->CallStaticBooleanMethod(
            dalvikEnv, pojav_environ->bridgeClazz, mid, (jlong) (uintptr_t) onload);
    if ((*dalvikEnv)->ExceptionCheck(dalvikEnv)) {
        (*dalvikEnv)->ExceptionClear(dalvikEnv);
        ok = JNI_FALSE;
    }
    __android_log_print(ANDROID_LOG_INFO, "QCXSDL", "SDL JNI_OnLoad via Dalvik -> %d", ok);
    // Keep the thread attached: SDL caches this thread's JNIEnv in TLS.
    return ok;
}

JNIEXPORT void JNICALL
Java_org_lwjgl_glfw_CallbackBridge_nativeNotifyGameReady(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jclass clazz) {
    JavaVM *dalvik = pojav_environ != NULL ? pojav_environ->dalvikJavaVMPtr : NULL;
    if (dalvik == NULL || pojav_environ->bridgeClazz == NULL) return;
    JNIEnv *dalvikEnv = NULL;
    if ((*dalvik)->GetEnv(dalvik, (void **) &dalvikEnv, JNI_VERSION_1_4) != JNI_OK) {
        if ((*dalvik)->AttachCurrentThread(dalvik, &dalvikEnv, NULL) != JNI_OK) return;
    }
    jmethodID mid = (*dalvikEnv)->GetStaticMethodID(
            dalvikEnv, pojav_environ->bridgeClazz, "onGameReady", "()V");
    if (mid == NULL) {
        if ((*dalvikEnv)->ExceptionCheck(dalvikEnv)) (*dalvikEnv)->ExceptionClear(dalvikEnv);
        return;
    }
    (*dalvikEnv)->CallStaticVoidMethod(dalvikEnv, pojav_environ->bridgeClazz, mid);
    if ((*dalvikEnv)->ExceptionCheck(dalvikEnv)) (*dalvikEnv)->ExceptionClear(dalvikEnv);
}

JNIEXPORT jboolean JNICALL
Java_org_lwjgl_glfw_CallbackBridge_nativeHookSdlWindows(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jclass clazz, jlong sdl_handle) {
    install_sdl_window_hooks((void *) (uintptr_t) sdl_handle);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_org_lwjgl_glfw_CallbackBridge_nativeRunSdlJniOnLoad(JNIEnv *env, __attribute__((unused)) jclass clazz, jlong address) {
    JavaVM *vm = NULL;
    (*env)->GetJavaVM(env, &vm);
    jni_onload_func_t onload = (jni_onload_func_t) (uintptr_t) address;
    if (vm == NULL || onload == NULL) return JNI_FALSE;
    jint version = onload(vm, NULL);
    return version != JNI_ERR ? JNI_TRUE : JNI_FALSE;
}

/**
 * Game-JVM side: forward to the Dalvik class so that JNI_OnLoad runs with an
 * app-classloader Java frame on the stack (its FindClass must see app classes).
 */
JNIEXPORT jboolean JNICALL
Java_org_lwjgl_glfw_CallbackBridge_nativeRunSdlJniOnLoadViaDalvik(JNIEnv *env, __attribute__((unused)) jclass clazz, jlong address) {
    JavaVM *dalvik = pojav_environ != NULL ? pojav_environ->dalvikJavaVMPtr : NULL;
    if (dalvik == NULL || pojav_environ->bridgeClazz == NULL) return JNI_FALSE;

    JNIEnv *dalvikEnv = NULL;
    if ((*dalvik)->GetEnv(dalvik, (void **) &dalvikEnv, JNI_VERSION_1_4) != JNI_OK) {
        if ((*dalvik)->AttachCurrentThread(dalvik, &dalvikEnv, NULL) != JNI_OK) return JNI_FALSE;
    }
    jmethodID mid = (*dalvikEnv)->GetStaticMethodID(
            dalvikEnv, pojav_environ->bridgeClazz, "runSdlJniOnLoad", "(J)Z");
    if (mid == NULL) {
        if ((*dalvikEnv)->ExceptionCheck(dalvikEnv)) (*dalvikEnv)->ExceptionClear(dalvikEnv);
        return JNI_FALSE;
    }
    jboolean ok = (*dalvikEnv)->CallStaticBooleanMethod(
            dalvikEnv, pojav_environ->bridgeClazz, mid, address);
    if ((*dalvikEnv)->ExceptionCheck(dalvikEnv)) {
        (*dalvikEnv)->ExceptionClear(dalvikEnv);
        ok = JNI_FALSE;
    }
    return ok;
}

static jmethodID method_sdlSymbol = NULL;

/**
 * One native for both VM copies of libpojavexec:
 *  - invoked on the Dalvik VM  -> dlsym the libSDL3 loaded in the app namespace;
 *  - invoked on the game JVM   -> forward to the Dalvik CallbackBridge.sdlSymbol,
 *    which runs this same function in the Dalvik copy.
 * This keeps exactly one libSDL3 instance (the one with SDL's Android JNI glue).
 */
JNIEXPORT jlong JNICALL
Java_org_lwjgl_glfw_CallbackBridge_nativeSdlSymbol(JNIEnv *env, __attribute__((unused)) jclass clazz, jstring name) {
    if (name == NULL) return 0;

    JavaVM *vm = NULL;
    (*env)->GetJavaVM(env, &vm);

    if (pojav_environ != NULL && pojav_environ->dalvikJavaVMPtr != NULL &&
            vm != pojav_environ->dalvikJavaVMPtr) {
        JNIEnv *dalvikEnv = NULL;
        if ((*pojav_environ->dalvikJavaVMPtr)->AttachCurrentThread(
                pojav_environ->dalvikJavaVMPtr, &dalvikEnv, NULL) != JNI_OK || dalvikEnv == NULL) {
            return 0;
        }

        jlong result = 0;
        const char *cname = (*env)->GetStringUTFChars(env, name, NULL);
        if (cname != NULL) {
            jstring dname = (*dalvikEnv)->NewStringUTF(dalvikEnv, cname);
            (*env)->ReleaseStringUTFChars(env, name, cname);
            if (dname != NULL) {
                if (method_sdlSymbol == NULL) {
                    method_sdlSymbol = (*dalvikEnv)->GetStaticMethodID(
                            dalvikEnv, pojav_environ->bridgeClazz, "sdlSymbol", "(Ljava/lang/String;)J");
                }
                if (method_sdlSymbol != NULL) {
                    result = (*dalvikEnv)->CallStaticLongMethod(
                            dalvikEnv, pojav_environ->bridgeClazz, method_sdlSymbol, dname);
                }
                (*dalvikEnv)->DeleteLocalRef(dalvikEnv, dname);
                if ((*dalvikEnv)->ExceptionCheck(dalvikEnv)) {
                    (*dalvikEnv)->ExceptionClear(dalvikEnv);
                }
            }
        }
        // Keep the thread attached; see the note in nativeNotifyLauncher.
        return result;
    }

    const char *cname = (*env)->GetStringUTFChars(env, name, NULL);
    void *handle = dlopen("libSDL3.so", RTLD_NOLOAD);
    void *sym = (handle != NULL && cname != NULL) ? dlsym(handle, cname) : NULL;
    if (cname != NULL) (*env)->ReleaseStringUTFChars(env, name, cname);
    return (jlong) (uintptr_t) sym;
}

/**
 * Phase 0 diagnostic: addresses of SDL_Init as seen from this (Dalvik/app)
 * namespace. The overlay logs its own value right before it calls the same
 * function; if the two differ the linker mapped a second libSDL3 copy and the
 * surface/JNI state would be split between them.
 */
JNIEXPORT jlong JNICALL
Java_org_lwjgl_glfw_CallbackBridge_nativeSdlInitAddress(__attribute__((unused)) JNIEnv *env, __attribute__((unused)) jclass clazz) {
    void *handle = dlopen("libSDL3.so", RTLD_NOLOAD);
    void *sym = handle != NULL ? dlsym(handle, "SDL_Init") : NULL;
    __android_log_print(ANDROID_LOG_INFO, "QCXSDL", "Dalvik SDL_Init=%p handle=%p", sym, handle);
    return (jlong) (uintptr_t) sym;
}

jboolean critical_send_char(jchar codepoint) {
    if (pojav_environ->GLFW_invoke_Char && pojav_environ->isInputReady) {
        if (pojav_environ->isUseStackQueueCall) {
            sendData(EVENT_TYPE_CHAR, codepoint, 0, 0, 0);
        } else {
            pojav_environ->GLFW_invoke_Char((void*) pojav_environ->showingWindow, (unsigned int) codepoint);
        }
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

jboolean noncritical_send_char(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jchar codepoint) {
    return critical_send_char(codepoint);
}

jboolean critical_send_char_mods(jchar codepoint, jint mods) {
    if (pojav_environ->GLFW_invoke_CharMods && pojav_environ->isInputReady) {
        if (pojav_environ->isUseStackQueueCall) {
            sendData(EVENT_TYPE_CHAR_MODS, (int) codepoint, mods, 0, 0);
        } else {
            pojav_environ->GLFW_invoke_CharMods((void*) pojav_environ->showingWindow, codepoint, mods);
        }
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

jboolean noncritical_send_char_mods(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jchar codepoint, jint mods) {
    return critical_send_char_mods(codepoint, mods);
}
/*
JNIEXPORT void JNICALL Java_org_lwjgl_glfw_CallbackBridge_nativeSendCursorEnter(JNIEnv* env, jclass clazz, jint entered) {
    if (pojav_environ->GLFW_invoke_CursorEnter && pojav_environ->isInputReady) {
        pojav_environ->GLFW_invoke_CursorEnter(pojav_environ->showingWindow, entered);
    }
}
*/

void critical_send_cursor_pos(jfloat x, jfloat y) {
#ifdef DEBUG
    LOGD("Sending cursor position \n");
#endif
    if (pojav_environ->GLFW_invoke_CursorPos && pojav_environ->isInputReady) {
#ifdef DEBUG
        LOGD("pojav_environ->GLFW_invoke_CursorPos && pojav_environ->isInputReady \n");
#endif
        if (!pojav_environ->isCursorEntered) {
            if (pojav_environ->GLFW_invoke_CursorEnter) {
                pojav_environ->isCursorEntered = true;
                if (pojav_environ->isUseStackQueueCall) {
                    sendData(EVENT_TYPE_CURSOR_ENTER, 1, 0, 0, 0);
                } else {
                    pojav_environ->GLFW_invoke_CursorEnter((void*) pojav_environ->showingWindow, 1);
                }
            } else if (pojav_environ->isGrabbing) {
                // Some Minecraft versions does not use GLFWCursorEnterCallback
                // This is a smart check, as Minecraft will not in grab mode if already not.
                pojav_environ->isCursorEntered = true;
            }
        }

        if (!pojav_environ->isUseStackQueueCall) {
            pojav_environ->GLFW_invoke_CursorPos((void*) pojav_environ->showingWindow, (double) (x), (double) (y));
        } else {
            pojav_environ->cursorX = x;
            pojav_environ->cursorY = y;
        }
    }
}

void noncritical_send_cursor_pos(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz,  jfloat x, jfloat y) {
    critical_send_cursor_pos(x, y);
}
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
void critical_send_key(jint key, jint scancode, jint action, jint mods) {
    if (pojav_environ->GLFW_invoke_Key && pojav_environ->isInputReady) {
        // 26.3 has no GLFW class, so the key/mouse buffers were never resolved;
        // writing through a NULL pointer would crash the SDL path.
        if (pojav_environ->keyDownBuffer) pojav_environ->keyDownBuffer[max(0, key-31)] = (jbyte) action;
        if (pojav_environ->isUseStackQueueCall) {
            sendData(EVENT_TYPE_KEY, key, scancode, action, mods);
        } else {
            pojav_environ->GLFW_invoke_Key((void*) pojav_environ->showingWindow, key, scancode, action, mods);
        }
    }
}
void noncritical_send_key(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jint key, jint scancode, jint action, jint mods) {
    critical_send_key(key, scancode, action, mods);
}

void critical_send_mouse_button(jint button, jint action, jint mods) {
    if (pojav_environ->GLFW_invoke_MouseButton && pojav_environ->isInputReady) {
        if (pojav_environ->mouseDownBuffer) pojav_environ->mouseDownBuffer[max(0, button)] = (jbyte) action;
        if (pojav_environ->isUseStackQueueCall) {
            sendData(EVENT_TYPE_MOUSE_BUTTON, button, action, mods, 0);
        } else {
            pojav_environ->GLFW_invoke_MouseButton((void*) pojav_environ->showingWindow, button, action, mods);
        }
    }
}

void noncritical_send_mouse_button(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jint button, jint action, jint mods) {
    critical_send_mouse_button(button, action, mods);
}

void critical_send_screen_size(jint width, jint height) {
    pojav_environ->savedWidth = width;
    pojav_environ->savedHeight = height;
    if (pojav_environ->isInputReady) {
        if (pojav_environ->GLFW_invoke_FramebufferSize) {
            if (pojav_environ->isUseStackQueueCall) {
                sendData(EVENT_TYPE_FRAMEBUFFER_SIZE, width, height, 0, 0);
            } else {
                pojav_environ->GLFW_invoke_FramebufferSize((void*) pojav_environ->showingWindow, width, height);
            }
        }

        if (pojav_environ->GLFW_invoke_WindowSize) {
            if (pojav_environ->isUseStackQueueCall) {
                sendData(EVENT_TYPE_WINDOW_SIZE, width, height, 0, 0);
            } else {
                pojav_environ->GLFW_invoke_WindowSize((void*) pojav_environ->showingWindow, width, height);
            }
        }
    }
}

void noncritical_send_screen_size(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jint width, jint height) {
    critical_send_screen_size(width, height);
}

void critical_send_scroll(jdouble xoffset, jdouble yoffset) {
    if (pojav_environ->GLFW_invoke_Scroll && pojav_environ->isInputReady) {
        if (pojav_environ->isUseStackQueueCall) {
            sendData(EVENT_TYPE_SCROLL, (int)xoffset, (int)yoffset, 0, 0);
        } else {
            pojav_environ->GLFW_invoke_Scroll((void*) pojav_environ->showingWindow, (double) xoffset, (double) yoffset);
        }
    }
}

void noncritical_send_scroll(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jdouble xoffset, jdouble yoffset) {
    critical_send_scroll(xoffset, yoffset);
}


JNIEXPORT void JNICALL Java_org_lwjgl_glfw_GLFW_nglfwSetShowingWindow(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jlong window) {
    pojav_environ->showingWindow = (long) window;
}

JNIEXPORT void JNICALL Java_org_lwjgl_glfw_CallbackBridge_nativeSetWindowAttrib(__attribute__((unused)) JNIEnv* env, __attribute__((unused)) jclass clazz, jint attrib, jint value) {
    if (!s_runtimeHasGlfw || !pojav_environ->showingWindow || !pojav_environ->isUseStackQueueCall) {
        // If the window is not shown, there is nothing to do yet.
        // For Minecraft < 1.13, calling to JNI functions here crashes the JVM for some reason, therefore it is skipped for now.
        return;
    }

    (*pojav_environ->runtimeJNIEnvPtr_JRE)->CallStaticVoidMethod(
            pojav_environ->runtimeJNIEnvPtr_JRE,
            pojav_environ->vmGlfwClass, pojav_environ->method_glftSetWindowAttrib,
            (jlong) pojav_environ->showingWindow, attrib, value
    );
}
const static JNINativeMethod critical_fcns[] = {
        {"nativeSetUseInputStackQueue", "(Z)V", critical_set_stackqueue},
        {"nativeSendChar", "(C)Z", critical_send_char},
        {"nativeSendCharMods", "(CI)Z", critical_send_char_mods},
        {"nativeSendKey", "(IIII)V", critical_send_key},
        {"nativeSendCursorPos", "(FF)V", critical_send_cursor_pos},
        {"nativeSendMouseButton", "(III)V", critical_send_mouse_button},
        {"nativeSendScroll", "(DD)V", critical_send_scroll},
        {"nativeSendScreenSize", "(II)V", critical_send_screen_size}
};

const static JNINativeMethod noncritical_fcns[] = {
        {"nativeSetUseInputStackQueue", "(Z)V", noncritical_set_stackqueue},
        {"nativeSendChar", "(C)Z", noncritical_send_char},
        {"nativeSendCharMods", "(CI)Z", noncritical_send_char_mods},
        {"nativeSendKey", "(IIII)V", noncritical_send_key},
        {"nativeSendCursorPos", "(FF)V", noncritical_send_cursor_pos},
        {"nativeSendMouseButton", "(III)V", noncritical_send_mouse_button},
        {"nativeSendScroll", "(DD)V", noncritical_send_scroll},
        {"nativeSendScreenSize", "(II)V", noncritical_send_screen_size}
};


static bool criticalNativeAvailable;

void dvm_testCriticalNative(void* arg0, void* arg1, void* arg2, void* arg3) {
    if(arg0 != 0 && arg2 == 0 && arg3 == 0) {
        criticalNativeAvailable = false;
    }else if (arg0 == 0 && arg1 == 0){
        criticalNativeAvailable = true;
    }else {
        criticalNativeAvailable = false; // just to be safe
    }
}

static bool tryCriticalNative(JNIEnv *env) {
    static const JNINativeMethod testJNIMethod[] = {
            { "testCriticalNative", "(II)V", dvm_testCriticalNative}
    };
    jclass criticalNativeTest = (*env)->FindClass(env, "pojlib/input/CriticalNativeTest");
    if(criticalNativeTest == NULL) {
        (*env)->ExceptionClear(env);
        return false;
    }
    jmethodID criticalNativeTestMethod = (*env)->GetStaticMethodID(env, criticalNativeTest, "invokeTest", "()V");
    (*env)->RegisterNatives(env, criticalNativeTest, testJNIMethod, 1);
    (*env)->CallStaticVoidMethod(env, criticalNativeTest, criticalNativeTestMethod);
    (*env)->UnregisterNatives(env, criticalNativeTest);
    return criticalNativeAvailable;
}

static void registerFunctions(JNIEnv *env) {
    bool use_critical_cc = tryCriticalNative(env);
    jclass bridge_class = (*env)->FindClass(env, "org/lwjgl/glfw/CallbackBridge");
    if (use_critical_cc) {
        __android_log_print(ANDROID_LOG_INFO, "pojavexec",
                            "CriticalNative is available. Enjoy the 4.6x times faster input!");
    } else {
        __android_log_print(ANDROID_LOG_INFO, "pojavexec",
                            "CriticalNative is not available. Upgrade, maybe?");
    }
    (*env)->RegisterNatives(env,
                            bridge_class,
                            use_critical_cc ? critical_fcns : noncritical_fcns,
                            sizeof(critical_fcns) / sizeof(critical_fcns[0]));
}