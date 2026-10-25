using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Threading.Tasks;
using Newtonsoft.Json;
using TMPro;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.Serialization;
using UnityEngine.UI;
using System.Text.RegularExpressions;

public class LoginHandler : MonoBehaviour
{
    bool isLoggedIn;
    
    [SerializeField] private TMP_Dropdown accountDropdown;
    public List<ConfigHandler.Accounts> accounts = new();
    private Dictionary<string, string> accountUUIDMap = new();
    private string configPath;
    private string configFile;
    public GameObject loginText;
    public WindowHandler handler;
    public UIHandler UIHandler;
    public bool isDemoMode;
    
    public string selectedAccountUsername = "Add Account";
    private string selectedAccountUUID;
    public SkinHandler skinHandler;
    private ConfigHandler.Config config;

    private const string OfflineEntryName = "Offline Login";
    private const string OfflineEntryUuid = "offline";
    private const int OfflineNameMaxLength = 16;
    private GameObject offlineRow;
    private TMP_InputField offlineNameInput;

    public void Start()
    {
	    configPath = Application.persistentDataPath + "/launcher.conf";
	    if (!File.Exists(configPath))
	    {
		    File.Create(configPath).Close();
	    }
	    configFile = File.ReadAllText(configPath);
	    config = JsonConvert.DeserializeObject<ConfigHandler.Config>(configFile);
	    ParseAccounts();
	    Login();
	    BuildSkinButton();
    }

    private float skinImportPoll;

    public void Update()
    {
	    if (Application.platform != RuntimePlatform.Android || JNIStorage.apiClass == null) return;

	    skinImportPoll += Time.unscaledDeltaTime;
	    if (skinImportPoll < 0.5f) return;
	    skinImportPoll = 0f;

	    bool imported;
	    try { imported = JNIStorage.apiClass.GetStatic<bool>("skinImported"); }
	    catch { return; }
	    if (!imported) return;

	    try { JNIStorage.apiClass.SetStatic("skinImported", false); } catch { }
	    LoadAvForCurrentAccount(selectedAccountUsername);
	    Debug.Log("QCXR: imported skin applied to the preview");
    }
	
    public void Login()
    {
	    if (Application.platform != RuntimePlatform.Android) return;
		isLoggedIn = false;
	    selectedAccountUsername = accountDropdown.options[accountDropdown.value].text;

	    if (selectedAccountUsername == OfflineEntryName)
	    {
		    selectedAccountUUID = OfflineEntryUuid;
		    loginText.SetActive(false);
		    ShowOfflineNameEditor();
		    return;
	    }

	    HideOfflineNameEditor();
	    selectedAccountUUID = accountUUIDMap[selectedAccountUsername];
	    Debug.Log(selectedAccountUUID);
	    loginText.SetActive(true);
	    JNIStorage.apiClass.CallStatic("login", JNIStorage.activity, selectedAccountUsername == "Add Account" ? null : selectedAccountUUID);
	    CheckVerification();
    }

    public void ParseAccounts()
    {
	    accounts.Clear();
	    accountUUIDMap.Clear();

	    int i = 0;
	    foreach (var account in config.accounts.Where(account => account != null))
	    {
		    Debug.Log("Account " + i++);
		    accounts.Add(account);
		    accountUUIDMap[account.username] = account.uuid;
		    Debug.Log("Account " + i++ + " added.");
	    }

	    Debug.Log("Accounts parsed.");
	    ConfigHandler.Accounts addAccount = new ConfigHandler.Accounts { username = "Add Account", uuid = "null" };
	    ConfigHandler.Accounts offlineEntry = new ConfigHandler.Accounts { username = OfflineEntryName, uuid = OfflineEntryUuid };

	    if (accounts.Count == 0)
	    {
		    Debug.Log("No accounts, using account sign in.");
		    accounts.Add(addAccount);
		    accountUUIDMap[addAccount.username] = addAccount.uuid;
		    accounts.Add(offlineEntry);
		    accountUUIDMap[offlineEntry.username] = offlineEntry.uuid;
		    accountDropdown.ClearOptions();
		    accountDropdown.AddOptions(accounts.Select(account => account.username).ToList());
		    Login();
	    }
	    else
	    {
		    accounts.Add(addAccount);
		    accountUUIDMap[addAccount.username] = addAccount.uuid;
		    accounts.Add(offlineEntry);
		    accountUUIDMap[offlineEntry.username] = offlineEntry.uuid;
		    accountDropdown.ClearOptions();
		    accountDropdown.AddOptions(accounts.Select(account => account.username).ToList());
	    }
    }
    
