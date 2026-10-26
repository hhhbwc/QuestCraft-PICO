package pojlib.install;

import android.app.Activity;

import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import org.apache.commons.io.FileUtils;

import pojlib.APIHandler;
import pojlib.util.download.DownloadManager;
import pojlib.util.download.DownloadUtils;
import pojlib.util.json.MinecraftInstances;
import pojlib.util.*;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.StringJoiner;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

//This class reads data from a game version json and downloads its contents.
//This works for the base game as well as mod loaders
public class Installer {

    private static final int DEFAULT_JAVA_MAJOR = 22;
    private static final int JAVA_25_MAJOR = 25;
    private static final String DEFAULT_JRE_URL = "https://github.com/QuestCraftPlusPlus/android-openjdk-build-multiarch/releases/latest/download/JRE.zip";
    private static final String JAVA_25_ASSET = "runtimes/JRE-25.zip";
    private static final String JAVA_MAJOR_MARKER = ".questcraft-java-major";
    private static final String OSHI_ANDROID_SHIM = Constants.USER_HOME + "/hacks/OshiAndroidShim.jar";
    private static final String FABRIC_LOADER_26_2 = "0.19.3";
    private static final String FABRIC_LOADER_26_2_JAR = Constants.USER_HOME +
        "/libraries/net/fabricmc/fabric-loader/" + FABRIC_LOADER_26_2 +
        "/fabric-loader-" + FABRIC_LOADER_26_2 + ".jar";

    /**
     * 26.3-only per-instance LWJGL 3.4.1 runtime (Java jars only; the Bionic
     * natives ship in the APK's jniLibs under *341 names because the game JVM
     * may only dlopen files that live in nativeLibraryDir).
     * 26.2 and older keep the merged GLFW 3.3.x classes; these assets are never
     * installed for them.
     */
    private static final String LWJGL3_SDL_ASSET_DIR = "components/lwjgl3/3.4.1";
    private static final String LWJGL3_SDL_VERSION = "3.4.1";
    private static final String LWJGL3_SDL_LEGACY_DIR = "lwjgl3/3.4.3";
    // The overlay must be the first entry so its patched SDLInit/SDLMouse and the
    // bundled-library name mapper shadow/load before the stock 3.4.1 classes.
    private static final String[] LWJGL3_SDL_JARS = {
        "lwjgl-sdl-overlay-3.4.1.jar",
        "lwjgl-3.4.1.jar",
        "lwjgl-opengl-3.4.1.jar",
        "lwjgl-openal-3.4.1.jar",
        "lwjgl-stb-3.4.1.jar",
        "lwjgl-freetype-3.4.1.jar",
        "lwjgl-shaderc-3.4.1.jar",
        "lwjgl-spvc-3.4.1.jar",
        "lwjgl-vma-3.4.1.jar",
        "lwjgl-sdl-3.4.1.jar",
        "lwjgl-vulkan-3.4.1.jar"
    };

    /**
     * True when the instance uses the LWJGL 3.4.1 + SDL stack. Exact match for now;
     * extend here when a later 26.x ships with SDL too. Everything SDL-related in
     * Installer/JREUtils/UnityPlayerActivity is gated on this, so 26.2 and older
     * keep their GLFW/LWJGL 3.3.x behavior untouched.
     */
    public static boolean isSdlVersion(String versionName) {
        return "26.3".equals(versionName);
    }

    public static void installJVM(Activity activity, boolean force) {
        installJVM(activity, force, DEFAULT_JAVA_MAJOR);
    }

    public static void installJVM(Activity activity, boolean force, VersionInfo versionInfo) {
        installJVM(activity, force, getRequiredJavaMajor(versionInfo));
    }

