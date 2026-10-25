# QuestCraft-PICO

在 **PICO 4 / PICO Neo3（Adreno 650）** 上把 QuestCraft（Minecraft: Java Edition 26.x）跑起来的一组适配补丁与踩坑记录。

本项目**只是补丁集与文档**，不包含 Minecraft 本体、资源、JRE、APK，也不包含任何绕过 Minecraft 账号授权的功能。正常使用需要你自己拥有 Minecraft: Java Edition。

## 现状

| 目标 | 状态 | 说明 |
|---|---|---|
| MC **26.2** + QuestCraft 6.0.1 在 PICO 4 启动 | ✅ 可玩 | 修掉 freetype 崩溃、临时禁用 Sodium、退出游戏自动回启动器 |
| MC **26.3** | ⚠️ 已移植，跑不起来 | Vivecraft 26.3 Quest 版已编译成功；但 MC 26.3 把平台层从 GLFW 换成 **SDL**，Android 侧没有 `org.lwjgl.sdl` 与其 native → 启动即 `NoClassDefFoundError: org/lwjgl/sdl/SDL` |

## 仓库内容

```
patches/
  pojlib-compat.patch         freetype 修复 + Sodium 兼容 + 26.x 启动参数
  pojlib-modcompat.patch      prelaunch 里挂上 Sodium 兼容处理（2 行）
  wrapper-compat.patch        InstanceButton 异常不再被吞（排查 launch 卡死用）
  vivecraft-26.3-quest.patch  Vivecraft 26.2-Quest 补丁移植到 26.3（相对上游 Multiloader-26.3）
src/pojlib/
  ModCompat.java              Sodium 置空逻辑（见 KNOWN-ISSUES #2）
scripts/
  build-aar.ps1               Pojlib → AAR
  build-apk.ps1               Unity CLI → APK
  inject-openxr-jar.py        把 Android OpenXR binding 注入 Vivecraft jar
  make-sodium-stub.py         生成 320 字节的空壳 Sodium.jar
docs/
  MC26-on-PICO.md             为什么这么改：逐条根因与验证方法
```

## 三条硬约束（先看这个再动手）

1. **freetype 必须是 Bionic 构建的 2.13+**。LWJGL 官方 `natives-linux-arm64` 里的 freetype 是 glibc（`NEEDED libc.so.6`），Android 加载不了；JRE 自带的那份只有 2.9，缺 LWJGL 3.3.6 要的 8 个符号。
2. **Sodium 在这台机器上必须禁用**。它的地形着色器用 `#version 330 core` + `isamplerBuffer`，MobileGlues 1.3.4 翻不了，会原样透传给高通编译器 → `Invalid #version` → 地形整块不渲染（能透视看到地下怪物）。代价是失去 Sodium 的优化。
3. **不要指望 Vulkan**。Adreno 650 没有 Vulkan 1.2 / `VK_KHR_dynamic_rendering`，`--graphicsBackend vulkan` 必然失败并回退到 OpenGL + MobileGlues。

详见 [KNOWN-ISSUES.md](KNOWN-ISSUES.md) 与 [docs/MC26-on-PICO.md](docs/MC26-on-PICO.md)。

## 快速开始

见 [BUILD.md](BUILD.md)。大方向：

1. `git clone` 上游 `QuestCraftPlusPlus/Pojlib` 与 `QuestCraftPlusPlus/QCXR-XR-Wrapper`；
2. 打 `patches/*.patch`，把 `src/pojlib/ModCompat.java` 放进去；
3. 用 NDK 构建（或从已安装应用中提取）一份 **Bionic freetype 2.13** 放进 `Pojlib/src/main/jniLibs/arm64-v8a/libfreetype.so`；
4. `build-aar.ps1` → 替换 wrapper 里的 AAR → `build-apk.ps1` → `adb install`。

## 许可

- 本仓库的补丁与脚本：**LGPL-3.0**（与上游 Pojlib / QCXR-XR-Wrapper 一致；见 [LICENSE](LICENSE)）。
- Vivecraft 补丁派生自 [Vivecraft/VivecraftMod](https://github.com/Vivecraft/VivecraftMod)（LGPL-3.0）与 [QuestCraftPlusPlus/MCXR](https://github.com/QuestCraftPlusPlus/MCXR)（MIT）的 OpenXR 集成。
- 第三方组件许可清单见 [NOTICE.md](NOTICE.md)。

Minecraft 是 Mojang/Microsoft 的专有软件，不在本仓库内，也不由本仓库分发。
