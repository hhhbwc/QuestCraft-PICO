package pojlib.lwjgl;

import java.util.function.Function;

/**
 * LWJGL 3.4.1 name mapper for the 26.3 (SDL) runtime.
 *
 * The APK ships the 3.4.1 natives under *341 names (liblwjgl341.so, ...) so they
 * can coexist with the unsuffixed 3.3.6 copies used by 26.2 and older. The mapper
 * is wired through -Dorg.lwjgl.system.bundledLibrary.nameMapper in
 * JREUtils.addLwjglCompatibilityArgs; without it LWJGL falls back to the identity
 * mapper and 26.3 binds 3.3.6 natives against 3.4.1 classes.
 *
 * Inputs are bare bundled module names ("lwjgl", "lwjgl_opengl", ...). Explicit
 * per-module libname properties (org.lwjgl.<module>.libname) never pass through
 * here; the trailing guards keep full filenames and already-suffixed names intact.
 */
public final class BundledLibraryNames341 implements Function<String, String> {
    @Override
    public String apply(String name) {
        if (name == null || name.endsWith(".so") || name.endsWith("341")) {
            return name;
        }
        return name + "341";
    }
}