    private async void CheckVerification() {
	    if (Application.platform != RuntimePlatform.Android) return;
	    while (!isLoggedIn)
	    {
		    await Task.Delay(1500);
		    
		    Debug.Log("Check Login State");
		    if (JNIStorage.accountObj != JNIStorage.apiClass.GetStatic<AndroidJavaObject>("currentAcc"))
		    {
			    JNIStorage.accountObj = JNIStorage.apiClass.GetStatic<AndroidJavaObject>("currentAcc");
			    if (JNIStorage.accountObj != null)
			    {
				    string configFile = File.ReadAllText(configPath);
				    string accName = JNIStorage.apiClass.GetStatic<string>("profileName");
				    string accUUID = JNIStorage.apiClass.GetStatic<string>("profileUUID");
				    Debug.Log("Logged Into " + accName + " | " + accUUID);
				    loginText.SetActive(false);
				    isLoggedIn = true;
				    config = JsonConvert.DeserializeObject<ConfigHandler.Config>(configFile);
				    bool accExists = config.accounts.Any(account => account.username == accName);
				    if (config.accounts.All(account => account.uuid != accUUID))
				    {
					    config.accounts.Add(new ConfigHandler.Accounts { username = accName, uuid = accUUID });
				    }

				    config.lastSelectedAccount = accountDropdown.value;
				    string JSON = JsonConvert.SerializeObject(config, Formatting.Indented);
				    File.WriteAllText(configPath, JSON);
				    isDemoMode = JNIStorage.apiClass.GetStatic<bool>("isDemoMode");
				    if (!accExists)
				    {
					    ParseAccounts();
				    }

				    selectedAccountUsername = accName;
				    selectedAccountUUID = accUUID;
				    LoadAvForCurrentAccount(accName != "Add Account" ? selectedAccountUsername : accName);
			    }
		    }
	    }
    }
    
    public void RemoveAccount()
    {	    JNIStorage.accountObj = null;
	    JNIStorage.apiClass.CallStatic<bool>("removeAccount", JNIStorage.activity, selectedAccountUUID);
	    
	    configFile = File.ReadAllText(configPath);
	    config = JsonConvert.DeserializeObject<ConfigHandler.Config>(configFile);
	    
	    config.accounts.RemoveAll(account => account.username == selectedAccountUsername);
	    config.lastSelectedAccount = 0;
	    string JSON = JsonConvert.SerializeObject(config, Formatting.Indented);
	    File.WriteAllText(configPath, JSON);
	    
	    accountDropdown.ClearOptions();
	    accountDropdown.AddOptions(accounts.Select(account => account.username).ToList());
	    selectedAccountUsername = accounts.First().username;

	    handler.LogoutWindowUnsetter();
	    ParseAccounts();
	    LoadAvForCurrentAccount(selectedAccountUsername);
    }

    private void LoadAvForCurrentAccount(string username)
    {
	    bool offline;
	    try
	    {
		    offline = JNIStorage.apiClass.GetStatic<bool>("offlineProfile");
	    }
	    catch (System.Exception e)
	    {
		    Debug.Log("QCXR: offlineProfile unavailable: " + e.Message);
		    offline = false;
	    }

	    if (!offline)
	    {
		    handler.LoadAv(username);
		    return;
	    }

	    string skinPath = null;
	    bool slim = false;
	    try
	    {
		    skinPath = JNIStorage.apiClass.GetStatic<string>("offlineSkinPath");
		    slim = JNIStorage.apiClass.GetStatic<bool>("offlineSkinSlim");
	    }
	    catch (System.Exception e)
	    {
		    Debug.Log("QCXR: offline skin info unavailable: " + e.Message);
	    }

	    if (!string.IsNullOrEmpty(skinPath)) handler.LoadCustomOfflineAv(skinPath, slim);
	    else handler.LoadOfflineAv();
    }

    // ----- Offline account (local profile, no Microsoft sign-in) -----

