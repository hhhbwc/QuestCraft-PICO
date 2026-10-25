# MC 26.x 在 PICO 4 上的技术笔记

记录每一处结论是怎样验证的，方便以后复现或推给上游。

## 设备基线

| 项 | 值 |
|---|---|
| 头显 | PICO 4（PICOA8110，Android 10，已 root） |
| GPU | Adreno 650，GLES 3.2，Vulkan 1.1 |
| 图形栈 | OpenGL → MobileGlues 1.3.4（GL 4.0 / GLES 3.2） |
| 包 | `com.qcxr.qcxr`，Activity `pojlib.UnityPlayerActivity` |
| JRE | 内置 OpenJDK 25（`files/runtimes/JRE`） |

## 验证手段（都是离线可做的）

- **ELF 依赖**：解析 `.dynamic` 的 `SONAME/NEEDED`，`NEEDED` 里出现 `libc.so.6`/`ld-linux-aarch64.so.1` 就是 glibc 构建（Android 一定加载失败）。
- **符号比对**：从 `lwjgl-glfw-classes.jar` 的 `org/lwjgl/util/freetype/FreeType.class` 抽出 `FT_*` 函数名，与 `.so` 的 `.dynsym` 求差集——比"能 dlopen"更能说明能否真正使用。
- **Java 反汇编**：`javap -p -c` 读混淆前的 MC jar（loom 的 `minecraft-merged-deobf`），确认包名/方法签名是否变了。26.3 的 `GpuSurface` 从 `com.mojang.blaze3d.systems` 搬到了 `com.mojang.renderpearl.api.device`。
- **补丁基线定位**：`git log --diff-filter=D -- <path>` 找文件被删/改名的提交，取父提交做基线，能把"噪声补丁"从 44 个文件收敛到 20 个文件。
- **着色器**：`logcat` 里 `AdrenoGLES-0: ERROR: Invalid #version` 是 `/vendor/lib64/libllvm-qgl.so` 打的——说明源码没被翻译，直接透传到了高通编译器。

## 时间线（26.2）

1. `build16`：把 LWJGL 的 glibc freetype 塞进 jniLibs → 崩溃从 "找不到库" 变成 "加载失败(error = null)"。
2. `build17`：补诊断日志，发现异常被 `catch(Throwable){ printStackTrace(); }` 吃掉（只进 logcat，不进头显控制台）。
3. `build18`：换 Bionic freetype 2.13 + `preferredFreetype()`（RTLD_GLOBAL 只留一份）→ **JVM 起来了**，MC 26.2 能进世界。
4. `build19/22`：皮肤、退出重启、app 内换肤。
5. Sodium：尝试改 `#version`、删 `isamplerBuffer` 均无效 → 空壳禁用，走原版地形管线。

## 时间线（26.3）

1. 上游 `Multiloader-26.3`（1.3.17）存在、`26.3-1.3.17-fabric` 在 Modrinth 有；但 QuestCraft 用的是自己魔改的 OpenXR 版，必须自己移植。
2. 移植：以 `dee18e3` 为基线抽 20 文件补丁 → 打到 26.3；冲突集中在包改名（`GpuSurface`/`GpuFormat`/`CommandEncoder`/`GpuTextureView`）。
3. 编译通过、jar 内嵌 OpenXR Android binding（`KHRAndroidCreateInstance`）。
4. 实机：`NoClassDefFoundError: org/lwjgl/sdl/SDL` → **26.3 改成 SDL-only**，当前无解（见 KNOWN-ISSUES #4）。

## 还没验证的

- 26.3 在 SDL 层打通后的 mixin 运行时行为（Mixin 目标已静态核对：`Minecraft.renderFrame(boolean)`、`GpuSurface.{configure,isSuboptimal,acquireNextTexture,blitFromTexture}` 均在）。
- 关闭 Sodium 后 26.2 的长时间稳定性（区块加载/内存）。
