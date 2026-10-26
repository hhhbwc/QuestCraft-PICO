# QuestCraft-PICO

**PICO 4 上的 Minecraft: Java Edition VR** —— 基于 QuestCraft 架构的 PICO 适配版：
Unity 启动器 + Pojlib（Java 启动链）+ Vivecraft OpenXR + MobileGlues GL 翻译层，
支持 **Minecraft 26.3（SDL 渲染后端）与 26.2 及更早版本（GLFW/LWJGL 3.3.x）**，
按实例自动切换，互不污染。

> **授权前提**：正常使用仍需你拥有 Minecraft: Java Edition。
> 离线登录（Offline Login）仅用于绕过 PICO 环境下的微软授权流程，不提供盗版。

---

## 特性

### 渲染后端
- **26.3：SDL 渲染后端**（LWJGL 3.4.1 Java + SDL3 3.4.14，按实例隔离安装，不污染全局 jniLibs）
- **26.2 及以下：GLFW/LWJGL 3.3.6 合并包**（原路径，行为不变）
- **MobileGlues 双版本隔离**：26.2 用 MobileGlues 2.0.0（Sodium 0.9.2 已启用）；
  26.3 用 `libmobileglues263.so`（MobileGlues dev 分支构建，修复 uniform block 被
  ESS 转换截断导致的黑屏/星点问题），OpenXR 的 EGL 入口共用同一套驱动栈
- OpenXR 手柄/头显追踪走 Quest 版 Vivecraft（26.3-1.3.17），菜单跟随面部

### 启动器
- **离线账户（Offline Login）**：与 vanilla/PCL2CE 兼容的离线 UUID 规则，档案落盘、完全离线可用
- **离线自定义皮肤**：本地选图 / PFP / 体型判定，覆盖 MC 默认皮肤
- VR 键盘直打运行时输入框（16 字封顶），账号下拉框风格统一

### 性能与稳定性
- **Java 25 运行时（26.3）启用 ZGC** 并发回收，GC 停顿不可感知
- **JVM 堆按版本封顶**（26.3 ≤ 3GB）：避免与 Unity/OpenXR 合成器争抢 8GB 物理内存引发换页
- **着色器转换三级缓存**：MC 进程内 → MobileGlues 磁盘缓存（跨启动）→ Adreno 驱动缓存；
  首次进世界约有 30 秒管线编译预热期（MC 26.3 管线惰性编译的固有行为），之后 10%–400%
  渲染分辨率档位下均无编译卡顿
- **启动链优化**：mod 库 SHA1 本地缓存（跳过每库一次的网络往返）、资产索引快速路径、
  六阶段启动耗时打点（见 launcher 日志 `prelaunch:` 行）

---

## 环境要求

| 需要 | 版本 | 说明 |
|---|---|---|
| Unity | **2022.3.22f1** + Android Build Support | 工程写死版本，必须一致 |
| JDK | **17** | 编译 Pojlib AAR |
| Android SDK | build-tools 34 / platform 35 | AAR 打包 |
| NDK | 26.1.10909125 | Pojlib JNI（SDL 钩子等） |
| 网络 | 首次需要 | Gradle 依赖 + Unity 包导入 |
| adb | 可选 | `-Install` 时用 |

## 构建

```powershell
.\build.ps1                 # AAR + APK
.\build.ps1 -SkipAar        # 只改了 C# 时，省掉 AAR 步骤
.\build.ps1 -Install        # 构建完顺便 adb install -r
```

脚本自动在常见位置查找 Unity / JDK17 / Android SDK，找不到就显式传参：

```powershell
.\build.ps1 -Unity 'C:\Program Files\Unity 2022.3.22f1' `
            -Jdk17 'C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot' `
            -AndroidSdk 'D:\android-sdk'
```

产物：`out\QuestCraft-pico-private-<时间>.apk`（Unity 日志同目录）。

- 首次构建 20–40 分钟（Unity 导入工程 + 生成 `Library` + 联网拉包）；
- 之后改 Java 约 1–4 分钟；只改 C# 约 1–2 分钟。

> 外部依赖：`build.ps1` 会在仓库父目录寻找 `tools\overlay341\lwjgl-sdl-overlay-3.4.1.jar`
> （SDL overlay 的最新构建）；找不到时自动使用 `Pojlib/src/main/assets/components/`
> 内已提交的副本，不影响构建。

---

## 目录结构

