# 构建

## 需要什么

| 工具 | 版本 | 用途 |
|---|---|---|
| Unity | 2022.3.22f1 | 打 APK（`QCXR-XR-Wrapper`） |
| JDK | 17 | 打 Pojlib AAR |
| JDK | 25+（本项目用 26） | 编译 Vivecraft 26.3（`minecraft_version=26.3`） |
| Android SDK | 含 build-tools 34、platform 35 | AAR / APK |
| Android NDK | 任意 r25+ | 只在**自己构建 freetype**时需要 |
| adb | - | 装机与日志 |

## 1. 取源码

```bash
git clone https://github.com/QuestCraftPlusPlus/Pojlib.git
git clone https://github.com/QuestCraftPlusPlus/QCXR-XR-Wrapper.git
git clone -b Multiloader-26.3 https://github.com/Vivecraft/VivecraftMod.git vc-26.3
```

打补丁（补丁前缀是 `a/Pojlib/...`、`a/QCXR-XR-Wrapper/...`，在各自仓库根目录用 `-p2`）：

```bash
cd Pojlib
git apply -p2 ../patches/pojlib-compat.patch
git apply -p2 ../patches/pojlib-modcompat.patch
cp ../src/pojlib/ModCompat.java src/main/java/pojlib/util/

cd ../QCXR-XR-Wrapper
git apply -p2 ../patches/wrapper-compat.patch
```

> 如果你的目录布局是"一个父目录里同时有 `Pojlib/` 和 `QCXR-XR-Wrapper/`"，则在父目录用 `-p1`。

## 2. freetype（唯一需要自己准备的原生库）

要 **Bionic / arm64 / freetype ≥ 2.13**。两种方式：

**A. 自己用 NDK 构建（推荐，可复现）**

```bash
curl -LO https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz
tar xf freetype-2.13.3.tar.xz && cd freetype-2.13.3
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 \
      -DBUILD_SHARED_LIBS=ON -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON \
      -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
cmake --build build -j
cp build/libfreetype.so <Pojlib>/src/main/jniLibs/arm64-v8a/libfreetype.so
```

**B. 从设备上已有的 Bionic 构建里取**（例如某些 Android 应用自带的 freetype），
用 ELF 解析确认 `NEEDED` 里没有 `libc.so.6 / ld-linux-aarch64.so.1` 即可。

> 校验一句话：`NEEDED` 必须是 `libm.so / libdl.so / libc.so`，`SONAME` 为 `libfreetype.so`。

## 3. Pojlib → AAR

```powershell
$env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot'
$env:PATH = 'C:\Program Files\Git\usr\bin;' + $env:PATH
$env:HOST_OS = 'windows'          # 本机 uname 报 MSYS_NT，NDK 的 init.mk 只认 MINGW64/CYGWIN
.\gradlew.bat assembleRelease --no-daemon
```

产物：`Pojlib/build/outputs/aar/Pojlib-release.aar` → 覆盖到
`QCXR-XR-Wrapper/Assets/Plugins/Android/Pojlib-release.aar`（`.meta` 不要动）。

## 4. Unity → APK

```powershell
.\scripts\build-apk.ps1 -Unity 'C:\Program Files\Unity 2022.3.22f1\Editor\Unity.exe' `
    -Project 'C:\src\QCXR-XR-Wrapper' -Output 'C:\out\QuestCraft-pico.apk' -Log 'C:\out\build.log'
```

脚本内部用 `Start-Process -Wait`，因为 Unity 是 GUI 子系统程序，`&` 不会等它。

## 5. 安装

```bash
adb install -r QuestCraft-pico.apk
adb shell dumpsys package com.qcxr.qcxr | grep lastUpdateTime   # 核实是否真的装上了
adb shell am start -n com.qcxr.qcxr/pojlib.UnityPlayerActivity
```

## 6.（可选）Vivecraft 26.3 Quest 版

```bash
cd vc-26.3
git apply ../patches/vivecraft-26.3-quest.patch

# 26.3 的 classpath 不再带 glfw，编译需要：
#   libs/lwjgl-glfw-3.4.1.jar        （compileOnly）
#   libs/lwjgl-openxr-android-3.3.3.jar（含 KHRAndroidCreateInstance，见下）
python ../scripts/inject-openxr-jar.py build/libs/vivecraft-26.3-1.3.17-fabric.jar libs/lwjgl-openxr-android-3.3.3.jar

JAVA_HOME=<jdk25+> ./gradlew :fabric:build --no-daemon
```

`lwjgl-openxr-android-3.3.3.jar` 不在 Maven：可从官方 QuestCraft APK 内嵌的
`META-INF/jars/lwjgl-openxr-android-3.3.3.jar` 提取（LWJGL 本体 BSD-3-Clause）。

产物放进 `Pojlib/src/main/assets/mods/vivecraft-26.3-1.3.17-quest.bin`，
并在 `mods.json` 里引用 `asset://mods/...`。

> 注意：即使 jar 编好了，**26.3 仍然跑不起来**——见 [KNOWN-ISSUES.md](KNOWN-ISSUES.md) #4（SDL）。
