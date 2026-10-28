# QuestCraft-PICO 交接文档

> 交给后续 AI 使用。请先读完本文件，再开始修改或测试。
>
> 状态截止：2026-10-28。本文区分“已证实”“候选原因”和“尚未验证”，不要把候选原因写成已修复结论。

## 1. 当前目标

项目是运行在 PICO 4 上的 360° VR Minecraft Java 应用，用户希望做一次完整检查和优化，重点关注：

- VR 中显示文字、HUD、按钮与实际射线热区是否错位；
- 启动速度；
- 帧率、卡顿、内存回收和渲染后端；
- 触摸/手柄按钮失效、空输入崩溃、导入/镜像功能失效；
- Minecraft 26.2 和 26.3 两个实例是否都能启动、进入世界、退出并再次启动。

用户特别提醒：这是 360° VR 3D 应用，普通 2D 截图可能没有拍到目标 UI。因此不能因为截图看不到某个按钮，就判断按钮不存在或失效；VR UI 必须结合头显观察、射线交互和日志验证。

## 2. 项目与设备

### 本机项目

- Git 仓库：`C:\Users\wzy\Documents\Qoder\2026-10-24\qcxr-oss`
- 当前分支：`main`
- 当前 HEAD：`4bcb222` (`docs: soften offline login disclaimer to 'testing only'`)
- 上一个重要提交：`9228ae9` (`feat: Minecraft 26.3 SDL backend + per-version renderer isolation + perf fixes`)
- 工作区不是干净状态，见第 6 节。

### 构建环境

- Unity：`C:\Program Files\Unity\2022.3.22f1`
- JDK 17：`C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot`
- Android SDK：`D:\android-sdk`
- Android 包名：`com.qcxr.qcxr`
- Activity：`pojlib.UnityPlayerActivity`
- 设备：PICO 4，型号 `PICOA8110`，Android 10，设备已 root（Magisk）
- GPU/图形：Adreno 650，GLES 3.2，Vulkan 1.1，OpenGL + MobileGlues 是当前主要路径
- 设备已存在实例：`26.2`、`26.3`

### 项目架构

- `QCXR-XR-Wrapper/`：Unity 2022.3 启动器、离线账户 UI、VR Canvas、按钮和 OpenXR 交接。
- `Pojlib/`：Java/Android 启动链、内置 OpenJDK 25、LWJGL、Minecraft 启动、MobileGlues/Sodium/freetype 兼容处理。
- 26.2：GLFW/LWJGL 3.3.6 路径。
- 26.3：SDL/LWJGL 3.4.1 路径，使用独立 SDL Surface/native hook。
- APK 同时放置未加后缀 native 和 `341` 后缀 native，以便 26.2 与 26.3 共存。

## 3. 已完成的代码修改

这些修改已经进入当前工作区，并包含在本次 AAR/APK 构建中。

### 3.1 26.3 LWJGL native 名称映射

新增：

`Pojlib/src/main/java/pojlib/lwjgl/BundledLibraryNames341.java`

作用：将 LWJGL 3.4.1 的裸模块名，例如 `lwjgl`、`lwjgl_opengl`，映射成 APK 中的 `lwjgl341`、`lwjgl_opengl341` 对应 native 名称；已经带 `.so` 或已经以 `341` 结尾的名称保持不变。

接入位置：

`Pojlib/src/main/java/pojlib/util/JREUtils.java:694-707`

26.3 SDL 分支现在追加：

```text
-Dorg.lwjgl.system.bundledLibrary.nameMapper=pojlib.lwjgl.BundledLibraryNames341
```

背景：旧设备日志曾明确出现 LWJGL native version mismatch，说明 3.4.1 Java 类可能绑定到旧的 3.3.6 native。此前设备上的 26.3 仍能成功进入游戏，但存在版本错配和潜在运行期不稳定；不能把这个问题描述成“必然启动失败”。

### 3.2 键盘退格空输入保护

文件：`QCXR-XR-Wrapper/Assets/Scripts/KeyboardHandler.cs`

`BackspacePress()` 对以下输入框增加了 `text.Length > 0` 检查：

- `searchBox`
- `ramSetter`
- `instanceEditor`

原代码空字符串时会执行 `Remove(-1, 1)`，导致异常。外部输入框分支原本已有长度检查。