    private void BuildSkinButton()
    {
	    if (accountDropdown == null || skinHandler == null) return;
	    RectTransform strip = accountDropdown.transform.parent as RectTransform;
	    if (strip == null) return;

	    GameObject go = new GameObject("SkinButton", typeof(RectTransform), typeof(CanvasRenderer), typeof(Image), typeof(Button));
	    go.layer = strip.gameObject.layer;
	    go.transform.SetParent(strip, false);

	    RectTransform rt = go.transform as RectTransform;
	    rt.anchorMin = rt.anchorMax = new Vector2(0f, 0f);
	    rt.pivot = new Vector2(0f, 1f);
	    rt.anchoredPosition = new Vector2(14f, -34f);
	    rt.sizeDelta = new Vector2(180f, 46f);
	    rt.localScale = Vector3.one;

	    Image img = go.GetComponent<Image>();
	    Image playImage = UIHandler.playButton != null ? UIHandler.playButton.targetGraphic as Image : null;
	    if (playImage != null && playImage.sprite != null)
	    {
		    img.sprite = playImage.sprite;
		    img.type = Image.Type.Sliced;
		    img.pixelsPerUnitMultiplier = 3f;
	    }
	    img.color = Color.white;

	    Button btn = go.GetComponent<Button>();
	    btn.targetGraphic = img;
	    btn.onClick = new Button.ButtonClickedEvent();
	    btn.onClick.AddListener(skinHandler.PickSkin);

	    GameObject lbl = new GameObject("Text", typeof(RectTransform));
	    lbl.layer = go.layer;
	    lbl.transform.SetParent(go.transform, false);
	    TextMeshProUGUI text = lbl.AddComponent<TextMeshProUGUI>();
	    if (accountDropdown.captionText != null) text.font = accountDropdown.captionText.font;
	    text.fontSize = 26;
	    text.alignment = TextAlignmentOptions.Center;
	    text.color = Color.white;
	    text.raycastTarget = false;
	    text.enableWordWrapping = false;
	    text.text = "Skin";
	    RectTransform lrt = lbl.transform as RectTransform;
	    lrt.anchorMin = Vector2.zero;
	    lrt.anchorMax = Vector2.one;
	    lrt.offsetMin = new Vector2(6f, 2f);
	    lrt.offsetMax = new Vector2(-6f, -2f);

	    Debug.Log("QCXR: skin button built");
    }

    private void ShowOfflineNameEditor()
    {
	    if (offlineRow == null)
	    {
		    try
		    {
			    BuildOfflineNameEditor();
		    }
		    catch (System.Exception e)
		    {
			    Debug.LogException(e);
			    UIHandler.SetAndShowError("Offline UI error: " + e.Message);
			    return;
		    }
	    }
	    // The editor replaces the account panel (avatar, name, logout button) while shown.
	    accountDropdown.gameObject.SetActive(false);
	    offlineRow.SetActive(true);
	    offlineNameInput.text = config != null && !string.IsNullOrEmpty(config.offlineUsername) ? config.offlineUsername : "";
	    offlineNameInput.Select();
	    Debug.Log("QCXR: offline name editor shown");
    }

    private void HideOfflineNameEditor()
    {
	    if (offlineRow != null) offlineRow.SetActive(false);
	    if (accountDropdown != null) accountDropdown.gameObject.SetActive(true);
    }

    public void ConfirmOfflineName()
    {
	    string name = offlineNameInput.text.Trim();
	    Debug.Log("QCXR: confirm offline name '" + name + "'");
	    if (!Regex.IsMatch(name, "^[A-Za-z0-9_]{3,16}$"))
	    {
		    UIHandler.SetAndShowError("Invalid offline name: 3-16 chars, only A-Z a-z 0-9 _");
		    return;
	    }

	    config.offlineUsername = name;
	    string JSON = JsonConvert.SerializeObject(config, Formatting.Indented);
	    File.WriteAllText(configPath, JSON);

	    HideOfflineNameEditor();
	    loginText.SetActive(true);
	    bool ok = JNIStorage.apiClass.CallStatic<bool>("loginOffline", JNIStorage.activity, name);
	    Debug.Log("QCXR: loginOffline result=" + ok);
	    if (ok)
	    {
		    CheckVerification();
	    }
	    else
	    {
		    loginText.SetActive(false);
		    UIHandler.SetAndShowError("Offline login failed, see the log for details.");
	    }
    }

