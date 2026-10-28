using System.Threading.Tasks;
using TMPro;
using Unity.XR.CoreUtils;
using UnityEngine;
using UnityEngine.UI;

public class KeyboardHandler : MonoBehaviour
{
    [SerializeField] private TMP_InputField searchBox;
    [SerializeField] private TMP_InputField ramSetter;
    [SerializeField] private TMP_InputField instanceEditor;
    [SerializeField] private GameObject keyboard;
    public AudioSource keyPop;
    private static bool isShift;
    private TMP_InputField externalField;

    // Input field created at runtime (e.g. offline login) registers itself here
    // so the in-VR keyboard also types into it.
    public void RegisterExternalField(TMP_InputField field)
    {
        externalField = field;
    }

    private void Update()
    {
        keyboard.SetActive(searchBox.IsActive() || ramSetter.IsActive() || instanceEditor.IsActive() || (externalField != null && externalField.IsActive()));
    }
    
    public void KeyPress(string key)
    {
        if (searchBox.IsActive())
        {
            searchBox.text += isShift ? key.ToUpper() : key.ToLower();
            keyPop.Play();
        }
        else if (ramSetter.IsActive())
        {
            ramSetter.text += isShift ? key.ToUpper() : key.ToLower();
            keyPop.Play();
        } else if (instanceEditor.IsActive())
        {
            instanceEditor.text += isShift ? key.ToUpper() : key.ToLower();
            keyPop.Play();
        }
        else if (externalField != null && externalField.IsActive())
        {
            if (externalField.characterLimit <= 0 || externalField.text.Length < externalField.characterLimit)
            {
                externalField.text += isShift ? key.ToUpper() : key.ToLower();
                keyPop.Play();
            }
        }
    }

    public void BackspacePress()
    {
        if (searchBox.IsActive())
        {
            if (searchBox.text.Length > 0)
                searchBox.text = searchBox.text.Remove(searchBox.text.Length - 1, 1);
            keyPop.Play();
        }
        else if (ramSetter.IsActive())
        {
            if (ramSetter.text.Length > 0)
                ramSetter.text = ramSetter.text.Remove(ramSetter.text.Length - 1, 1);
            keyPop.Play();
        } else if (instanceEditor.IsActive())
        {
            if (instanceEditor.text.Length > 0)
                instanceEditor.text = instanceEditor.text.Remove(instanceEditor.text.Length - 1, 1);
            keyPop.Play();
        }
        else if (externalField != null && externalField.IsActive() && externalField.text.Length > 0)
        {
            externalField.text = externalField.text.Remove(externalField.text.Length - 1, 1);
            keyPop.Play();
        }
    }

    public void ShiftPress()
    {
        ShiftPressTask();
    }

    private async Task ShiftPressTask()
    {
        isShift = !isShift;
        // var keys = keyboard.GetNamedChild("Keys").GetChildGameObjects().GetComponentsInChildren<TMP_Text>();
        //
        // foreach (var key in keys)
        // {
        //     key.text = isShift ? key.text.ToLower() : key.text.ToUpper();
        // }
        
        keyPop.Play();
    }

    public void SpacePress()
    {
        if (searchBox.IsActive())
        {
            searchBox.text += " ";
            keyPop.Play();
        }
        else if (ramSetter.IsActive())
        {
            ramSetter.text += " ";
            keyPop.Play();
        } else if (instanceEditor.IsActive())
        {
            instanceEditor.text += " ";
            keyPop.Play();
        }
        else if (externalField != null && externalField.IsActive())
        {
            if (externalField.characterLimit <= 0 || externalField.text.Length < externalField.characterLimit)
            {
                externalField.text += " ";
                keyPop.Play();
            }
        }
    }
}