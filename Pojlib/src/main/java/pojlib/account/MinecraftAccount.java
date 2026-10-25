package pojlib.account;

import android.app.Activity;

import com.google.gson.Gson;

import org.json.JSONException;

import java.io.File;
import java.io.FileReader;
import java.io.IOException;

import java.nio.charset.StandardCharsets;
import java.util.UUID;

import javax.annotation.Nullable;

import pojlib.util.Constants;
import pojlib.util.GsonUtils;
import pojlib.util.Logger;
import pojlib.util.MSAException;

public class MinecraftAccount {
    public String accessToken;
    public String uuid;
    public String username;
    public boolean isDemoMode = false;
    public long expiresOn;
    public String userType = "msa";

    public static MinecraftAccount login(Activity activity, String gameDir, String msToken) throws MSAException, IOException, JSONException {
        Msa instance = new Msa(activity);
        MinecraftAccount account = instance.performLogin(msToken);

        GsonUtils.objectToJsonFile(gameDir + "/" + account.uuid + ".json", account);
        return account;
    }

    /**
     * Create a local offline profile (no Microsoft account needed, for testing).
     * Same scheme as third-party launchers: UUID = v3 UUID of "OfflinePlayer:<name>" (MD5),
     * fake access token "0". The account JSON is persisted so it can be re-selected later
     * via API.login like any other account.
     */
    public static MinecraftAccount loginOffline(Activity activity, String gameDir, String username) {
        if (username == null || !username.matches("[A-Za-z0-9_]{3,16}")) {
            throw new IllegalArgumentException("Invalid offline username: " + username + " (must be 3-16 chars: A-Z a-z 0-9 _)");
        }

        MinecraftAccount account = new MinecraftAccount();
        account.username = username;
        account.uuid = UUID.nameUUIDFromBytes(("OfflinePlayer:" + username).getBytes(StandardCharsets.UTF_8)).toString();
        account.accessToken = "0";
        account.userType = "legacy";
        account.isDemoMode = false;
        account.expiresOn = Long.MAX_VALUE; // never refresh through the MS auth flow

        GsonUtils.objectToJsonFile(gameDir + "/" + account.uuid + ".json", account);
        return account;
    }

    public static boolean removeAccount(Activity activity, String uuid) {
        File accountFile = new File(activity.getFilesDir() + "/accounts/" + uuid + ".json");

        // Only wipe the MS auth cache for real Microsoft accounts; offline profiles (token "0") must not touch it.
        MinecraftAccount acc = load(activity.getFilesDir() + "/accounts", uuid);
        boolean offlineProfile = acc != null && "0".equals(acc.accessToken);
        boolean removed = accountFile.delete();
        if (!offlineProfile) {
            File accountCache = new File(Constants.USER_HOME + "/cache_data");
            removed = removed && accountCache.delete();
        }
        return removed;
    }

    //Try this before using login - the account will have been saved to disk if previously logged in
    public static MinecraftAccount load(String path, String uuid) {
        return GsonUtils.jsonFileToObject(path + "/" + uuid + ".json", MinecraftAccount.class);
    }

    public static String getSkinFaceUrl(MinecraftAccount account) {
        if (account.isDemoMode) {
            return Constants.MINOTAR_URL + "/helm/MHF_Steve";
        } else {
            try {
                return Constants.MINOTAR_URL + "/helm/" + account.uuid;
            } catch (NullPointerException e) {
                Logger.getInstance().appendToLog("Username likely not set! Please set your username at Minecraft.net and try again. | " + e);
                return null;
            }
        }
    }
}
