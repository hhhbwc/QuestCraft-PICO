using System.Collections.Generic;
using System.IO;
using Newtonsoft.Json;
using TMPro;
using UnityEngine;

public class ConfigHandler : MonoBehaviour
{
    public TMP_Dropdown accountsDropdown;
    public WindowHandler windowHandler;
    public DevHandler devHandler;
    public Config config;
    public static string configPath;

    public void LoadConfig()
    {
        configPath = Application.persistentDataPath + "/launcher.conf";
        
        if (File.Exists(configPath))
        { 
            string configFile = File.ReadAllText(configPath);
            config = JsonConvert.DeserializeObject<Config>(configFile);
            
            if (!config.acceptedLegal) { windowHandler.LegalSetter(); }
            if (config.setDevMods) { devHandler._devToggle.isOn = true; }
            if (config.setCustomRAM)
            {
                devHandler._ramSetterToggle.isOn = true;
                devHandler._ramSetterField.text = config.customRAMValue;
                JNIStorage.apiClass.SetStatic("memoryValue", config.customRAMValue);
            }
        }
        else
        {
            config = new Config
            {
                acceptedLegal = false,
                setDevMods = false,
                setCustomRAM = false,
                customRAMValue = "2048",
                lastSelectedInstance = 0,
                lastSelectedInstanceName = "26.2",
                lastSelectedAccount = 0,
                accounts = new List<Accounts>()
            };

            SaveConfig();
            windowHandler.LegalSetter();
        }

        // The dropdown is populated by JNIStorage.UpdateInstances(); applying the saved
        // selection any earlier clamps it to index 0 (26.3) and loses the user's version.
        ApplyInstanceSelection();
    }

    /** Restores the last used version by name, so reordering the supported list cannot shift it. */
    public void ApplyInstanceSelection(string preferredName = null)
    {
        if (config == null || JNIStorage.instance == null || JNIStorage.instance.instancesDropdown == null)
            return;

        TMP_Dropdown dropdown = JNIStorage.instance.instancesDropdown;
        List<TMP_Dropdown.OptionData> options = dropdown.options;
        if (options.Count == 0)
            return;

        string savedName = string.IsNullOrEmpty(preferredName) ? config.lastSelectedInstanceName : preferredName;
        int index = -1;
        if (!string.IsNullOrEmpty(savedName))
        {
            index = options.FindIndex(option => option.text == savedName);
        }
        if (index < 0)
        {
            // Old configs only stored an index, and a fresh config used to land on 26.3
            // (the first supported version) even though it cannot start on PICO yet.
            index = options.FindIndex(option => option.text == "26.2");
        }
        if (index < 0)
        {
            index = Mathf.Clamp(config.lastSelectedInstance, 0, options.Count - 1);
        }

        config.lastSelectedInstance = index;
        config.lastSelectedInstanceName = options[index].text;
        dropdown.SetValueWithoutNotify(index);
        if (JNIStorage.instance.uiHandler != null)
        {
            JNIStorage.instance.uiHandler.SyncInstanceSelection(index);
        }
        SaveConfig();
    }

    public void SetLastSelectedInstance(int instance)
    {
        config = JsonConvert.DeserializeObject<Config>(File.ReadAllText(configPath));

        TMP_Dropdown dropdown = JNIStorage.instance != null ? JNIStorage.instance.instancesDropdown : null;
        if (dropdown != null && dropdown.options.Count > 0)
        {
            instance = Mathf.Clamp(instance, 0, dropdown.options.Count - 1);
            config.lastSelectedInstance = instance;
            config.lastSelectedInstanceName = dropdown.options[instance].text;
        }

        SaveConfig();
    }

    public void SetLastSelectedAccount(int account)
    {
        config = JsonConvert.DeserializeObject<Config>(File.ReadAllText(configPath));
        config.lastSelectedAccount = account;
        SaveConfig();
    }
    
    public void SetMemoryValue()
    {
        config = JsonConvert.DeserializeObject<Config>(File.ReadAllText(configPath));
        string ramValue = devHandler._ramSetterField.text;
        
        if (ramValue != null)
        {
            config.customRAMValue = ramValue;
            SaveConfig();
            JNIStorage.apiClass.SetStatic("memoryValue", ramValue);
        }
    }

    public void AgreeToLegal()
    {
        config = JsonConvert.DeserializeObject<Config>(File.ReadAllText(configPath));
        config.acceptedLegal = true;
        SaveConfig();
    }

    private void SaveConfig()
    {
        File.WriteAllText(configPath, JsonConvert.SerializeObject(config, Formatting.Indented));
    }

    public class Config
    {
        public bool acceptedLegal;
        public bool setDevMods;
        public bool setCustomRAM;
        public string customRAMValue;
        public int lastSelectedInstance;
        public string lastSelectedInstanceName;
        public int lastSelectedAccount;
        public string offlineUsername;
        public List<Accounts> accounts;
    }
    
    public class Accounts
    {
        public string username;
        public string uuid;
    }
}
