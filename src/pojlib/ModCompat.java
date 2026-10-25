package pojlib.util;

import android.app.Activity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import pojlib.util.json.MinecraftInstances;

/**
 * Device-level workarounds for the modpack.
 *
 * Sodium 0.9's terrain shaders cannot be translated by MobileGlues 1.3.4 on Adreno
 * (the driver ends up with desktop GLSL and answers "Invalid #version"), which makes
 * all three terrain pipelines invalid -> blocks are invisible. Until MobileGlues can
 * handle them, the mod is replaced by an inert stub so the game falls back to its own
 * minecraft:pipeline terrain path. The real jar is kept as Sodium.jar.orig.
 */
public class ModCompat {
    private static final String SODIUM = "Sodium.jar";
    private static final long STUB_MAX_SIZE = 100 * 1024L;

    public static void ensureLaunchableMods(Activity activity, MinecraftInstances.Instance instance) {
        if (instance == null) return;
        disableSodium(activity, instance);
    }

    private static void disableSodium(Activity activity, MinecraftInstances.Instance instance) {
        File mods = new File(instance.gameDir, "mods");
        File sodium = new File(mods, SODIUM);
        if (!sodium.isFile() || sodium.length() < STUB_MAX_SIZE) {
            return; // absent, or already the stub
        }

        File backup = new File(mods, SODIUM + ".orig");
        try {
            if (!backup.isFile()) {
                copyFile(sodium, backup);
            }
            try (InputStream in = activity.getAssets().open("sodium-stub.jar")) {
                copyStream(in, sodium);
            }
            Logger.getInstance().appendToLog("ModCompat: Sodium replaced by the inert stub "
                    + "(MobileGlues cannot compile its terrain shaders); original kept as " + backup.getName());
        } catch (Exception e) {
            Logger.getInstance().appendToLog("ModCompat: could not disable Sodium: " + e);
        }
    }

    private static void copyFile(File from, File to) throws IOException {
        try (InputStream in = new java.io.FileInputStream(from)) {
            copyStream(in, to);
        }
    }

    private static void copyStream(InputStream in, File to) throws IOException {
        try (OutputStream out = new FileOutputStream(to)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
        }
    }
}