### 3.3 实例按钮空实例保护

文件：`QCXR-XR-Wrapper/Assets/Scripts/InstanceButton.cs`

- `MirrorNativesForInstance()` 从无 `await` 的 `async void` 改为 `void`。
- `ImportSaves()` 从无 `await` 的 `async void` 改为 `void`。
- 两个函数在 `JNIStorage.GetInstance(currInstName)` 返回 null 时记录 warning 并返回，避免空引用异常。

### 3.4 时钟文本更新降频

文件：`QCXR-XR-Wrapper/Assets/Scripts/UIHandler.cs`

增加 `lastClock` 缓存。只有分钟/小时文本变化时才写入 `minuteHourText.text`，避免每帧重复触发 TextMeshPro 文本更新。

## 4. 本次构建结果

### 构建命令

本次 Unity APK 重试使用：

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 `
  -SkipAar `
  -Unity "C:\Program Files\Unity 2022.3.22f1" `
  -Jdk17 "C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot" `
  -AndroidSdk "D:\android-sdk"
```

`-SkipAar` 使用的是前一步刚构建好的 AAR；前一步 AAR 构建成功，输出约 108.7 MB，已复制到：

`QCXR-XR-Wrapper/Assets/Plugins/Android/Pojlib-release.aar`

AAR 中已确认存在：

`pojlib/lwjgl/BundledLibraryNames341.class`

构建时 `tools/overlay341` 源 overlay jar 不存在，脚本使用仓库中已提交的 overlay copy；这不是构建失败。

### APK 产物

- 构建状态：成功，Unity 退出码 0
- 输出：`out/QuestCraft-pico-private-20261028-0856.apk`
- 大小：约 146.4 MB
- SHA-256：

```text
97782edffb508528035eab842c0297ec82be09763d946143080df4cfe9b9a991
```

- 构建耗时：约 5.1 分钟
- Unity 日志：`out/build-20261028-0856.log`

注意：截至本文生成时，这个新 APK **只确认构建成功，尚未确认已经安装并在头显上运行**。不要把新 mapper 或本轮 C# 修改写成“已通过真机验证”。

第一次完整构建因 Unity Package Manager 暂时出现 `packages.unity.com` DNS `ENOTFOUND` 失败；第二次使用已有 AAR 重试成功。不是当前代码编译错误。

## 5. 之前的设备实测证据

以下主要来自 2026-10-26 安装的旧 APK，不能直接等同于本次新 APK 的结果。

### 5.1 26.2

26.2 已经可以启动并进入 Minecraft 世界。一次启动记录的 prelaunch 小计大约为：

- env + JVM：1453 ms
- LWJGL runtime：191 ms
- client：1000 ms
- libraries：791 ms
- assets：2 ms
- compat assets：92 ms
- 合计约 3.5 秒

这些数字只覆盖已安装资源后的 prelaunch，不包含首次下载、JVM 完整初始化、Minecraft 建世界和 shader warm-up。

设备日志中曾出现多次 ZGC Minor Collection，时间点约为启动后 114 s、141 s、168 s；记录的 collection 周期总时长约 4.425 s、3.739 s、0.938 s。服务线程还曾报告落后约 10.749 s（214 ticks）。这说明存在内存分配压力或线程延迟，需要进一步测量，但不能把 collection 的总周期时长直接当成 Java 主线程 stop-the-world 暂停。

26.2 当前视频设置曾观察到：

- `guiScale=4`
- `renderDistance=6`
- `preferredGraphicsBackend=default`
- `lang=zh_cn`
- `fullscreenResolution=1920x1080@90:24`

PICO 4 的 Vulkan 1.1 不满足该 Minecraft 路径所需的 Vulkan 扩展，因此建议优先用 OpenGL + MobileGlues；不要因为设置里保存了 Vulkan 就认为硬件一定能用 Vulkan。

### 5.2 26.3

旧 APK 的 26.3 在 2026-10-26 19:30 左右曾成功运行：

- 日志出现 `LWJGL 3.4.1 SDL runtime installed`
- 日志出现 `QuestCraft: SDL support enabled!`
- SDL 3.4.14 初始化成功
- GLES 3.2 初始化成功
- 成功进入游戏
- 约 19:33 保存世界后退出

但同一批日志明确出现：

```text
-Djava.library.path is set to a folder containing shared libraries of an older LWJGL version
```

这正是本轮加入 3.4.1 name mapper 要验证的风险点。结论应写成“26.3 曾成功启动，但旧构建存在 native 版本错配警告”，而不是“26.3 完全无法启动”。

26.3 旧日志还出现过：

- 14:10 左右 `Unable to initialize SDL`，后续成功运行表明该问题至少部分修复；
- 18:28 左右 `Client shutdown from post-main`，说明退出路径/重启 watchdog 仍需复测；
- 26.3 设置曾观察到 `guiScale=2`、`renderDistance=4`、`preferredGraphicsBackend=opengl`、`lang=zh_cn`。

### 5.3 Sodium/MobileGlues

设备上 26.2 和 26.3 的 `Sodium.jar` 都是真实 jar，不是 `sodium_stub.txt` 强制回退状态；两者都有 `.orig` 备份。

`Pojlib/src/main/java/pojlib/util/ModCompat.java` 当前逻辑：

- 启动前安装 MobileGlues 配置；
- 26.2+ 默认恢复真实 Sodium；
- 监听 `files/mg/latest.log`；
- 出现 shader 转换、编译、链接或 `Invalid #version` 等错误时，把 Sodium 替换为 inert stub，并保留 `.orig`；
- 写入 `sodium_stub.txt`，下次启动回退原版地形管线。

