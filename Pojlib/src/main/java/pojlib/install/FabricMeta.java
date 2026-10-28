package pojlib.install;

import com.google.gson.annotations.SerializedName;
import pojlib.APIHandler;
import pojlib.util.Constants;
import pojlib.util.GsonUtils;
import pojlib.util.Version;

import java.io.File;

public class FabricMeta {

    private static final APIHandler handler = new APIHandler(Constants.FABRIC_META_URL);

    public static class FabricVersion {
        @SerializedName("version")
        public String version;
        @SerializedName("stable")
        public boolean stable;
        @SerializedName("separator")
        public String separator;
    }

    public static FabricVersion[] getVersions() {
        return handler.get("versions/loader", FabricVersion[].class);
    }

    public static FabricVersion getVersion(String requestedVersion) {
        FabricVersion[] versions = getVersions();
        if(versions == null) {
            return null;
        }

        for(FabricVersion version : versions) {
            if(version != null && requestedVersion.equals(version.version)) {
                return version;
            }
        }
        return null;
    }

    private static Version getVersionFromFabric(FabricVersion fabric) {
        if(fabric == null || fabric.separator == null || fabric.separator.contains("+")) {
            // Only used pre-0.11, no use for us
            return null;
        }

        String[] verName = fabric.version.split("\\.");
        if(verName.length < 3) {
            return null;
        }
        int major;
        int minor;
        int patch;
        try {
            major = Integer.parseInt(verName[0]);
            minor = Integer.parseInt(verName[1]);
            patch = Integer.parseInt(verName[2]);
        } catch (NumberFormatException e) {
            return null;
        }

        return new Version(major, minor, patch);
    }

    private static boolean isNewer(Version candidate, Version latest) {
        if(candidate == null || latest == null) {
            return candidate != null;
        }

        if(candidate.major != latest.major) {
            return candidate.major > latest.major;
        }
        if(candidate.minor != latest.minor) {
            return candidate.minor > latest.minor;
        }
        return candidate.patch > latest.patch;
    }

    public static FabricVersion getLatestVersion() {
        FabricVersion latest = null;
        boolean latestIsStable = false;
        for (FabricVersion version : getVersions()) {
            if(version == null) {
                continue;
            }

            if(latest == null) {
                latest = version;
                latestIsStable = version.stable;
                continue;
            }

            Version newVer = getVersionFromFabric(version);
            Version latestVer = getVersionFromFabric(latest);

            if(newVer == null)
                continue;

            if(version.stable && !latestIsStable) {
                latest = version;
                latestIsStable = true;
                continue;
            }

            if(!version.stable && latestIsStable) {
                continue;
            }

            if(isNewer(newVer, latestVer)) {
                latest = version;
                latestIsStable = version.stable;
            }
        }
        return latest;
    }

    public static VersionInfo getVersionInfo(FabricVersion fabricVersion, String minecraftVersion) {
        return handler.get(String.format("versions/loader/%s/%s/profile/json", minecraftVersion, fabricVersion.version), VersionInfo.class);
    }

    /**
     * Launcher-side profile lookup that survives offline starts. Two network
     * round-trips before every 26.2 launch were costing seconds on bad Wi-Fi, so
     * a fresh profile is reused for a day and the cached one is the fallback.
     */
    public static VersionInfo getVersionInfoCached(String loaderVersion, String minecraftVersion) {
        File cache = new File(Constants.USER_HOME,
                "fabric-loader-" + loaderVersion + "-" + minecraftVersion + ".json");
        long now = System.currentTimeMillis();
        if (cache.isFile() && now - cache.lastModified() < 24L * 60 * 60 * 1000) {
            VersionInfo cached = GsonUtils.jsonFileToObject(cache.getAbsolutePath(), VersionInfo.class);
            if (cached != null) {
                return cached;
            }
        }
        FabricVersion fabricVersion = getVersion(loaderVersion);
        VersionInfo fresh = fabricVersion == null ? null : getVersionInfo(fabricVersion, minecraftVersion);
        if (fresh != null) {
            GsonUtils.objectToJsonFile(cache.getAbsolutePath(), fresh);
            return fresh;
        }
        if (cache.isFile()) {
            VersionInfo cached = GsonUtils.jsonFileToObject(cache.getAbsolutePath(), VersionInfo.class);
            if (cached != null) {
                return cached;
            }
        }
        return null;
    }
}
