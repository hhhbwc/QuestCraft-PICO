# 已知问题与根因

按"症状 → 根因 → 处理"排列。根因都经过代码或日志核对；能在真机复现的会注明。

## 1. 点 Play 后卡在 `Modifying Vivecraft config`（或直接崩）

**根因**：`jniLibs/arm64-v8a/libfreetype.so` 放的是 LWJGL linux-arm64 的 **glibc** 构建：

```
NEEDED: libc.so.6, libm.so.6, ld-linux-aarch64.so.1
```

Android 是 Bionic，这三个都不存在 → `dlopen` 失败 → `UnsatisfiedLinkError: ... libfreetype.so (error = null)`。
注意 **文件存在 ≠ 能加载**；`build16` 曾误以为"放进去就修好了"。

**处理**：
- 用一份 **Bionic（NDK 构建）的 freetype 2.13.x**，`NEEDED` 只应有 `libm.so / libdl.so / libc.so`；
- JRE 自带的 freetype 是 Bionic 但只有 **2.9**，缺 `FT_Get_Transform`、`FT_Get_Color_Glyph_Paint`、`FT_GlyphSlot_Slant` 等 8 个 LWJGL 3.3.6 需要的符号，不能直接用；
- `JREUtils.dlopen` 是 `RTLD_GLOBAL`，会让旧 2.9 先进全局符号表劫持新库 → `preferredFreetype()` 改为优先加载 APK 里那份，保证进程内只有一份 freetype。

**自检**：解析 ELF `.dynamic` 的 `SONAME/NEEDED`，再与 `lwjgl-glfw-classes.jar` 中 `org/lwjgl/util/freetype/FreeType.class` 的函数名比对。

## 2. 进世界后方块不渲染（能透视看到地下实体）

**日志**：

```
Couldn't compile vertex shader (sodium:blocks/block_layer_opaque): ERROR: Invalid #version
→ sodium:pipeline/{solid,cutout,translucent}_terrain 全部 invalid
```

**根因**：Sodium 0.9 的地形着色器里有 `#ifdef VULKAN` + `layout(push_constant)`。MobileGlues **1.3.4** 的 glslang 预置头缺少 `#undef VULKAN`，解析到 Vulkan 专属分支失败，随后把桌面 GLSL **原样透传**给 `/vendor/lib64/libllvm-qgl.so`，于是出现 `Invalid #version`。

**处理**：升级到 MobileGlues **2.0.0**（1.3.5 起加入 `shader.setPreamble("#undef VULKAN\n")`，2.0.0 还修了 multidraw / 状态缓存 / GLSL cache），Sodium 用 0.9.2。`ModCompat` 在启动前写 `files/mg/config.json`（`enableNoError=2`、GLSL 缓存 128 MB），并轮询 `files/mg/latest.log`；若仍出现翻译/编译失败，自动恢复 320 字节空壳并写 `sodium_stub.txt`，下次启动回退原版地形管线。

**自检**：`strings libmobileglues.so | grep 'undef VULKAN'`（1.3.4 无、2.0.0 有）；空壳保留文件名（`updateModByType` 会检测 `!mod.exists()` 重新下载）。

## 3. Vulkan 后端在 PICO 4 上不可用

Adreno 650 只有 Vulkan 1.1，缺 `VK_KHR_synchronization2` 与 `VK_KHR_dynamic_rendering`。游戏会打印 `Failed to create backend Vulkan` 并回退 OpenGL + MobileGlues。
启动器**不再强制** `--graphicsBackend vulkan`，后端完全由 `options.txt` 的 `preferredGraphicsBackend` 决定；新实例默认 `opengl`。在游戏里选 Vulkan 会被保存，但这台硬件仍会回退——这是硬件限制，不是设置丢失。

## 4. MC 26.3 启动即崩：SDL

```
java.lang.NoClassDefFoundError: org/lwjgl/sdl/SDL
  at NativeLibrariesBootstrap.loadSdl
```

**根因**：26.3 的加载清单固定为 `LWJGL system / SDL / OpenGL / OpenAL / STB / freetype / shaderc / spvc / vma`；client jar 里 **0 处 GLFW 引用、26 处 SDL 引用**，没有回退。

还差三层：
1. `org.lwjgl.sdl` Java 类（Maven 有 `lwjgl-sdl`）；
2. **Android/Bionic 版 native**（LWJGL 的 `natives-linux-arm64` 里的 `libSDL3.so` 是 glibc）；
3. SDL3 的 Android 平台层（SDL 依赖它自己的 Activity/JNI 胶水，MC 是普通 Activity），并与 Pojlib 的 dummy SurfaceView、MobileGlues 的 EGL 上下文对齐。

Vivecraft 侧已经移植好（`patches/vivecraft-26.3-quest.patch`），SDL 这层通了就能接。
启动器侧不依赖 26.2 的修复（设置持久化、MobileGlues 配置、Sodium 回退）对 26.3 同样生效；26.3 只卡在 SDL 这一层。

## 5. 皮肤

- 启动器 PFP/小人偶原来按**用户名**查 `minotar.net`：离线名若与真实账号同名，会显示那个人的皮肤；
- 皮肤类型查询失败时原代码会落成 **slim** 模型（手臂错位），应默认 classic。

## 6. 退出游戏卡死

`JREUtils.launchJavaVM` 的 JVM 线程结束时不通知 Unity，而 Unity 在 OpenXR 交接时已经 `CloseXR()`，于是退出后没人给头显出图。现在 JVM 结束后调用 `CallbackBridge.restartUnitySession()` 重启启动器进程。

## 7. 构建环境（Windows）

- `Pojlib/build` 与 `Unity/Library|Temp` 里如果目录被打了 **ReadOnly** 属性，gradle 会 `AccessDeniedException`、Unity Player 构建会 0.4 秒瞬挂（bee exit code 3）。构建前批量清属性。
- Unity 是 GUI 子系统程序，PowerShell 的 `&` 不会等待 → 用 `Start-Process -Wait -PassThru` 才能拿到真实退出码。
- 26.3 的 Vivecraft 需要 **JDK 25+**（本项目用 JDK 26）与仓库自带 Gradle 9.3；首次 MinecraftMaven 约 8 分钟。

## 8. 游戏内设置不保存（语言 / 渲染后端 / 视频设置）

**根因**：`JREUtils.prelaunchCheck` 每次启动都会调用 `Installer.refreshLaunchCompatibilityAssets`，而它无条件重写 `options.txt`（`lang`、`preferredGraphicsBackend` 和所有视频/音频设置都在这个文件里）以及 Sodium 配置；同时 `generateLaunchArgs` 又固定追加 `--graphicsBackend vulkan`。两者叠加，玩家在游戏里改的每一项都会在下次启动被覆盖。

**处理**：

- `Installer` 改为 `copyAssetIfMissing`：配置只在**缺失**时写入默认值，重装/修复实例也不再覆盖；
- 删除强制的 `--graphicsBackend vulkan`，后端完全由 `options.txt` 决定；
- 新实例默认 `preferredGraphicsBackend:opengl`（PICO 4 跑不了 26.2 的 Vulkan 1.2，见 #3）。

**自检**：进游戏改语言 → 退出 → 检查 `instances/<版本>/options.txt` 里 `lang:` 是否保留；下次启动语言不变。

## 9. 中毒/反胃只有状态图标，没有画面效果

不是启动器 bug：Vivecraft 在 VR 里通过 `MinecraftVRMixin.vivecraft$noPostEffectVR` 主动关闭屏幕后处理（避免晕动），状态图标仍由原版 HUD 逻辑绘制。上游 QuestCraft 一直如此。

