# 第三方组件与许可

本仓库**不包含**下列二进制；它们只在你自己构建的 APK 中出现，分发 APK 时请各自遵守其许可证。

| 组件 | 许可证 | 说明 |
|---|---|---|
| [Pojlib](https://github.com/QuestCraftPlusPlus/Pojlib) | LGPL-3.0 | 本仓库补丁的目标之一 |
| [QCXR-XR-Wrapper](https://github.com/QuestCraftPlusPlus/QCXR-XR-Wrapper) | LGPL-3.0 | 同上 |
| [Vivecraft](https://github.com/Vivecraft/VivecraftMod) | LGPL-3.0 | 本仓库的 26.3 补丁是其派生作品 |
| [MCXR](https://github.com/QuestCraftPlusPlus/MCXR) | MIT | QuestCraft 的 OpenXR 集成来源 |
| LWJGL | BSD-3-Clause | `lwjgl-openxr-android` 为其 Android 构建 |
| freetype | FTL / GPLv2（双许可） | 需自行构建 Bionic 版 |
| MobileGlues | LGPL-2.1 | 随 APK 分发时注意其许可（与 GPLv3 不兼容） |
| OpenAL Soft | LGPL-2.1-or-later | |
| shaderc / SPIRV-Cross | Apache-2.0 | |
| OpenJDK（内置 JRE） | GPLv2 + Classpath Exception | 通常作为独立程序随包分发 |

## 明确不包含

- **Minecraft: Java Edition 本体、assets、Mojang 映射** —— 专有，按 Mojang EULA 需要你自己拥有游戏；
- 任何 APK 成品、`.so` 二进制、JRE 二进制；
- 任何绕过 Minecraft 账号授权的功能。

## 本仓库代码

LGPL-3.0，见 [LICENSE](LICENSE)。因为 Pojlib 与 QCXR-XR-Wrapper 本身是 LGPL-3.0，基于它们的补丁自动以 LGPL-3.0 授权。