MobileGlues 配置日志曾显示 `ignoreError=1`，GLSL cache 上限约 128 MB。旧 logcat 里还出现过 Adreno `kgsl_sharedmem_alloc` / `glTexSubImage2D` 相关栈，是否与 LWJGL native 错配有关尚未证实。

## 6. 当前工作区状态

当前 `git status --short` 主要显示：

```text
 M QCXR-XR-Wrapper/Assets/Scripts/InstanceButton.cs
 M QCXR-XR-Wrapper/Assets/Scripts/KeyboardHandler.cs
 M QCXR-XR-Wrapper/Assets/Scripts/UIHandler.cs
 M QCXR-XR-Wrapper/Assets/XR/Settings/OpenXR Package Settings.asset
 M QCXR-XR-Wrapper/Packages/packages-lock.json
 M QCXR-XR-Wrapper/ProjectSettings/Packages/com.unity.testtools.codecoverage/Settings.json
 M QCXR-XR-Wrapper/ProjectSettings/ProjectSettings.asset
?? Pojlib/src/main/java/pojlib/lwjgl/
?? QCXR-XR-Wrapper/Assets/Plugins/Android/Pojlib-release.aar
```

其中前三个 C# 文件和 `Pojlib/src/main/java/pojlib/lwjgl/BundledLibraryNames341.java` 是本轮有意修改。Unity 设置文件可能是 Unity 导入/构建过程中产生的变更，提交前必须逐个 `git diff` 审核，不要盲目提交。

AAR 是构建产物。如果项目仓库不希望提交二进制，应在交付前确认项目约定；但 Unity 本地构建需要它存在。

## 7. 尚未解决或需要重点复核的问题

### 7.1 文字/画面错位：当前没有最终结论

已知候选点：

1. `QCXR-XR-Wrapper/Assets/Scripts/UiRedocker.cs:29-35`
   - UI 重新挂到 `watchDisplay` 或 `mainScreen` 时强制设置：

     ```csharp
     ui.transform.localScale = new Vector3(-1, 1, -1);
     ```

   - 负缩放可能造成文字镜像、视觉方向与射线热区不一致，必须在头显中分别观察主屏和手表屏，并测试按钮射线位置。

2. 26.3 SDL 分辨率/viewport 同步
   - 之前分析怀疑 SDL 窗口不消费 GLFW 的窗口尺寸事件，游戏内改变分辨率后可能出现 viewport、buffer、Canvas/画面尺寸脱节。
   - 当前 HEAD 中没有找到 `SdlBridge.syncResolution` 或 `syncResolution` 的落地实现。不要把它当作已完成修复。
   - 应先在新 APK 上复现：进入 26.3，记录初始分辨率，切换分辨率/GUI scale，观察文字、准星、按钮和实际射线是否发生偏移；同时查 SDL/window size、surface、viewport 日志。