    private void BuildOfflineNameEditor()
    {
	    RectTransform dropRt = accountDropdown.transform as RectTransform;
	    Transform parent = dropRt.parent;
	    Image dropImage = accountDropdown.GetComponent<Image>();
	    TMP_FontAsset font = accountDropdown.captionText != null ? accountDropdown.captionText.font : null;
	    int layer = dropRt.gameObject.layer;

	    // Row occupies exactly the account panel's rect, so the child coordinates are
	    // relative to the same box (the panel itself is hidden while the editor is up).
	    offlineRow = new GameObject("OfflineLoginRow", typeof(RectTransform));
	    offlineRow.layer = layer;
	    // Built inactive: TMP_InputField must not run OnEnable before its references are wired.
	    offlineRow.SetActive(false);
	    offlineRow.transform.SetParent(parent, false);
	    RectTransform rowRt = offlineRow.transform as RectTransform;
	    rowRt.anchorMin = dropRt.anchorMin;
	    rowRt.anchorMax = dropRt.anchorMax;
	    rowRt.pivot = dropRt.pivot;
	    rowRt.anchoredPosition = dropRt.anchoredPosition;
	    rowRt.sizeDelta = dropRt.sizeDelta;
	    rowRt.localRotation = dropRt.localRotation;
	    rowRt.localScale = dropRt.localScale;

	    float w = dropRt.rect.width;
	    float h = dropRt.rect.height;
	    float gap = Mathf.Round(h * 0.13f);
	    float buttonW = Mathf.Round(w * 0.30f);
	    float buttonH = Mathf.Round(h * 0.78f);
	    float inputW = w - gap - buttonW;

	    // ---- input: same rounded panel sprite as the account panel ----
	    GameObject inputGo = new GameObject("OfflineNameInput", typeof(RectTransform), typeof(CanvasRenderer), typeof(Image));
	    inputGo.layer = layer;
	    inputGo.transform.SetParent(offlineRow.transform, false);
	    RectTransform inputRt = inputGo.transform as RectTransform;
	    inputRt.anchorMin = inputRt.anchorMax = new Vector2(0.5f, 0.5f);
	    inputRt.pivot = new Vector2(0.5f, 0.5f);
	    inputRt.anchoredPosition = new Vector2(-0.5f * w + inputW * 0.5f, 0f);
	    inputRt.sizeDelta = new Vector2(inputW, h);
	    inputRt.localScale = Vector3.one;

	    Image inputBg = inputGo.GetComponent<Image>();
	    if (dropImage != null)
	    {
		    inputBg.sprite = dropImage.sprite;
		    inputBg.type = Image.Type.Sliced;
		    inputBg.pixelsPerUnitMultiplier = dropImage.pixelsPerUnitMultiplier;
	    }
	    inputBg.color = new Color(0f, 0f, 0f, 0.65f);

	    GameObject areaGo = new GameObject("Text Area", typeof(RectTransform), typeof(RectMask2D));
	    areaGo.layer = layer;
	    areaGo.transform.SetParent(inputGo.transform, false);
	    RectTransform areaRt = areaGo.transform as RectTransform;
	    areaRt.anchorMin = Vector2.zero;
	    areaRt.anchorMax = Vector2.one;
	    areaRt.offsetMin = Vector2.zero;
	    areaRt.offsetMax = Vector2.zero;

	    TextMeshProUGUI text = CreateOfflineText("Text", areaRt, font, h, layer);
	    text.text = "";
	    TextMeshProUGUI placeholder = CreateOfflineText("Placeholder", areaRt, font, h, layer);
	    placeholder.text = "Offline username";
	    placeholder.fontStyle = FontStyles.Italic;
	    placeholder.color = new Color(1f, 1f, 1f, 0.4f);

	    offlineNameInput = inputGo.AddComponent<TMP_InputField>();
	    offlineNameInput.targetGraphic = inputBg;
	    offlineNameInput.textViewport = areaRt;
	    offlineNameInput.textComponent = text;
	    offlineNameInput.placeholder = placeholder;
	    offlineNameInput.contentType = TMP_InputField.ContentType.Standard;
	    offlineNameInput.lineType = TMP_InputField.LineType.SingleLine;
	    offlineNameInput.characterLimit = OfflineNameMaxLength;
	    offlineNameInput.caretWidth = Mathf.Max(2, Mathf.RoundToInt(h * 0.04f));
	    offlineNameInput.customCaretColor = true;
	    offlineNameInput.caretColor = Color.white;
	    offlineNameInput.selectionColor = new Color(0.25f, 0.55f, 1f, 0.4f);
	    offlineNameInput.text = "";

	    // ---- confirm: Play button's sprite recipe, rebuilt so nothing is stretched ----
	    GameObject btnGo = new GameObject("OfflineConfirm", typeof(RectTransform), typeof(CanvasRenderer), typeof(Image), typeof(Button));
	    btnGo.layer = layer;
	    btnGo.transform.SetParent(offlineRow.transform, false);
	    RectTransform btnRt = btnGo.transform as RectTransform;
	    btnRt.anchorMin = btnRt.anchorMax = new Vector2(0.5f, 0.5f);
	    btnRt.pivot = new Vector2(0.5f, 0.5f);
	    btnRt.anchoredPosition = new Vector2(0.5f * w - buttonW * 0.5f, 0f);
	    btnRt.sizeDelta = new Vector2(buttonW, buttonH);
	    btnRt.localScale = Vector3.one;

	    Image btnImage = btnGo.GetComponent<Image>();
	    Image playImage = UIHandler.playButton.targetGraphic as Image;
	    if (playImage != null && playImage.sprite != null)
	    {
		    btnImage.sprite = playImage.sprite;
		    // 9-slice: the border is (sprite pixels / pixelsPerUnitMultiplier) local units,
		    // so a value below 1 keeps the launcher's thin black outline at this size.
		    btnImage.type = Image.Type.Sliced;
		    btnImage.pixelsPerUnitMultiplier = 0.6f;
	    }
	    else if (dropImage != null)
	    {
		    btnImage.sprite = dropImage.sprite;
		    btnImage.type = Image.Type.Sliced;
		    btnImage.pixelsPerUnitMultiplier = dropImage.pixelsPerUnitMultiplier;
	    }
	    btnImage.color = Color.white;

	    Button btn = btnGo.GetComponent<Button>();
	    btn.targetGraphic = btnImage;
	    btn.onClick = new Button.ButtonClickedEvent();
	    btn.onClick.AddListener(ConfirmOfflineName);

	    TextMeshProUGUI btnLabel = CreateOfflineText("Text", btnRt, font, buttonH, layer);
	    btnLabel.alignment = TextAlignmentOptions.Center;
	    btnLabel.fontSize = Mathf.Round(buttonH * 0.46f);
	    btnLabel.enableAutoSizing = true;
	    btnLabel.fontSizeMin = Mathf.Round(buttonH * 0.16f);
	    btnLabel.fontSizeMax = btnLabel.fontSize;
	    btnLabel.color = Color.white;
	    btnLabel.text = "Confirm";
	    RectTransform lblRt = btnLabel.transform as RectTransform;
	    lblRt.offsetMin = new Vector2(buttonH * 0.08f, buttonH * 0.05f);
	    lblRt.offsetMax = new Vector2(-buttonH * 0.08f, -buttonH * 0.05f);

	    KeyboardHandler kb = FindObjectOfType<KeyboardHandler>();
	    if (kb != null) kb.RegisterExternalField(offlineNameInput);

	    Debug.Log("QCXR: offline editor built, input=" + inputRt.rect.size + " @" + inputRt.anchoredPosition
		    + ", btn=" + btnRt.rect.size + " @" + btnRt.anchoredPosition);
    }

