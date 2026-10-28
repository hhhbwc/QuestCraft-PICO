package pojlib.install;

import com.google.gson.annotations.SerializedName;
import pojlib.APIHandler;
import pojlib.util.Constants;
import pojlib.util.GsonUtils;

import java.io.File;

public class MinecraftMeta {

    private static final APIHandler handler = new APIHandler(Constants.MOJANG_META_URL);

    public static class MinecraftVersions {
        @SerializedName("versions")
        public MinecraftVersion[] versions;
    }

    public static class MinecraftVersion {
        @SerializedName("id")
        public String id;
        @SerializedName("sha1")
        public String sha1;
    }

    public static MinecraftVersion[] getVersions() {
        return handler.get("mc/game/version_manifest_v2.json", MinecraftVersions.class).versions;
    }

    public static VersionInfo getVersionInfo(String versionName) {
        for (MinecraftVersion minecraftVersion : getVersions()) {
            if (minecraftVersion.id.equals(versionName)) {
                return handler.get(String.format("v1/packages/%s/%s.json", minecraftVersion.sha1, minecraftVersion.id), VersionInfo.class);
            }
        }
        return null;
    }

    /**
     * Version metadata never changes for a released version, and fetching it
     * meant two HTTP round-trips before every launch. Keep it on disk and fall
     * back to the copy when the network is down.
     */
    public static VersionInfo getVersionInfoCached(String versionName) {
        File cache = new File(Constants.USER_HOME, "meta/" + versionName + ".json");
        long now = System.currentTimeMillis();
        if (cache.isFile() && now - cache.lastModified() < 24L * 60 * 60 * 1000) {
            VersionInfo cached = GsonUtils.jsonFileToObject(cache.getAbsolutePath(), VersionInfo.class);
            if (cached != null) {
                return cached;
            }
        }
        VersionInfo fresh = null;
        try {
            fresh = getVersionInfo(versionName);
        } catch (Exception e) {
            e.printStackTrace();
        }
        if (fresh != null) {
            GsonUtils.objectToJsonFile(cache.getAbsolutePath(), fresh);
            return fresh;
        }
        if (cache.isFile()) {
            return GsonUtils.jsonFileToObject(cache.getAbsolutePath(), VersionInfo.class);
        }
        return null;
    }
}