3. VR Canvas 尺寸/相机/射线
   - 静态检查显示场景有 XRI `XRUIInputModule`；世界空间 Canvas 有标准 `GraphicRaycaster` 和 `TrackedDeviceGraphicRaycaster`。
   - 静态管线没有发现“完全没有事件系统”的问题，但这不能证明每个 UI 的 RectTransform、层级、负缩放和交互区域正确。

由于是 360° VR，优先用头显观察和交互日志验证；截图只能作为辅助。

### 7.2 启动速度

已知 prelaunch 约 3.5 秒只是资源已准备好的小计。需要将启动拆成：

- Unity 启动器进入可操作界面；
- 点击/自动启动到 `prelaunchCheck` 开始；
- env + JVM；
- LWJGL 安装；
- client/libraries/assets/compat assets；
- Java VM 启动；
- OpenXR handoff；
- Minecraft 主菜单；
- 世界可交互。

不要只报一个总时间。使用 `Logger` 中已有的 `prelaunch: ... took ... ms` 行，并配合 logcat 时间戳。

### 7.3 帧率和卡顿

需要同时记录：

- 头显刷新率和实际稳定帧率；
- 进入世界后静止、转头、移动、加载区块三种场景；
- render distance、GUI scale、后端、MobileGlues/Sodium 状态；
- ZGC 日志和服务线程 tick lag；
- 是否出现 Adreno shader、纹理上传或 `kgsl` 错误。

不要仅凭一次截图或一次短时主观感受下结论。

### 7.4 退出路径

26.3 旧日志曾出现 `Client shutdown from post-main`。必须分别测试：

- 游戏内正常退出；
- 从 Unity/头显界面返回；
- 退出后是否恢复启动器 UI；
- 再次启动 26.2/26.3 是否仍可用；
- 是否残留 Java 进程、SDL surface 或黑屏。

## 8. 推荐的下一步操作

### 8.1 先检查新 APK

在仓库根目录执行：

```powershell
$apk = "out\QuestCraft-pico-private-20261028-0856.apk"
Get-Item $apk
Get-FileHash $apk -Algorithm SHA256
```

确认包内 mapper 和 26.3 native：

```powershell
jar tf $apk | Select-String "BundledLibraryNames341.class|liblwjgl341.so|liblwjgl_opengl341.so|liblwjgl_stb341.so"
```

若 `jar` 无法读取 APK，可以使用：

```powershell
& "D:\android-sdk\build-tools\<版本>\aapt2.exe" dump files $apk
```

### 8.2 覆盖安装，不要清数据

设备旧 APK 的 debug 签名 SHA-1 已与本机 `~/.android/debug.keystore` 匹配，因此应先尝试保留数据覆盖安装：

```powershell
adb devices
adb install -r "out\QuestCraft-pico-private-20261028-0856.apk"
adb shell dumpsys package com.qcxr.qcxr | Select-String "versionName|lastUpdateTime"
```

不要在没有必要时执行 `adb uninstall`、清除 app data 或删除实例，因为这些操作会破坏已有 Minecraft 世界和日志证据。

### 8.3 通过自动启动测试 26.2/26.3

代码内置测试入口，定义在：

- `Pojlib/src/main/java/pojlib/UnityPlayerActivity.java:109-112`
- `QCXR-XR-Wrapper/Assets/Scripts/JNIStorage.cs:152-189`

每次测试前建议：

```powershell
adb shell am force-stop com.qcxr.qcxr
adb logcat -c
adb shell am start -n com.qcxr.qcxr/pojlib.UnityPlayerActivity --es qcxr_autolaunch 26.2
```

26.3 使用：

```powershell
adb shell am force-stop com.qcxr.qcxr
adb logcat -c
adb shell am start -n com.qcxr.qcxr/pojlib.UnityPlayerActivity --es qcxr_autolaunch 26.3
```

采集日志时至少保留：

```powershell
adb logcat -v threadtime > out\device-26.2-YYYYMMDD-HHMM.log
```

另一次测试改成 `device-26.3-...log`。可重点搜索：

```text
AutoLaunch
prelaunch
LWJGL
SDL
mismatch
MobileGlues
Sodium
shader
Invalid #version
ZGC
Minor Collection
shutdown
Exception
UnsatisfiedLinkError
NoClassDefFoundError
```