    private static TextMeshProUGUI CreateOfflineText(string name, RectTransform parent, TMP_FontAsset font, float fieldHeight, int layer)
    {
	    GameObject go = new GameObject(name, typeof(RectTransform));
	    go.layer = layer;
	    go.transform.SetParent(parent, false);
	    RectTransform rt = go.transform as RectTransform;
	    rt.anchorMin = Vector2.zero;
	    rt.anchorMax = Vector2.one;
	    rt.pivot = new Vector2(0f, 0.5f);
	    float padX = Mathf.Round(fieldHeight * 0.10f);
	    float padY = Mathf.Round(fieldHeight * 0.16f);
	    rt.offsetMin = new Vector2(padX, padY);
	    rt.offsetMax = new Vector2(-padX, -padY);
	    rt.localScale = Vector3.one;

	    TextMeshProUGUI t = go.AddComponent<TextMeshProUGUI>();
	    if (font != null) t.font = font;
	    t.fontSize = Mathf.Round(fieldHeight * 0.35f);
	    t.alignment = TextAlignmentOptions.MidlineLeft;
	    t.color = Color.white;
	    t.raycastTarget = false;
	    t.enableWordWrapping = false;
	    t.richText = false;
	    t.overflowMode = TextOverflowModes.Overflow;
	    return t;
    }
}