```
Pojlib/                      Java 侧（Gradle 工程，输出 AAR）
  src/main/java/pojlib/        启动链：Installer（版本化 LWJGL）、JREUtils（JVM 参数/GC）、
                               UnityPlayerActivity（SDL Surface 生命周期）、ModCompat（Sodium 守护）
  src/main/java/org/libsdl/    SDL Android 胶水（基于 Amethyst 移植，见 NOTICE.md）
  src/main/jni/native_hooks/   bytehook 钩子：SDL_InitSubSystem、libSDL3 JNI_OnLoad 守卫
  src/main/jni/include/SDL3/   SDL3 头文件
  src/main/assets/components/  按实例隔离的 LWJGL 3.4.1（Java + Bionic natives）与 overlay
  src/main/assets/mods/        Quest 版 Vivecraft（26.2 / 26.3 两套）
  src/main/assets/runtimes/    内置 JRE-25（26.3 用）
  src/main/jniLibs/arm64-v8a/  libmobileglues.so(2.0.0) / libmobileglues263.so(dev 修复版) /
                               libSDL3.so / libbytehook.so / libshadowhook.so / 3.3.6 natives
QCXR-XR-Wrapper/             Unity 工程（Assets / Packages / ProjectSettings）
  Assets/Scripts/LoginHandler.cs     离线账户 UI
  Assets/Scripts/SkinHandler.cs      本地皮肤 / 系统选图 / slim-classic 判定
  Assets/Scripts/InstanceButton.cs   启动链（异常不再被吞）
docs/KNOWN-ISSUES.md         每一处兼容修复的根因与验证方法
docs/patches/                上游修复的独立补丁（如 MobileGlues GLSL-ES 修复）
build.ps1                    一键构建（AAR → Unity → APK）
```

---

## 常改的地方

| 想改什么 | 改哪里 |
|---|---|
| 26.3/26.2 版本判定 | `Pojlib/src/main/java/pojlib/install/Installer.java` → `isSdlVersion` |
| 26.3 JVM 参数 / GC / 堆上限 | `Pojlib/src/main/java/pojlib/util/JREUtils.java` → `launchJavaVM` |
| SDL 着色器钩子 | `Pojlib/src/main/jni/native_hooks/sdl_hook.c` |
| 眼缓冲采样率 / 菜单跟随 | `Pojlib/src/main/java/pojlib/util/json/MinecraftInstances.java` → `CheckVivecraftConfig` |
| 离线名字规则 / UUID | `Pojlib/src/main/java/pojlib/API.java` → `loginOffline` |
| 启动器整体 UI | `QCXR-XR-Wrapper/Assets/Scripts/UIHandler.cs`、`WindowHandler.cs` 与 `Assets/Scenes/Main.unity` |

---

## 常见问题

- **刚进世界攻击/渲染顿一下（约 30 秒）**：MC 26.3 管线惰性编译的固有行为（三层缓存首次填充），
  之后所有渲染分辨率档位均流畅；可在视频设置里按需调整"渲染分辨率"滑条（10%–400%）。
- **Sodium 地形仍黑**：启动器检测到着色器失败会自动把 Sodium 换回 320B 空壳并在
  `files/sodium_stub.txt` 留标记；删掉标记文件即可重试。26.2/26.3 均如此。
- **gradle 报 AccessDeniedException**：目录被打了只读属性（备份工具常见），脚本启动时自动清；
  仍失败手动清 `Pojlib\build`。Unity 立刻失败（bee exit code 3）同理清 `Library`、`Temp`。
- **启动即崩、日志提到 `libfreetype.so`**：jniLibs 里的 freetype 被换成了 glibc 构建。
  必须是 Bionic（`NEEDED` 只有 `libm/libdl/libc`）。
- **换机器构建后不能覆盖安装**：默认 Debug 签名用本机 debug.keystore，签名不同。
  备份 `/sdcard/Android/data/com.qcxr.qcxr/files`（实例/皮肤/账户）后卸载重装再拷回。
  想统一签名 / 开 Release 优化，见 `docs/` 与 `build.ps1` 中注释（`-release -keystorePath ...`）。

---

## 致谢与许可

- **QuestCraft / PojavLauncher** 启动链与 JRE 集成（Apache-2.0 / LGPL-2.0）
- **Vivecraft** VR 渲染与 OpenXR 集成（LGPL-3.0）
- **MobileGlues** GL 翻译层（LGPL-2.1）及 dev 分支的 GLSL-ES 转换修复
- **Amethyst-Android** SDL 胶水移植参照（LGPL-3.0）
- **LWJGL**（BSD-3）、**SDL3**（zlib）、**bytehook**（Apache-2.0）、
  **OpenAL Soft**（LGPL-2.1）、**SPIRV-Cross**（Apache-2.0）
- 本仓库许可证见 [LICENSE](LICENSE)；第三方归属明细见 [NOTICE.md](NOTICE.md)
  与 `Pojlib/NOTICE.md`
