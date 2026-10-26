package pojlib.util;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonParser;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.List;
import java.util.UUID;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import pojlib.API;
import pojlib.util.json.MinecraftInstances;

/**
 * Local skins for offline profiles.
 *
 * The game picks a default skin for an unknown profile from 18 bundled textures
 * (DefaultPlayerSkin: index = floorMod(uuid.hashCode(), 18)), and there is no skin
 * server to ask. We therefore generate a small resource pack that replaces all 18
 * of them with the user's PNG, which makes the custom skin show up no matter which
 * slot the profile's UUID lands on.
 *
 * Skin files are read from <files>/skins/<username>.png, falling back to
 * <files>/skins/custom.png.
 */
public class OfflineSkin {
    private static final String PACK_NAME = "qcxr-offline-skin";
    private static final String PACK_ENTRY = "file/" + PACK_NAME + ".zip";

    private static final String[] DEFAULT_SKINS = {
            "entity/player/slim/alex", "entity/player/slim/ari", "entity/player/slim/efe",
            "entity/player/slim/kai", "entity/player/slim/makena", "entity/player/slim/noor",
            "entity/player/slim/steve", "entity/player/slim/sunny", "entity/player/slim/zuri",
            "entity/player/wide/alex", "entity/player/wide/ari", "entity/player/wide/efe",
            "entity/player/wide/kai", "entity/player/wide/makena", "entity/player/wide/noor",
            "entity/player/wide/steve", "entity/player/wide/sunny", "entity/player/wide/zuri"
    };
    private static final int SLIM_COUNT = 9;

    public static File skinsDir() {
        return new File(Constants.USER_HOME, "skins");
    }

    public static File findSkin(String username) {
        File dir = skinsDir();
        if (username != null) {
            File named = new File(dir, username + ".png");
            if (named.isFile() && named.length() > 0) {
                return named;
            }
        }
        File custom = new File(dir, "custom.png");
        return custom.isFile() && custom.length() > 0 ? custom : null;
    }

    public static int slot(String uuid) {
        try {
            return Math.floorMod(UUID.fromString(uuid).hashCode(), DEFAULT_SKINS.length);
        } catch (Exception e) {
            return DEFAULT_SKINS.length - 1;
        }
    }

    /** Mirror what the launcher UI needs to draw the same model the game will use. */
    public static void updateStatics(String username, String uuid, boolean offline) {
        if (!offline) {
            API.offlineSkinPath = null;
            API.offlineSkinSlim = false;
            return;
        }
        File skin = findSkin(username);
        API.offlineSkinPath = skin != null ? skin.getAbsolutePath() : null;
        API.offlineSkinSlim = uuid != null && slot(uuid) < SLIM_COUNT;
    }

    /** Called from prelaunch: (re)builds or removes the pack and keeps options.txt in sync. */
    public static void apply(MinecraftInstances.Instance instance, String username, String uuid, boolean offline) {
        if (instance == null) return;
        File pack = new File(new File(instance.gameDir, "resourcepacks"), PACK_NAME + ".zip");
        File skin = offline ? findSkin(username) : null;

        if (skin == null) {
            if (pack.exists() && pack.delete()) {
                Logger.getInstance().appendToLog("OfflineSkin: removed " + PACK_NAME + " (no skin file)");
            }
            setPackEnabled(instance, false);
            return;
        }

        try {
            writePack(pack, skin);
            setPackEnabled(instance, true);
            Logger.getInstance().appendToLog("OfflineSkin: applied " + skin.getName() + " (slot " + slot(uuid)
                    + (slot(uuid) < SLIM_COUNT ? ", slim)" : ", wide)"));
        } catch (Exception e) {
            Logger.getInstance().appendToLog("OfflineSkin: failed to build pack: " + e);
        }
    }

    private static void writePack(File pack, File skin) throws IOException {
        File parent = pack.getParentFile();
        if (parent != null) parent.mkdirs();

        byte[] png = Files.readAllBytes(skin.toPath());
        // 26.2 (resource format 88) requires min_format/max_format; a bare pack_format is
        // rejected, and supported_formats is deprecated from format 65 onwards.
        String meta = "{\n"
                + "  \"pack\": {\n"
                + "    \"pack_format\": 88,\n"
                + "    \"min_format\": 88,\n"
                + "    \"max_format\": 255,\n"
                + "    \"description\": \"Offline skin\"\n"
                + "  }\n"
                + "}\n";

        try (ZipOutputStream zip = new ZipOutputStream(new FileOutputStream(pack))) {
            zip.putNextEntry(new ZipEntry("pack.mcmeta"));
            zip.write(meta.getBytes(StandardCharsets.UTF_8));
            zip.closeEntry();

            for (String path : DEFAULT_SKINS) {
                zip.putNextEntry(new ZipEntry("assets/minecraft/textures/" + path + ".png"));
                zip.write(png);
                zip.closeEntry();
            }
        }
    }

    private static void setPackEnabled(MinecraftInstances.Instance instance, boolean enabled) {
        File options = new File(instance.gameDir, "options.txt");
        if (!options.isFile()) return;

        try {
            List<String> lines = Files.readAllLines(options.toPath(), StandardCharsets.UTF_8);
            boolean changed = false;
            Gson gson = new Gson();

            for (int i = 0; i < lines.size(); i++) {
                String line = lines.get(i);
                int sep = line.indexOf(':');
                if (sep < 0) continue;
                String key = line.substring(0, sep);

                if (key.equals("resourcePacks")) {
                    JsonArray packs = JsonParser.parseString(line.substring(sep + 1)).getAsJsonArray();
                    boolean present = false;
                    for (int j = 0; j < packs.size(); j++) {
                        if (PACK_ENTRY.equals(packs.get(j).getAsString())) { present = true; break; }
                    }
                    if (enabled && !present) {
                        packs.add(PACK_ENTRY);
                        lines.set(i, key + ":" + gson.toJson(packs));
                        changed = true;
                    } else if (!enabled && present) {
                        JsonArray kept = new JsonArray();
                        for (int j = 0; j < packs.size(); j++) {
                            if (!PACK_ENTRY.equals(packs.get(j).getAsString())) kept.add(packs.get(j));
                        }
                        lines.set(i, key + ":" + gson.toJson(kept));
                        changed = true;
                    }
                } else if (key.equals("incompatibleResourcePacks")) {
                    // never leave a stale "incompatible" marker for our pack
                    JsonArray packs = JsonParser.parseString(line.substring(sep + 1)).getAsJsonArray();
                    JsonArray kept = new JsonArray();
                    for (int j = 0; j < packs.size(); j++) {
                        if (!PACK_ENTRY.equals(packs.get(j).getAsString())) kept.add(packs.get(j));
                    }
                    if (kept.size() != packs.size()) {
                        lines.set(i, key + ":" + gson.toJson(kept));
                        changed = true;
                    }
                }
            }

            if (changed) {
                Files.write(options.toPath(), lines, StandardCharsets.UTF_8);
            }
        } catch (Exception e) {
            Logger.getInstance().appendToLog("OfflineSkin: options.txt update failed: " + e);
        }
    }
}
