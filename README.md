# QuestCraft-PICO 私有版 · 源码包

**已改好的完整源码，拿到就能构建**：不需要 git、不需要打补丁。

> 私有版本：含「离线账户（Offline Login）」与「自定义皮肤」。
> 这块绕过的是 Minecraft 的账号授权，正常使用仍需你自己拥有 Minecraft: Java Edition；
> 请勿公开分发本源码或构建产物。

## 一次性准备

| 需要 | 版本 | 说明 |
|---|---|---|
| Unity | **2022.3.22f1** + Android Build Support | 必须同版本，工程里写死了 |
| JDK | **17** | 打 Pojlib AAR |
| Android SDK | 含 build-tools 34 / platform 35 | AAR 用 |
| 网络 | 首次需要 | Gradle 依赖 + Unity 包导入 |
| adb | 可选 | `-Install` 时用 |

脚本会自动在常见位置找 Unity / JDK17 / Android SDK；找不到就显式传：

```powershell
.\build.ps1 -Unity 'C:\Program Files\Unity 2022.3.22f1' -Jdk17 'C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot' -AndroidSdk 'D:\android-sdk'
```

## 构建

```powershell
.\build.ps1                 # AAR + APK
.\build.ps1 -SkipAar        # 只改了 C# 时，省掉 AAR 步骤
.\build.ps1 -Install        # 构建完顺便 adb install -r
```

产物：`out\QuestCraft-pico-private-<时间>.apk`，Unity 日志同目录。

- **首次构建 20–40 分钟**（Unity 要导入整个工程、生成 `Library`，还要联网拉包）；
- 之后改了 Java → 1–4 分钟；只改 C# → 1–2 分钟。

## 里面有什么

```
Pojlib/                  Java 侧（Gradle 工程）
  src/main/java/pojlib/util/OfflineSkin.java    离线皮肤：覆盖 MC 的 18 张默认皮肤 + 维护 options.txt
  src/main/java/pojlib/util/ModCompat.java      Sodium 置空（否则方块不渲染）
  src/main/assets/mods/vivecraft-26.3-*.bin     Quest 版 Vivecraft（26.3 移植件，26.2 用不上）
  src/main/assets/sodium-stub.jar               320B 空壳，prelaunch 时替换真 Sodium
  src/main/jniLibs/arm64-v8a/libfreetype.so     Bionic 2.13（不能换成 Maven 上 LWJGL 的 glibc 版）
  src/main/assets/runtimes/                     内置 JRE（约 100MB，必需）
QCXR-XR-Wrapper/         Unity 工程（Assets / Packages / ProjectSettings）
  Assets/Scripts/LoginHandler.cs                离线账户 UI（运行时构建输入框 + Confirm）
  Assets/Scripts/SkinHandler.cs                 本地皮肤 / 系统选图 / slim-classic 判定
  Assets/Scripts/KeyboardHandler.cs             VR 键盘打进运行时输入框（16 字封顶）
  Assets/Scripts/InstanceButton.cs              启动链异常不再被吞
docs/KNOWN-ISSUES.md     每一处兼容修复的根因与验证方法
LICENSE                  LGPL-3.0（Pojlib / QCXR-XR-Wrapper 上游即此许可）
```

## 常改的地方

| 想改什么 | 改哪里 |
|---|---|
| 离线名字规则 / UUID | `Pojlib/src/main/java/pojlib/API.java` → `loginOffline` |
| 离线皮肤资源包内容 | `Pojlib/src/main/java/pojlib/util/OfflineSkin.java` |
| 离线输入框 / Confirm 按钮布局 | `QCXR-XR-Wrapper/Assets/Scripts/LoginHandler.cs` → `BuildOfflineNameEditor` |
| 皮肤选择（PFP / 小人偶 / 体型） | `Assets/Scripts/SkinHandler.cs` |
| 启动器整体 UI | `Assets/Scripts/UIHandler.cs`、`WindowHandler.cs` 与场景 `Assets/Scenes/Main.unity` |

## 常见问题

- **gradle 报 AccessDeniedException**：某个目录被打了只读属性（备份工具常见）。脚本启动时会自动清一次；仍失败就手动清 `Pojlib\build`。
- **Unity 立刻失败（bee exit code 3）**：同上，清 `QCXR-XR-Wrapper\Library`、`Temp` 的只读属性。
- **方块不可见 / 能透视看到地下**：说明 Sodium 没被置空。进游戏后检查设备上
  `/sdcard/Android/data/com.qcxr.qcxr/files/instances/<版本>/mods/Sodium.jar` 是否为 320 字节。
- **启动即崩、日志里有 `libfreetype.so`**：jniLibs 里那份被换成了 glibc 构建。必须是 Bionic（`NEEDED` 只有 `libm.so/libdl.so/libc.so`）。
- **26.3 入口闪退**：已知，MC 26.3 强制 SDL，Android 侧没有对应绑定（见 docs/KNOWN-ISSUES.md #4）。用 26.2 实例。

## 关于签名与构建类型（重要）

- 默认走 **Debug** 构建（`QuestCraftCliBuild` 只有在传 `-release` 时才用自定义 keystore），
  签名用的是**本机**的 `%USERPROFILE%\.android\debug.keystore`。
- 所以：**别的机器编出来的 APK 不能覆盖安装另一台机器编的 APK**（签名不同）。
  如果设备上已有别人的构建：

  ```bash
  # 先备份启动器数据（实例/皮肤/账户），卸载会一起删掉
  adb shell "cp -r /sdcard/Android/data/com.qcxr.qcxr/files /sdcard/Download/qcxr-backup"
  adb uninstall com.qcxr.qcxr
  # 装新包后把 files 拷回去
  ```

- 想统一签名 / 想要 Release 性能，改用：

  ```powershell
  # 先生成一个项目 keystore（一次即可）
  keytool -genkeypair -v -keystore keystore\qcxr.keystore -alias qcxr `
          -keyalg RSA -keysize 2048 -validity 10000 `
          -storepass qcxrpass -keypass qcxrpass -dname "CN=QCXR, O=QCXR, C=CN"
  ```

  然后把 `build.ps1` 里 Unity 的启动参数加上：
  `'-release', '-keystorePath', 'keystore\qcxr.keystore', '-keystorePass', 'qcxrpass', '-keyaliasName', 'qcxr', '-keyaliasPass', 'qcxrpass'`

  Release 会开优化（IL2CPP 原生代码明显更快），但要重新卸载安装一次。

## 性能提醒

当前所有构建都是 **Sodium 被置空 + 无 Vulkan** 的状态（见 docs/KNOWN-ISSUES.md #2/#3），
区块加载和帧数都会低于官方 Quest 版；这是 Adreno 650 + MobileGlues 的硬限制，不是配置问题。