    private static void installJVM(Activity activity, boolean force, int requiredJavaMajor) {
        Logger.getInstance().appendToLog("Checking JRE");
        File jre = new File(activity.getFilesDir(), "runtimes/JRE");
        File marker = new File(jre, JAVA_MAJOR_MARKER);

        try {
            if (!jre.exists() || force || !isInstalledJavaMajor(marker, requiredJavaMajor)) {
                Logger.getInstance().appendToLog("Installing Java " + requiredJavaMajor + " JRE");
                File runtimeRoot = new File(activity.getFilesDir(), "runtimes");
                FileUtil.ensureDirectory(runtimeRoot);

                if (jre.exists()) {
                    if (jre.isDirectory()) {
                        FileUtils.deleteDirectory(jre);
                    } else {
                        FileUtils.deleteQuietly(jre);
                    }
                }

                File jreZip = new File(runtimeRoot, requiredJavaMajor >= JAVA_25_MAJOR ? "JRE-25.zip" : "JRE.zip");
                if (requiredJavaMajor >= JAVA_25_MAJOR) {
                    byte[] java25Runtime = FileUtil.loadFromAssetToByte(activity, JAVA_25_ASSET);
                    if (java25Runtime == null) {
                        Logger.getInstance().appendToLog("Bundled Java 25 runtime is missing from APK assets!");
                        return;
                    }
                    FileUtils.writeByteArrayToFile(jreZip, java25Runtime);
                } else {
                    DownloadUtils.downloadFile(DEFAULT_JRE_URL, jreZip);
                }

                DownloadManager.reset();
                FileUtil.unzipArchive(jreZip.getPath(), jre.getPath());
                Files.copy(
                        Paths.get(activity.getApplicationInfo().nativeLibraryDir + "/libawt_xawt.so"),
                        Paths.get(activity.getFilesDir() + "/runtimes/JRE/lib/libawt_xawt.so"),
                        StandardCopyOption.REPLACE_EXISTING
                );
                FileUtils.writeStringToFile(marker, String.valueOf(requiredJavaMajor), StandardCharsets.UTF_8);
                jreZip.delete();
            }

            Logger.getInstance().appendToLog("JRE installed");
        } catch (IOException e) {
            Logger.getInstance().appendToLog("Failed to install JRE: " + e.getMessage());
            e.printStackTrace();
        }
    }

    private static int getRequiredJavaMajor(VersionInfo versionInfo) {
        if (versionInfo != null && versionInfo.javaVersion != null && versionInfo.javaVersion.majorVersion >= JAVA_25_MAJOR) {
            return JAVA_25_MAJOR;
        }
        return DEFAULT_JAVA_MAJOR;
    }

    private static boolean isInstalledJavaMajor(File marker, int requiredJavaMajor) {
        try {
            return marker.exists() && FileUtils.readFileToString(marker, StandardCharsets.UTF_8).trim().equals(String.valueOf(requiredJavaMajor));
        } catch (IOException e) {
            return false;
        }
    }

    // Will only download client if it is missing, however it will overwrite if sha1 does not match the downloaded client
    // Returns client classpath
    public static CompletableFuture<String> installClient(VersionInfo minecraftVersionInfo, String gameDir) throws IOException {
        return CompletableFuture.supplyAsync(() -> {
            Logger.getInstance().appendToLog("Checking Client");

            File clientFile = new File(gameDir + "/versions/" + minecraftVersionInfo.id + "/client.jar");

            try {
                for (int i = 0; i < 5; i++) {
                    if (i == 4)
                        throw new RuntimeException("Client download failed after 5 retries");

                    // Re-download when missing OR mismatched (the previous check
                    // re-downloaded the valid file and then kept a corrupt one).
                    if (!clientFile.exists() || !DownloadUtils.compareSHA1(clientFile, minecraftVersionInfo.downloads.client.sha1)) {
                        clientFile.delete();
                        DownloadUtils.downloadFile(minecraftVersionInfo.downloads.client.url, clientFile);
                    }

                    // Check if the downloaded client matches the expected SHA1 hash
                    if (DownloadUtils.compareSHA1(clientFile, minecraftVersionInfo.downloads.client.sha1)) {
                        Logger.getInstance().appendToLog("Client downloaded");
                        return clientFile.getAbsolutePath();
                    }
                }
            } catch (IOException e) {
                Logger.getInstance().appendToLog("Failed to download client: " + e.getMessage());
                e.printStackTrace();
            }
            return null;
        });
    }

