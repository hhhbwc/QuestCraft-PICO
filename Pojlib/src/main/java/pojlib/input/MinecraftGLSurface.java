package pojlib.input;

/**
 * QuestCraft-PICO 26.3 SDL port — Phase 0.
 *
 * Minimal holder for the launcher-side "SDL is wired up" flag. The SDL glue
 * (org.libsdl.app, derived from Amethyst-Android) checks this flag on every
 * surface/key/mouse callback so that it stays inert for 26.2 and older
 * instances; it is set to true from
 * {@link org.lwjgl.glfw.CallbackBridge#notifyLauncher} once the Dalvik side
 * has loaded libSDL3.so and run SDL.setupJNI().
 *
 * Amethyst keeps the same flag on its (much larger) MinecraftGLSurface view
 * class; Phase 0 does not port the input mapping, so only the flag is needed.
 */
public final class MinecraftGLSurface {
    public static volatile boolean sdlEnabled = false;

    private MinecraftGLSurface() {}
}
