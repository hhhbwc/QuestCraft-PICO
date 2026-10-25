# 已知问题与根因

按"症状 → 根因 → 处理"排列。每条都经过实机验证。

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

**根因**：`Invalid #version` 来自 `/vendor/lib64/libllvm-qgl.so`（高通 GLES 编译器），说明 **MobileGlues 没有翻译这段着色器，把桌面 GLSL 原样透传**。已排除的假设：改 `#version 330 core → 330`、删 `isamplerBuffer`（buffer texture）——都无效。

**处理**：`ModCompat` 在 prelaunch 把 `mods/Sodium.jar` 换成 320 字节空壳（原文件备份为 `Sodium.jar.orig`），让游戏走自带的 `minecraft:pipeline/*` 地形管线。

**代价**：区块构建单线程、剔除/排序更差 → 帧数与区块加载速度下降。
**注意**：不能用"改名/删除"的方式禁用——`Instance.updateModByType` 会检测 `!mod.exists()` 并重新下载。空壳保留文件名即可。

## 3. `--graphicsBackend vulkan` 一定失败

Adreno 650 不支持 Vulkan 1.2、缺 `VK_KHR_synchronization2` 与 `VK_KHR_dynamic_rendering`。游戏会打印 `Failed to create backend Vulkan` 并回退 OpenGL + MobileGlues，属正常现象。

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

## 5. 皮肤

- 启动器 PFP/小人偶原来按**用户名**查 `minotar.net`：离线名若与真实账号同名，会显示那个人的皮肤；
- 皮肤类型查询失败时原代码会落成 **slim** 模型（手臂错位），应默认 classic。

## 6. 退出游戏卡死

`JREUtils.launchJavaVM` 的 JVM 线程结束时不通知 Unity，而 Unity 在 OpenXR 交接时已经 `CloseXR()`，于是退出后没人给头显出图。现在 JVM 结束后调用 `CallbackBridge.restartUnitySession()` 重启启动器进程。

## 7. 构建环境（Windows）

- `Pojlib/build` 与 `Unity/Library|Temp` 里如果目录被打了 **ReadOnly** 属性，gradle 会 `AccessDeniedException`、Unity Player 构建会 0.4 秒瞬挂（bee exit code 3）。构建前批量清属性。
- Unity 是 GUI 子系统程序，PowerShell 的 `&` 不会等待 → 用 `Start-Process -Wait -PassThru` 才能拿到真实退出码。
- 26.3 的 Vivecraft 需要 **JDK 25+**（本项目用 JDK 26）与仓库自带 Gradle 9.3；首次 MinecraftMaven 约 8 分钟。