    // Will only download library if it is missing, however it will overwrite if sha1 does not match the downloaded library
    // Returns the classpath of the downloaded libraries
    public static CompletableFuture<String> installLibraries(VersionInfo versionInfo, String gameDir) throws IOException {
        return CompletableFuture.supplyAsync(() -> {
            Logger.getInstance().appendToLog("Checking Libraries for: " + versionInfo.id);
            StringJoiner classpath = new StringJoiner(File.pathSeparator);
            classpath.add(OSHI_ANDROID_SHIM);
            // LWJGL always comes from our own runtime: the merged 3.3.x GLFW classes
            // for 26.2 and older, the per-instance 3.4.1 jars for 26.3. Letting the
            // version JSON download its lwjgl jars would put 3.4.3 classes behind ours.
            final boolean sdlVersion = isSdlVersion(versionInfo.id);

            for (VersionInfo.Library library : versionInfo.libraries) {
                if (library.name.contains("lwjgl") || (library.name.contains("org.ow2.asm")) & !versionInfo.id.contains("fabric")) {
                    continue;
                }
                File sha1CacheFile = null;
                for (int i = 0; i < 5; i++) {
                    if (i == 4)
                        throw new RuntimeException(String.format("Library download of %s failed after 5 retries", library.name));

                    File libraryFile;
                    String sha1;

                    //Null means mod lib, otherwise vanilla lib
                    try {
                        if (library.downloads == null) {
                            String path = parseLibraryNameToPath(library.name);
                            libraryFile = new File(gameDir + "/libraries/", path);
                            // Skip the .sha1 network round-trip for libraries that were
                            // already verified on a previous launch: the cached hash is only
                            // trusted if the local file actually hashes to it, so a changed
                            // or corrupted jar still falls through to the download path.
                            File sha1Cache = new File(gameDir + "/libraries/", path + ".qcsha1");
                            if (libraryFile.isFile() && sha1Cache.isFile() && sha1Cache.length() == 40) {
                                String cachedSha1 = FileUtils.readFileToString(sha1Cache, StandardCharsets.UTF_8).trim();
                                if (DownloadUtils.compareSHA1(libraryFile, cachedSha1)) {
                                    classpath.add(libraryFile.getAbsolutePath());
                                    break;
                                }
                                sha1Cache.delete();
                            }
                            sha1CacheFile = sha1Cache;
                            sha1 = APIHandler.getRaw(library.url + path + ".sha1");
                            if (!libraryFile.exists()) {
                                Logger.getInstance().appendToLog("Downloading: " + library.name);
                                DownloadUtils.downloadFile(library.url + path, libraryFile);
                            }
                        } else {
                            VersionInfo.Library.Artifact artifact = library.downloads.artifact;
                            libraryFile = new File(gameDir + "/libraries/", artifact.path);
                            sha1 = artifact.sha1;
                            if (!libraryFile.exists()) {
                                Logger.getInstance().appendToLog("Downloading: " + library.name);
                                DownloadUtils.downloadFile(artifact.url, libraryFile, artifact.size);
                            }
                        }
                        if (DownloadUtils.compareSHA1(libraryFile, sha1)) {
                            if (sha1CacheFile != null && sha1 != null) {
                                try {
                                    FileUtils.writeStringToFile(sha1CacheFile, sha1, StandardCharsets.UTF_8);
                                } catch (IOException e) {
                                    e.printStackTrace();
                                }
                            }
                            classpath.add(libraryFile.getAbsolutePath());
                            break;
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }

            // Add our GLFW (26.2 and older only; 26.3 gets the per-instance 3.4.1 jars
            // via ensureSdlClasspath() instead)
            if (!sdlVersion) {
                classpath.add(Constants.USER_HOME + "/lwjgl3/lwjgl-glfw-classes.jar");
            }
            // DNS SRV Resolver fix
            classpath.add(Constants.USER_HOME + "/hacks/ResConfHack.jar");

            Logger.getInstance().appendToLog("Libraries installed");
            return classpath.toString();
        });
    }

    //Only works on minecraft, not fabric, quilt, etc...
    //Will only download asset if it is missing
    public static CompletableFuture<String> installAssets(VersionInfo minecraftVersionInfo, String gameDir) throws IOException {
        return CompletableFuture.supplyAsync(() -> {
            Logger.getInstance().appendToLog("Checking assets");
            // Fast path: once the index json exists, every asset was already
            // materialized by a previous launch (the sweep below only checks
            // existence for present files), so skip the index download and the
            // thousands of queued existence checks entirely.
            File assetIndexJson = new File(gameDir + "/assets/indexes/" + minecraftVersionInfo.assets + ".json");
            if (assetIndexJson.isFile()) {
                Logger.getInstance().appendToLog("Assets already present");
                return new File(gameDir + "/assets").getAbsolutePath();
            }
            JsonObject assets = APIHandler.getFullUrl(minecraftVersionInfo.assetIndex.url, JsonObject.class);

            for (Map.Entry<String, JsonElement> entry : assets.getAsJsonObject("objects").entrySet()) {
                VersionInfo.Asset asset = new Gson().fromJson(entry.getValue(), VersionInfo.Asset.class);
                DownloadManager.addTotalBytes(asset.size);
            }

            ThreadPoolExecutor tp = new ThreadPoolExecutor(8, 8, 100, TimeUnit.MILLISECONDS, new LinkedBlockingQueue<>());

            for (Map.Entry<String, JsonElement> entry : assets.getAsJsonObject("objects").entrySet()) {
                AsyncDownload thread = new AsyncDownload(entry, gameDir);
                tp.execute(thread);
            }

            tp.shutdown();
            try {
                while (!tp.awaitTermination(100, TimeUnit.MILLISECONDS)) ;
            } catch (InterruptedException e) {
                Logger.getInstance().appendToLog("Download thread interrupted" + e.getMessage());
            }



            return new File(gameDir + "/assets").getAbsolutePath();
        });
    }

    public static void moveLocalAssets(Activity activity, MinecraftInstances.Instance instance) throws IOException {
        try {
            // Configs the player edits in-game are seeded once; re-installing or
            // repairing an instance must not reset them.
            copyAssetIfMissing(activity, "sodium-options.json", new File(instance.gameDir, "config/sodium-options.json"));
            copyAssetIfMissing(activity, "sodium-mixins.properties", new File(instance.gameDir, "config/sodium-mixins.properties"));
            copyAssetIfMissing(activity, "smoothboot.json", new File(instance.gameDir, "config/smoothboot.json"));
            copyAssetIfMissing(activity, "immediatelyfast.json", new File(instance.gameDir, "config/immediatelyfast.json"));
            copyAssetIfMissing(activity, "moreculling.toml", new File(instance.gameDir, "config/moreculling.toml"));
            copyAssetIfMissing(activity, "modernfix-mixins.properties", new File(instance.gameDir, "config/modernfix-mixins.properties"));
            copyAssetIfMissing(activity, "options.txt", new File(instance.gameDir, "options.txt"));
            copyAssetIfMissing(activity, "servers.dat", new File(instance.gameDir, "servers.dat"));
            copyAssetIfMissing(activity, "vivecraft-client-config.json", new File(instance.gameDir, "config/vivecraft-client-config.json"));

            FileUtils.writeByteArrayToFile(new File(Constants.USER_HOME + "/hacks/ResConfHack.jar"), FileUtil.loadFromAssetToByte(activity, "hacks/ResConfHack.jar"));
            FileUtils.writeByteArrayToFile(new File(Constants.USER_HOME + "/hacks/OshiAndroidShim.jar"), FileUtil.loadFromAssetToByte(activity, "hacks/OshiAndroidShim.jar"));
            FileUtils.writeByteArrayToFile(new File(Constants.USER_HOME + "/hacks/resolv.conf"), FileUtil.loadFromAssetToByte(activity, "hacks/resolv.conf"));
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public static void refreshLaunchCompatibilityAssets(Activity activity, MinecraftInstances.Instance instance) {
        try {
            // options.txt holds the language, graphics backend and every video/audio
            // setting; Sodium's config is edited from its own screen. Rewriting them
            // on every launch was resetting all of that. Only seed what is missing.
            copyAssetIfMissing(activity, "options.txt", new File(instance.gameDir, "options.txt"));
            if ("26.2".equals(instance.versionName)) {
                copyAssetIfMissing(activity, "sodium-options.json", new File(instance.gameDir, "config/sodium-options.json"));
                copyAssetIfMissing(activity, "sodium-mixins.properties", new File(instance.gameDir, "config/sodium-mixins.properties"));
            }
            FileUtils.writeByteArrayToFile(new File(Constants.USER_HOME + "/hacks/OshiAndroidShim.jar"), FileUtil.loadFromAssetToByte(activity, "hacks/OshiAndroidShim.jar"));
        } catch (IOException e) {
            e.printStackTrace();
        }

        ensureClasspathEntryFirst(instance, OSHI_ANDROID_SHIM);
        if (isSdlVersion(instance.versionName)) {
            // 26.3: per-instance LWJGL 3.4.1 jars + SDL overlay;
            // the overlay and the jars are prepended to THIS instance's classpath only.
            try {
                installSdlLwjglRuntime(activity, instance);
                ensureSdlClasspath(instance);
            } catch (IOException e) {
                Logger.getInstance().appendToLog("Failed to install LWJGL 3.4.1 SDL runtime: " + e.getMessage());
                e.printStackTrace();
            }
        }
        if ("26.2".equals(instance.versionName)) {
            refreshFabricLoaderForCurrentLaunch(activity, instance);
        }
    }

    /** Copies the 26.3 LWJGL 3.4.1 jars to USER_HOME/lwjgl3/3.4.1/. */
    public static void installSdlLwjglRuntime(Activity activity, String versionName) throws IOException {
        if (!isSdlVersion(versionName)) {
            return;
        }
        File baseDir = new File(Constants.APP_FILES_DIR, "lwjgl3/" + LWJGL3_SDL_VERSION);
        for (String jar : LWJGL3_SDL_JARS) {
            installSdlAsset(activity, LWJGL3_SDL_ASSET_DIR + "/" + jar, new File(baseDir, jar));
        }
        installSdlAsset(activity, LWJGL3_SDL_ASSET_DIR + "/version", new File(baseDir, "version"));
        // Earlier 26.3 test builds copied the natives into app-private storage. The
        // game JVM's linker namespace refuses to dlopen those paths, so the natives
        // now live in the APK and this directory is dead weight.
        deleteRunsDir(new File(Constants.APP_FILES_DIR, LWJGL3_SDL_LEGACY_DIR));
        deleteRunsDir(new File(baseDir, "natives"));
        Logger.getInstance().appendToLog("LWJGL 3.4.1 SDL runtime installed");
    }

    private static void deleteRunsDir(File dir) {
        if (!dir.isDirectory()) {
            return;
        }
        File[] children = dir.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRunsDir(child);
            }
        }
        if (!dir.delete()) {
            Logger.getInstance().appendToLog("Could not remove stale LWJGL dir: " + dir);
        }
    }

    public static void installSdlLwjglRuntime(Activity activity, MinecraftInstances.Instance instance) throws IOException {
        installSdlLwjglRuntime(activity, instance.versionName);
    }

    private static void installSdlAsset(Activity activity, String assetPath, File target) throws IOException {
        byte[] bytes = FileUtil.loadFromAssetToByte(activity, assetPath);
        if (bytes == null) {
            // Missing natives are not fatal here: libSDL3.so lives in the APK's global
            // jniLibs and org.lwjgl.librarypath falls back to nativeLibraryDir.
            Logger.getInstance().appendToLog("Missing LWJGL 3.4.1 asset (skipped): " + assetPath);
            return;
        }
        if (target.isFile() && Arrays.equals(bytes, Files.readAllBytes(target.toPath()))) {
            return;
        }
        File parent = target.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }
        FileUtils.writeByteArrayToFile(target, bytes);
    }

    /** Prepends the 3.4.1 jars (overlay first) to this instance's classpath only. */
    public static void ensureSdlClasspath(MinecraftInstances.Instance instance) {
        if (!isSdlVersion(instance.versionName)) {
            return;
        }
        File baseDir = new File(Constants.APP_FILES_DIR, "lwjgl3/" + LWJGL3_SDL_VERSION);
        StringJoiner joiner = new StringJoiner(File.pathSeparator);
        for (String jar : LWJGL3_SDL_JARS) {
            joiner.add(new File(baseDir, jar).getAbsolutePath());
        }
        ensureClasspathEntriesFirst(instance, joiner.toString());
    }

    private static void copyAssetIfMissing(Activity activity, String asset, File target) throws IOException {
        if (target.isFile() && target.length() > 0) {
            return;
        }
        File parent = target.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }
        FileUtils.writeByteArrayToFile(target, FileUtil.loadFromAssetToByte(activity, asset));
    }

    private static void refreshFabricLoaderForCurrentLaunch(Activity activity, MinecraftInstances.Instance instance) {
        File installedLoader = new File(FABRIC_LOADER_26_2_JAR);
        if(installedLoader.isFile()) {
            ensureClasspathEntryFirst(instance, installedLoader.getAbsolutePath());
        }

        try {
            FabricMeta.FabricVersion fabricVersion = FabricMeta.getVersion(FABRIC_LOADER_26_2);
            if(fabricVersion == null) {
                return;
            }

            VersionInfo fabricVersionInfo = FabricMeta.getVersionInfo(fabricVersion, instance.versionName);
            if(fabricVersionInfo == null) {
                return;
            }

            instance.mainClass = fabricVersionInfo.mainClass;
            String fabricClasspath = installLibraries(fabricVersionInfo, Constants.USER_HOME).get();
            ensureClasspathEntriesFirst(instance, fabricClasspath);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void ensureClasspathEntriesFirst(MinecraftInstances.Instance instance, String classpath) {
        if(classpath == null || classpath.isEmpty()) {
            return;
        }

        String[] entries = classpath.split(java.util.regex.Pattern.quote(File.pathSeparator));
        for(int i = entries.length - 1; i >= 0; i--) {
            ensureClasspathEntryFirst(instance, entries[i]);
        }
    }

    private static void ensureClasspathEntryFirst(MinecraftInstances.Instance instance, String entry) {
        if(entry == null || entry.isEmpty()) {
            return;
        }

        if(instance.classpath == null || instance.classpath.isEmpty()) {
            instance.classpath = entry;
            return;
        }

        String[] entries = instance.classpath.split(java.util.regex.Pattern.quote(File.pathSeparator));
        List<String> repaired = new ArrayList<>();
        repaired.add(entry);
        for(String current : entries) {
            if(current == null || current.isEmpty() || entry.equals(current) || replacesClasspathEntry(entry, current)) {
                continue;
            }
            repaired.add(current);
        }
        instance.classpath = String.join(File.pathSeparator, repaired);
    }

    private static boolean replacesClasspathEntry(String entry, String current) {
        if(isFabricLoaderJar(entry) && isFabricLoaderJar(current)) {
            return true;
        }

        String entryArtifact = mavenArtifactKey(entry);
        String currentArtifact = mavenArtifactKey(current);
        return entryArtifact != null && entryArtifact.equals(currentArtifact);
    }

    private static String mavenArtifactKey(String path) {
        if(path == null || path.isEmpty()) {
            return null;
        }

        String normalized = path.replace('\\', '/');
        String marker = "/libraries/";
        int markerIndex = normalized.lastIndexOf(marker);
        if(markerIndex < 0) {
            return null;
        }

        String[] parts = normalized.substring(markerIndex + marker.length()).split("/");
        if(parts.length < 4) {
            return null;
        }

        StringBuilder key = new StringBuilder();
        for(int i = 0; i < parts.length - 2; i++) {
            if(i > 0) {
                key.append('/');
            }
            key.append(parts[i]);
        }
        return key.toString().toLowerCase(java.util.Locale.ROOT);
    }

    private static boolean isFabricLoaderJar(String path) {
        if(path == null || path.isEmpty()) {
            return false;
        }

        String fileName = new File(path).getName().toLowerCase();
        return fileName.startsWith("fabric-loader-") && fileName.endsWith(".jar");
    }

    public static class AsyncDownload implements Runnable {
        private final Map.Entry<String, JsonElement> entry;
        private final String gameDir;
        private final String fileName;

        public AsyncDownload(Map.Entry<String, JsonElement> entry, String gameDir) {
            this.entry = entry;
            this.gameDir = gameDir;
            this.fileName = entry.getKey();
        }

        @Override
        public void run() {
            VersionInfo.Asset asset = new Gson().fromJson(entry.getValue(), VersionInfo.Asset.class);
            String path = asset.hash.substring(0, 2) + "/" + asset.hash;
            File assetFile = new File(gameDir + "/assets/objects/", path);

            for (int i = 0; i < 5; i++) {
                if (i == 4) throw new RuntimeException(String.format("Asset download of %s failed after 5 retries", fileName));

                if (!assetFile.exists()) {
                    Logger.getInstance().appendToLog("Downloading: " + fileName);
                    try {
                        DownloadUtils.downloadFile(Constants.MOJANG_RESOURCES_URL + "/" + path, assetFile, 0);
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                }

                if (DownloadUtils.compareSHA1(assetFile, asset.hash)) {
                    break;
                } else {
                    assetFile.delete();
                }
            }
        }
    }


    //Used for mod libraries, vanilla is handled a different (tbh better) way
    private static String parseLibraryNameToPath(String libraryName) {
        String[] parts = libraryName.split(":");
        String location = parts[0].replace(".", "/");
        String name = parts[1];
        String version = parts[2];

        return String.format("%s/%s/%s/%s", location, name, version, name + "-" + version + ".jar");
    }
}
