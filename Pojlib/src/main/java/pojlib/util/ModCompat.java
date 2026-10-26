package pojlib.util;

import android.app.Activity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.Locale;

import pojlib.util.json.MinecraftInstances;

/**
 * Device-level workarounds for the modpack.
 *
 * MobileGlues 2.0.0 translates Sodium 0.9's terrain shaders. MobileGlues 1.3.4 did not:
 * its glslang preamble lacked "#undef VULKAN", so Sodium's Vulkan-only
 * "layout(push_constant)" branch was parsed, translation failed, and the raw desktop
 * GLSL reached the Qualcomm compiler -> "Invalid #version" -> black terrain.
 *
 * Sodium therefore stays enabled. If MobileGlues still reports a shader failure after
 * launch, the inert stub is restored so the next start uses the vanilla terrain
 * pipeline. Creating "sodium_stub.txt" in the app home forces the stub immediately.
 */
public class ModCompat {
    private static final String SODIUM = "Sodium.jar";
    private static final long STUB_MAX_SIZE = 100 * 1024L;

    private static final String STUB_OVERRIDE = "sodium_stub.txt";
    private static final String MG_CONFIG_ASSET = "mobileglues-config.json";
    private static final String MG_CONFIG_MARKER = "\"launcherManaged\": 2";

    private static final long WATCHDOG_FIRST_CHECK_MS = 20_000L;
    private static final long WATCHDOG_TIMEOUT_MS = 4 * 60 * 1000L;

    public static void ensureLaunchableMods(Activity activity, MinecraftInstances.Instance instance) {
        if (instance == null) return;
        installMobileGluesConfig(activity);

        File sodium = new File(new File(instance.gameDir, "mods"), SODIUM);
        if (stubForced()) {
            disableSodium(activity, sodium, false);
            return;
        }

        if (!supportsSodium(instance.versionName)) {
            disableSodium(activity, sodium, false);
            return;
        }

        restoreSodium(sodium);
        watchForShaderFailures(activity, sodium);
    }

    /** Sodium 0.9+ is verified on 26.2+; older instances keep the stub used by earlier builds. */
    private static boolean supportsSodium(String versionName) {
        if (versionName == null) return false;
        String[] parts = versionName.split("\\.");
        if (parts.length < 2) return false;
        try {
            int major = Integer.parseInt(parts[0]);
            int minor = Integer.parseInt(parts[1]);
            return major > 26 || (major == 26 && minor >= 2);
        } catch (NumberFormatException e) {
            return false;
        }
    }

    private static boolean stubForced() {
        return Constants.USER_HOME != null && new File(Constants.USER_HOME, STUB_OVERRIDE).isFile();
    }

    private static void restoreSodium(File sodium) {
        if (!sodium.isFile() || sodium.length() >= STUB_MAX_SIZE) {
            return;
        }

        File backup = new File(sodium.getParentFile(), SODIUM + ".orig");
        try {
            if (backup.isFile()) {
                copyFile(backup, sodium);
                Logger.getInstance().appendToLog("ModCompat: restored Sodium from " + backup.getName()
                        + " (" + sodium.length() + " bytes)");
            } else {
                sodium.delete();
                Logger.getInstance().appendToLog("ModCompat: removed the Sodium stub; it will be "
                        + "downloaded again on the next online launch");
            }
        } catch (Exception e) {
            Logger.getInstance().appendToLog("ModCompat: could not restore Sodium: " + e);
        }
    }

    private static void disableSodium(Activity activity, File sodium, boolean persistOverride) {
        try {
            if (sodium.isFile() && sodium.length() >= STUB_MAX_SIZE) {
                File backup = new File(sodium.getParentFile(), SODIUM + ".orig");
                copyFile(sodium, backup);
                try (InputStream in = activity.getAssets().open("sodium-stub.jar")) {
                    copyStream(in, sodium);
                }
                Logger.getInstance().appendToLog("ModCompat: Sodium replaced by the inert stub; "
                        + "original kept as " + backup.getName());
            }
            if (persistOverride && Constants.USER_HOME != null) {
                File override = new File(Constants.USER_HOME, STUB_OVERRIDE);
                try (OutputStream out = new FileOutputStream(override)) {
                    out.write("Sodium was disabled after MobileGlues reported a shader failure.\n"
                            .getBytes(StandardCharsets.UTF_8));
                }
                Logger.getInstance().appendToLog("ModCompat: wrote " + override.getAbsolutePath()
                        + "; delete it to try Sodium again");
            }
        } catch (Exception e) {
            Logger.getInstance().appendToLog("ModCompat: could not disable Sodium: " + e);
        }
    }

    private static void installMobileGluesConfig(Activity activity) {
        File mgDir = new File(activity.getFilesDir(), "mg");
        File config = new File(mgDir, "config.json");
        try {
            if (config.isFile() && readText(config).contains(MG_CONFIG_MARKER)) {
                return;
            }
            if (!mgDir.isDirectory() && !mgDir.mkdirs()) {
                Logger.getInstance().appendToLog("ModCompat: could not create " + mgDir);
                return;
            }
            try (InputStream in = activity.getAssets().open(MG_CONFIG_ASSET)) {
                copyStream(in, config);
            }
            Logger.getInstance().appendToLog("ModCompat: installed MobileGlues config at " + config);
        } catch (Exception e) {
            Logger.getInstance().appendToLog("ModCompat: could not install MobileGlues config: " + e);
        }
    }

    private static void watchForShaderFailures(Activity activity, File sodium) {
        if (!sodium.isFile() || sodium.length() < STUB_MAX_SIZE) {
            return; // Sodium is not installed, nothing to watch
        }

        final File log = new File(new File(activity.getFilesDir(), "mg"), "latest.log");
        final long start = System.currentTimeMillis();
        Thread watchdog = new Thread(() -> {
            try {
                Thread.sleep(WATCHDOG_FIRST_CHECK_MS);
                while (System.currentTimeMillis() - start < WATCHDOG_TIMEOUT_MS) {
                    if (log.isFile() && log.lastModified() >= start && hasShaderFailure(log)) {
                        disableSodium(activity, sodium, true);
                        Logger.getInstance().appendToLog("ModCompat: MobileGlues reported a shader "
                                + "failure; restarted stubbing Sodium. Restart the instance to fall "
                                + "back to the vanilla terrain pipeline.");
                        return;
                    }
                    Thread.sleep(5_000L);
                }
            } catch (InterruptedException ignored) {
            }
        }, "sodium-shader-watchdog");
        watchdog.setDaemon(true);
        watchdog.start();
    }

    private static boolean hasShaderFailure(File log) {
        String text = readTail(log, 512 * 1024).toLowerCase(Locale.ROOT);
        return text.contains("failed to convert shader")
                || text.contains("failed to convert glsl")
                || text.contains("shader compilation failed")
                || text.contains("failed to link program")
                || text.contains("couldn't compile vertex shader")
                || text.contains("couldn't compile fragment shader")
                || text.contains("invalid #version");
    }

    private static String readText(File file) {
        try {
            return new String(java.nio.file.Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "";
        }
    }

    private static String readTail(File file, int maxBytes) {
        try (RandomAccessFile raf = new RandomAccessFile(file, "r")) {
            long length = raf.length();
            long skip = Math.max(0, length - maxBytes);
            byte[] buffer = new byte[(int) (length - skip)];
            raf.seek(skip);
            raf.readFully(buffer);
            return new String(buffer, StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "";
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
