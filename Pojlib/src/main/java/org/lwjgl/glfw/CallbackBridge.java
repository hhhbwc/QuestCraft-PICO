package org.lwjgl.glfw;

import android.app.Activity;
import android.content.*;
import android.view.Choreographer;
import android.util.Log;

import androidx.annotation.Nullable;

import java.util.ArrayList;

import dalvik.annotation.optimization.CriticalNative;
import org.libsdl.app.SDL;
import org.libsdl.app.SDLActivity;
import pojlib.UnityPlayerActivity;

import pojlib.UnityPlayerActivity;
import pojlib.input.GrabListener;
import pojlib.input.LwjglGlfwKeycode;
import pojlib.input.MinecraftGLSurface;
import pojlib.util.Logger;

public class CallbackBridge {
    private static boolean isGrabbing = false;
    private static final ArrayList<GrabListener> grabListeners = new ArrayList<>();

    public static final int CLIPBOARD_COPY = 2000;
    public static final int CLIPBOARD_PASTE = 2001;
    public static final int CLIPBOARD_OPEN = 2002;

    public static volatile int windowWidth, windowHeight;
    public static volatile int physicalWidth, physicalHeight;
    public static float mouseX, mouseY;
    public volatile static boolean holdingAlt, holdingCapslock, holdingCtrl,
            holdingNumlock, holdingShift;

    public static void putMouseEventWithCoords(int button, float x, float y) {
        putMouseEventWithCoords(button, true, x, y);
    }

    public static void putMouseEventWithCoords(int button, boolean isDown, float x, float y /* , int dz, long nanos */) {
        sendCursorPos(x, y);
        sendMouseKeycode(button, CallbackBridge.getCurrentMods(), isDown);
    }


    public static void sendCursorPos(float x, float y) {
        mouseX = x;
        mouseY = y;
        nativeSendCursorPos(mouseX, mouseY);
    }

    public static void sendKeycode(int keycode, char keychar, int scancode, int modifiers, boolean isDown) {
        // TODO CHECK: This may cause input issue, not receive input!
        if(keycode != 0)  nativeSendKey(keycode,scancode,isDown ? 1 : 0, modifiers);
        if(isDown && keychar != '\u0000') {
            nativeSendCharMods(keychar,modifiers);
            nativeSendChar(keychar);
        }
    }

    public static void sendChar(char keychar, int modifiers){
        nativeSendCharMods(keychar,modifiers);
        nativeSendChar(keychar);
    }

    public static void sendKeyPress(int keyCode, int modifiers, boolean status) {
        sendKeyPress(keyCode, 0, modifiers, status);
    }

    public static void sendKeyPress(int keyCode, int scancode, int modifiers, boolean status) {
        sendKeyPress(keyCode, '\u0000', scancode, modifiers, status);
    }

    public static void sendKeyPress(int keyCode, char keyChar, int scancode, int modifiers, boolean status) {
        CallbackBridge.sendKeycode(keyCode, keyChar, scancode, modifiers, status);
    }

    public static void sendKeyPress(int keyCode) {
        sendKeyPress(keyCode, CallbackBridge.getCurrentMods(), true);
        sendKeyPress(keyCode, CallbackBridge.getCurrentMods(), false);
    }

    public static void sendMouseButton(int button, boolean status) {
        CallbackBridge.sendMouseKeycode(button, CallbackBridge.getCurrentMods(), status);
    }

    public static void sendMouseKeycode(int button, int modifiers, boolean isDown) {
        // if (isGrabbing()) DEBUG_STRING.append("MouseGrabStrace: " + android.util.Log.getStackTraceString(new Throwable()) + "\n");
        nativeSendMouseButton(button, isDown ? 1 : 0, modifiers);
    }

    public static void sendMouseKeycode(int keycode) {
        sendMouseKeycode(keycode, CallbackBridge.getCurrentMods(), true);
        sendMouseKeycode(keycode, CallbackBridge.getCurrentMods(), false);
    }

    public static void sendScroll(double xoffset, double yoffset) {
        nativeSendScroll(xoffset, yoffset);
    }

    public static void sendUpdateWindowSize(int w, int h) {
        nativeSendScreenSize(w, h);
    }

