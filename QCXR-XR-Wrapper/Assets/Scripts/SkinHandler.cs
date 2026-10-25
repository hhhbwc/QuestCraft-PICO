using System.IO;
using System.Threading.Tasks;
using Newtonsoft.Json.Linq;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.Serialization;

public class SkinHandler : MonoBehaviour
{
    public Material skin;
    public Material profilePicture;
    private string skinType;

    public GameObject rSlim;
    public GameObject rClassic;
    public GameObject lSlim;
    public GameObject lClassic;

    [FormerlySerializedAs("FigurineSlim")] public GameObject figurineSlim;
    [FormerlySerializedAs("FigurineClassic")] public GameObject figurineClassic;

    [Header("Debug")] public string FETCHINGUSERNAME;

    private Texture defaultSkinTexture;
    private Texture defaultProfileTexture;

    private void Awake()
    {
        // The materials ship with the bundled Steve texture; remember it so offline
        // profiles can fall back to it instead of whatever the last account fetched.
        if (skin != null) defaultSkinTexture = skin.mainTexture;
        if (profilePicture != null) defaultProfileTexture = profilePicture.mainTexture;
    }

    /// <summary>Local profiles have no skin server. minotar resolves by username, so an
    /// offline name that also exists as a real account would show that player's skin.</summary>
    public void LoadOfflineSkin()
    {
        if (skin != null && defaultSkinTexture != null) skin.mainTexture = defaultSkinTexture;
        if (profilePicture != null && defaultProfileTexture != null) profilePicture.mainTexture = defaultProfileTexture;
        skinType = "wide";
        ApplySkinType(classic: true);
        Debug.Log("QCXR: offline skin -> bundled default (classic)");
    }

    private void ApplySkinType(bool classic)
    {
        rSlim.SetActive(!classic);
        lSlim.SetActive(!classic);
        figurineSlim.SetActive(!classic);

        rClassic.SetActive(classic);
        lClassic.SetActive(classic);
        figurineClassic.SetActive(classic);
    }

    /// <summary>Skin PNG supplied by the user for a local profile (no skin server involved).</summary>
    public void LoadLocalSkin(string path, bool slim)
    {
        try
        {
            byte[] bytes = File.ReadAllBytes(path);
            Texture2D texture = new Texture2D(2, 2, TextureFormat.RGBA32, false);
            if (!texture.LoadImage(bytes))
            {
                Debug.Log("QCXR: custom skin is not a valid image: " + path);
                LoadOfflineSkin();
                return;
            }

            texture.filterMode = FilterMode.Point;
            texture.wrapMode = TextureWrapMode.Clamp;
            skin.mainTexture = texture;
            profilePicture.mainTexture = texture;
            skinType = slim ? "slim" : "wide";
            ApplySkinType(classic: !slim);
            Debug.Log("QCXR: custom offline skin loaded: " + path + (slim ? " (slim)" : " (wide)"));
        }
        catch (System.Exception e)
        {
            Debug.Log("QCXR: custom skin load failed, using default: " + e.Message);
            LoadOfflineSkin();
        }
    }

    [ContextMenu("DEBUG FETCH")]
    public void DEBUGFETCH()
    {
        LoadSkin(FETCHINGUSERNAME);
    }

    public void LoadSkin(string username)
    {
        async Task LoadImage(string username)
        {
            UnityWebRequest request = UnityWebRequestTexture.GetTexture("https://minotar.net/skin/" + username);
            request.SetRequestHeader("User-Agent", "QuestCraftPlusPlus/QuestCraft/" + Application.version + " (discord.gg/questcraft)");
            request.SendWebRequest();

            while (!request.isDone)
                await Task.Delay(16);
            
            if (request.result != UnityWebRequest.Result.Success)
                Debug.Log(request.error);
            else
            {
                Debug.Log("Loading skin");
                skin.mainTexture = ((DownloadHandlerTexture)request.downloadHandler).texture;
                skin.mainTexture.filterMode = FilterMode.Point;
                profilePicture.mainTexture = ((DownloadHandlerTexture)request.downloadHandler).texture;
                profilePicture.mainTexture.filterMode = FilterMode.Point;
            }
        }

        async Task SetSkinType(string username)
        {
            skinType = null; // do not reuse the previous account's arm model
            UnityWebRequest www = UnityWebRequest.Get("https://starlightskins.lunareclipse.studio/info/user/" + username);
            www.SetRequestHeader("User-Agent", "QuestCraftPlusPlus/QuestCraft/" + Application.version + " (discord.gg/questcraft)");
            www.SendWebRequest();

            while (!www.isDone)
            {
                await Task.Delay(16);
            }
            
            if (www.result != UnityWebRequest.Result.Success)
            {
                Debug.Log(www.error);
            }
            else
            {
                try
                {
                    JObject SkinType = JObject.Parse(www.downloadHandler.text);
                    skinType = (string)SkinType["skinType"];
                }
                catch (System.Exception e)
                {
                    Debug.Log("QCXR: skin type lookup failed: " + e.Message);
                }
                Debug.Log("skin type: " + (skinType ?? "<unknown>"));
            }

            // Unknown / missing answer -> classic, never the slim model (a slim
            // model over a classic texture is what makes arms look displaced).
            ApplySkinType(classic: skinType != "slim");
        }
        
        LoadImage(username);
        SetSkinType(username);
    }

    /// <summary>Opens the system image picker; the Java side stores the result as files/skins/custom.png.</summary>
    public void PickSkin()
    {
        try
        {
            Debug.Log("QCXR: opening skin picker");
            JNIStorage.apiClass.CallStatic("pickSkin", JNIStorage.activity);
        }
        catch (System.Exception e)
        {
            Debug.Log("QCXR: cannot open skin picker: " + e.Message);
        }
    }
}