### 8.4 验证 26.3 mapper 是否真的生效

需要同时满足：

1. 26.3 日志不再出现“加载旧版 LWJGL native”的 mismatch 警告；
2. 日志仍显示 SDL 3.4.1 runtime 正常初始化；
3. `/proc/<游戏进程 PID>/maps` 中能看到对应 `341` native，例如 `liblwjgl341.so`、`liblwjgl_opengl341.so` 或实际构建命名；
4. 26.2 仍然加载未加后缀的旧 native，不被 26.3 mapper 影响；
5. 进入世界、渲染方块、文字、保存和退出均正常。

设备 root 时可先找进程：

```powershell
adb shell pidof com.qcxr.qcxr
```

再根据实际 PID 检查：

```powershell
adb shell su -c "cat /proc/<PID>/maps | grep -E 'lwjgl|SDL|freetype|shaderc'"
```

### 8.5 采集 Minecraft/MobileGlues 文件

不要猜固定路径。先在 app 数据目录中查找：

```powershell
adb shell su -c "find /data/user/0/com.qcxr.qcxr/files -type f \( -name latest.log -o -name options.txt -o -name '*.log' \) 2>/dev/null"
```

重点拉取：

- Minecraft `latest.log`；
- `files/mg/latest.log`；
- `files/mg/config.json`；
- 对应实例的 `options.txt`；
- launcher 自己的日志。

### 8.6 头显内 VR UI 测试清单

必须在主屏和手表/重新 dock 后分别测试：

- 选择 26.2、选择 26.3；
- Play/启动按钮；
- 搜索框、RAM 输入框、实例编辑框；
- 空输入时连续按 Backspace；
- Mirror Natives；
- Import Saves；
- 登录/离线账户按钮；
- Skin/PFP 按钮；
- 返回、关闭、重启；
- UI redock 后文字是否镜像、上下/左右偏移；
- 射线指针位置是否与文字/按钮视觉位置一致；
- 修改 Minecraft GUI scale 或分辨率后重复点击测试。

每个失败都记录：实例版本、所在屏幕（主屏/手表）、头显视角、控制器/手势、是否有 Unity exception、是否有 Java/logcat 错误。

## 9. 重要已有根因与注意事项

### freetype

早期问题是把 Linux/glibc 的 freetype native 放进 Android，ELF 依赖包含 `libc.so.6`、`libm.so.6`、`ld-linux-aarch64.so.1`，Android Bionic 无法加载。当前路径已使用 Android/Bionic freetype，并在 `JREUtils` 中处理优先加载。不要重新引入 glibc freetype。

### Sodium/MobileGlues

26.2+ Sodium 不能简单永久禁用。当前 `ModCompat` 会先保留真实 jar，只有 MobileGlues 记录 shader failure 后才自动替换 inert stub。检查问题时先看 `Sodium.jar` 大小、`.orig` 备份和 `sodium_stub.txt`，不要只看文件名。

### Vulkan

PICO 4 的 Vulkan 1.1 不满足该 26.2 路径需要的扩展。优先 OpenGL + MobileGlues。不要重新在启动参数中强制追加 `--graphicsBackend vulkan`。

### 游戏设置持久化

之前的 bug 是启动时无条件重写 `options.txt`，并强制追加 Vulkan 参数。当前设计是只在配置缺失时写默认值，实例已有语言、后端、视频设置不应被覆盖。验证时必须“游戏内修改 → 正常退出 → 再次启动 → 检查 `options.txt` 和 UI”。

## 10. 交付标准

后续 AI 完成工作时，最终报告至少包含：

1. 使用的 APK 文件名、SHA-256、是否真的安装成功；
2. 26.2 和 26.3 分别的启动、进世界、保存、退出结果；
3. 26.3 是否仍有 native mismatch，实际加载了哪些 native；
4. prelaunch 分段时间和完整启动时间；
5. 关键 logcat/Minecraft/MobileGlues 错误及发生时间；
6. 头显中主屏/手表屏的文字与射线对齐结果；
7. 按钮逐项测试结果，不能只写“UI 正常”；
8. 是否修改了代码、修改了哪些文件、是否重新构建并验证；
9. 未解决问题和下一步，而不是把未验证项目标成通过。

本文件本身不代表代码已经提交，也不代表新 APK 已通过真机回归测试。