    public static boolean isGrabbing() {
        // Avoid going through the JNI each time.
        return isGrabbing;
    }

    // Called from JRE side
    @SuppressWarnings("unused")
    public static @Nullable String accessAndroidClipboard(int type, String copy) {
        switch (type) {
            case CLIPBOARD_COPY:
                UnityPlayerActivity.GLOBAL_CLIPBOARD.setPrimaryClip(ClipData.newPlainText("Copy", copy));
                return null;

            case CLIPBOARD_PASTE:
                if (UnityPlayerActivity.GLOBAL_CLIPBOARD.hasPrimaryClip() && UnityPlayerActivity.GLOBAL_CLIPBOARD.getPrimaryClipDescription().hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)) {
                    return UnityPlayerActivity.GLOBAL_CLIPBOARD.getPrimaryClip().getItemAt(0).getText().toString();
                } else {
                    return "";
                }

            default: return null;
        }
    }


    // Notification types (mirrors Amethyst-Android's CallbackBridge)
    public static final int NOTIF_TYPE_SDL = 0;

    // Notification actions
    public static final int ACTION_INIT_LAUNCHER_INTEGRATION = 0;
    public static final int ACTION_SEND_TEXTBOX_RECT = 1;
    public static final int ACTION_INIT_CONTROLLER = 2;
    public static final int ACTION_DEINIT_CONTROLLER = 3;

    /**
     * Used for any sort of notification that needs to be given from the JRE side.
     * Reached through libpojavexec's Java_org_lwjgl_glfw_CallbackBridge_nativeNotifyLauncher.
     * For NOTIF_TYPE_SDL / ACTION_INIT_LAUNCHER_INTEGRATION this performs the Dalvik-side
     * SDL bring-up: System.loadLibrary("SDL3") + SDL.setupJNI() + sdlEnabled = true.
     * Must run on the Dalvik VM — the SDL android glue caches THIS VM, so if the game
     * JVM loaded the library first, every org.libsdl.app callback would run on HotSpot.
     */
    // Called from JRE side via jni
    @SuppressWarnings("unused")
    public static boolean notifyLauncher(int type, int... action) {
        switch (type) {
            case NOTIF_TYPE_SDL:
                if (action[0] == ACTION_INIT_LAUNCHER_INTEGRATION) {
                    try {
                        // SDL itself lives in the game JVM's linker namespace; its
                        // JNI_OnLoad was already invoked with THIS VM by nativeInitSdl,
                        // so the native methods below are registered on these classes.
                        SDL.setupJNI();
                        MinecraftGLSurface.sdlEnabled = true;
                        // Phase 0 diagnostic: the JRE side logs its own SDL_Init
                        // address too; both must match or two libSDL3 copies exist.
                        Log.i("QCXSDL", "Dalvik SDL_Init=" + Long.toHexString(nativeSdlInitAddress()));
                        if (SDLActivity.getSDLSurface() != null) {
                            // Notifies SDL of native surface res which is needed for proper input handling
                            SDLActivity.getSDLSurface().nativeResize(windowWidth, windowHeight);
                        }
                        Logger.getInstance().appendToLog("QuestCraft: SDL support enabled!");
                        return true;
                    } catch (Throwable t) {
                        Logger.getInstance().appendToLog("QuestCraft: failed to initialize SDL launcher-side integration! We will likely crash");
                        t.printStackTrace();
                    }
                }
                if (action[0] == ACTION_SEND_TEXTBOX_RECT) {
                    // TODO (post Phase 0): forward textbox rect for SDL IME support
                }
                if (action[0] == ACTION_INIT_CONTROLLER) {
                    // TODO (post Phase 0): SDL controller integration
                }
                if (action[0] == ACTION_DEINIT_CONTROLLER) {
                    // TODO (post Phase 0): SDL controller integration
                }
                break;
        }
        return false;
    }

    public static int getCurrentMods() {
        int currMods = 0;
        if (holdingAlt) {
            currMods |= LwjglGlfwKeycode.GLFW_MOD_ALT;
        } if (holdingCapslock) {
            currMods |= LwjglGlfwKeycode.GLFW_MOD_CAPS_LOCK;
        } if (holdingCtrl) {
            currMods |= LwjglGlfwKeycode.GLFW_MOD_CONTROL;
        } if (holdingNumlock) {
            currMods |= LwjglGlfwKeycode.GLFW_MOD_NUM_LOCK;
        } if (holdingShift) {
            currMods |= LwjglGlfwKeycode.GLFW_MOD_SHIFT;
        }
        return currMods;
    }

    public static void setModifiers(int keyCode, boolean isDown){
        switch (keyCode){
            case LwjglGlfwKeycode.GLFW_KEY_LEFT_SHIFT:
                CallbackBridge.holdingShift = isDown;
                return;

            case LwjglGlfwKeycode.GLFW_KEY_LEFT_CONTROL:
                CallbackBridge.holdingCtrl = isDown;
                return;

            case LwjglGlfwKeycode.GLFW_KEY_LEFT_ALT:
                CallbackBridge.holdingAlt = isDown;
                return;

            case LwjglGlfwKeycode.GLFW_KEY_CAPS_LOCK:
                CallbackBridge.holdingCapslock = isDown;
                return;

            case LwjglGlfwKeycode.GLFW_KEY_NUM_LOCK:
                CallbackBridge.holdingNumlock = isDown;
        }
    }

    //Called from JRE side
    @SuppressWarnings("unused")
    private static void onGrabStateChanged(final boolean grabbing) {
        isGrabbing = grabbing;
    }

    public static void restartUnitySession(Activity activity) {
        UnityPlayerActivity unity = (UnityPlayerActivity) activity;
        unity.reinitUnity();
    }

    public static void addGrabListener(GrabListener listener) {
        synchronized (grabListeners) {
            listener.onGrabState(isGrabbing);
            grabListeners.add(listener);
        }
    }
    public static void removeGrabListener(GrabListener listener) {
        synchronized (grabListeners) {
            grabListeners.remove(listener);
        }
    }

    @CriticalNative
    public static native void nativeSetUseInputStackQueue(boolean useInputStackQueue);

    @CriticalNative private static native boolean nativeSendChar(char codepoint);
    // GLFW: GLFWCharModsCallback deprecated, but is Minecraft still use?
    @CriticalNative private static native boolean nativeSendCharMods(char codepoint, int mods);
    @CriticalNative private static native void nativeSendKey(int key, int scancode, int action, int mods);
    // private static native void nativeSendCursorEnter(int entered);
    @CriticalNative private static native void nativeSendCursorPos(float x, float y);
    @CriticalNative private static native void nativeSendMouseButton(int button, int action, int mods);
    @CriticalNative private static native void nativeSendScroll(double xoffset, double yoffset);
    @CriticalNative private static native void nativeSendScreenSize(int width, int height);
    public static native void nativeSetWindowAttrib(int attrib, int value);
    static {
        System.loadLibrary("pojavexec");
    }

    /** Returns SDL_Init as resolved in the Dalvik namespace (Phase 0 diagnostic). */
    private static native long nativeSdlInitAddress();

    /**
     * Resolves a libSDL3 symbol in this (Dalvik) linker namespace. Called from the
     * game JVM through libpojavexec so that both VMs share ONE libSDL3 instance
     * (the one whose JNI_OnLoad ran on this VM and whose surface is bound).
     */
    public static long sdlSymbol(String name) {
        return nativeSdlSymbol(name);
    }

    private static native long nativeSdlSymbol(String name);

    /**
     * Runs SDL's JNI_OnLoad from a Java frame of THIS (app) classloader, so its
     * FindClass("org/libsdl/app/SDLActivity") resolves. The address comes from
     * the game JVM's libSDL3 instance (nativeInitSdl).
     */
    public static boolean runSdlJniOnLoad(long address) {
        return nativeRunSdlJniOnLoad(address);
    }

    private static native boolean nativeRunSdlJniOnLoad(long address);

    /** Called from the game JVM when the first frame has been presented. */
    public static void onGameReady() {
        pojlib.API.gameReady = true;
        pojlib.util.Logger.getInstance().appendToLog("QuestCraft: game reported ready (first frame)");
    }
}
