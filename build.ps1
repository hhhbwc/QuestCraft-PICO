<#
.SYNOPSIS
  QuestCraft-PICO 私有版（含离线账户 + 自定义皮肤）一键构建。

.DESCRIPTION
  这个目录是**已经改好的完整源码**，不需要打补丁、不需要 git：
    Pojlib/            Java 侧（离线档案、离线皮肤资源包、Sodium 兼容、freetype）
    QCXR-XR-Wrapper/   Unity 工程（离线账户 UI、Skin 按钮、键盘、退出重启）
  脚本只做三件事：编译 AAR → 覆盖进 Unity → 打 APK。

.EXAMPLE
  .\build.ps1
  .\build.ps1 -AndroidSdk 'D:\android-sdk' -Install
  .\build.ps1 -SkipAar          # 只改了 C# 时用
#>
[CmdletBinding()]
param(
    [string]$Unity = '',
    [string]$Jdk17 = '',
    [string]$AndroidSdk = '',
    [string]$OutDir = '',
    [string]$Adb = 'adb',
    [switch]$Install,
    [switch]$SkipAar
)

$ErrorActionPreference = 'Stop'
$Root = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $OutDir) { $OutDir = Join-Path $Root 'out' }

$PojlibDir = Join-Path $Root 'Pojlib'
$WrapDir   = Join-Path $Root 'QCXR-XR-Wrapper'

function Step($msg) { Write-Host "`n== $msg" -ForegroundColor Cyan }
function Warn($msg) { Write-Host "   ! $msg" -ForegroundColor Yellow }

function Find-First($candidates, $probe, $what) {
    foreach ($c in $candidates) {
        if ($c -and (Test-Path (Join-Path $c $probe))) { return $c }
    }
    throw "找不到 $what。请用参数显式指定，例如：-$(if ($what -eq 'Unity') {'Unity'} else {if ($what -eq 'Android SDK') {'AndroidSdk'} else {'Jdk17'}}) <路径>"
}

function Run-Native([string]$exe, [string[]]$arguments, [string]$workdir = '') {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $out = $null; $code = 0
    try {
        if ($workdir) { Push-Location $workdir }
        $out = & $exe @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        if ($workdir) { Pop-Location }
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ Code = $code; Out = (($out | Out-String).Trim()) }
}

function Clear-ReadOnly($path) {
    if (-not (Test-Path $path)) { return }
    $n = 0
    Get-ChildItem $path -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.Attributes -band [IO.FileAttributes]::ReadOnly) {
            $_.Attributes = $_.Attributes -band (-bnot [IO.FileAttributes]::ReadOnly); $n++
        }
    }
    if ($n -gt 0) { Warn "清除了 $n 个只读属性（$path）" }
}

# ---------------- 环境 ----------------
Step '检查环境'

if (-not $Unity) {
    foreach ($u in @('C:\Program Files\Unity 2022.3.22f1', 'C:\Program Files\Unity\Hub\Editor\2022.3.22f1')) {
        if (Test-Path (Join-Path $u 'Editor\Unity.exe')) { $Unity = $u; break }
    }
}
if (-not $Unity -or -not (Test-Path (Join-Path $Unity 'Editor\Unity.exe'))) {
    throw "找不到 Unity 2022.3.22f1，请用 -Unity 指定（到 ...\2022.3.22f1 这一级）"
}
$UnityExe = Join-Path $Unity 'Editor\Unity.exe'

if (-not $Jdk17) {
    $jdkCandidates = @()
    if ($env:JAVA_HOME) { $jdkCandidates += $env:JAVA_HOME }
    $jdkCandidates += (Get-ChildItem 'C:\Program Files\Eclipse Adoptium' -Directory -Filter 'jdk-17*' -ErrorAction SilentlyContinue | ForEach-Object FullName)
    $jdkCandidates += (Get-ChildItem 'C:\Program Files\Java' -Directory -Filter 'jdk-17*' -ErrorAction SilentlyContinue | ForEach-Object FullName)
    $Jdk17 = Find-First $jdkCandidates 'bin\java.exe' 'JDK 17'
}
if (-not (Test-Path (Join-Path $Jdk17 'bin\java.exe'))) { throw "JDK 路径无效: $Jdk17（参数 -Jdk17）" }

if (-not $AndroidSdk) {
    $sdks = @($env:ANDROID_HOME, $env:ANDROID_SDK_ROOT, 'D:\android-sdk',
              (Join-Path $env:LOCALAPPDATA 'Android\Sdk')) | Where-Object { $_ }
    $AndroidSdk = Find-First $sdks 'platform-tools' 'Android SDK'
}
if (-not (Test-Path (Join-Path $AndroidSdk 'platform-tools'))) { throw "Android SDK 路径无效: $AndroidSdk（参数 -AndroidSdk）" }

Write-Host "   Unity      : $Unity"
Write-Host "   JDK 17     : $Jdk17"
Write-Host "   Android SDK: $AndroidSdk"

# Gradle 需要 local.properties（每次构建都写，避免路径过期）
$escaped = $AndroidSdk.Replace('\', '\\').Replace(':', '\:')
[IO.File]::WriteAllText((Join-Path $PojlibDir 'local.properties'), "sdk.dir=$escaped`n", (New-Object Text.UTF8Encoding $false))

Clear-ReadOnly (Join-Path $PojlibDir 'build')
Clear-ReadOnly (Join-Path $WrapDir 'Library')
Clear-ReadOnly (Join-Path $WrapDir 'Temp')

# ---------------- AAR ----------------
if (-not $SkipAar) {
    Step '构建 Pojlib AAR'
    # Keep the SDL overlay jar in sync with its sources (tools/overlay341).
    $overlaySrc = Join-Path (Split-Path -Parent $Root) 'tools\overlay341\lwjgl-sdl-overlay-3.4.1.jar'
    $overlayDst = Join-Path $PojlibDir 'src\main\assets\components\lwjgl3\3.4.1\lwjgl-sdl-overlay-3.4.1.jar'
    if (Test-Path $overlaySrc) {
        Copy-Item $overlaySrc $overlayDst -Force
        Write-Host "   SDL overlay synced from tools/overlay341"
    } else {
        Warn 'tools/overlay341 overlay jar not found; packaging the committed copy'
    }
    $env:JAVA_HOME = $Jdk17
    $env:PATH      = (Join-Path $Jdk17 'bin') + ';' + $env:PATH
    $env:HOST_OS   = 'windows'
    $r = Run-Native (Join-Path $PojlibDir 'gradlew.bat') @('assembleRelease', '--no-daemon') $PojlibDir
    if ($r.Code -ne 0) { throw "gradle 失败（exit $($r.Code)）`n$($r.Out)" }

    $aar = Join-Path $PojlibDir 'build\outputs\aar\Pojlib-release.aar'
    if (-not (Test-Path $aar)) { throw "没找到 AAR 产物: $aar" }
    Copy-Item $aar (Join-Path $WrapDir 'Assets\Plugins\Android\Pojlib-release.aar') -Force
    Write-Host ("   AAR {0:N1} MB 已覆盖到 Unity" -f ((Get-Item $aar).Length / 1MB))
} else {
    Warn '跳过 AAR（-SkipAar），Unity 里用的是上次的 AAR'
}

# ---------------- APK ----------------
Step '构建 APK（首次需导入工程，可能 20-40 分钟；之后 1-5 分钟）'
New-Item -ItemType Directory -Force $OutDir | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmm'
$apk = Join-Path $OutDir "QuestCraft-pico-private-$stamp.apk"
$log = Join-Path $OutDir "build-$stamp.log"

# Unity 是 GUI 子系统程序：补环境变量 + Start-Process -Wait
$env:LOCALAPPDATA = "$env:USERPROFILE\AppData\Local"
$env:APPDATA      = "$env:USERPROFILE\AppData\Roaming"
$env:ALLUSERSPROFILE = 'C:\ProgramData'
$env:PROGRAMDATA  = 'C:\ProgramData'
$env:PUBLIC       = 'C:\Users\Public'
$env:USERDOMAIN   = $env:COMPUTERNAME
$env:OS           = 'Windows_NT'
$env:ComSpec      = "$env:SystemRoot\system32\cmd.exe"
$env:TMP          = $env:TEMP
$env:PROCESSOR_ARCHITECTURE = 'AMD64'

$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $UnityExe -ArgumentList @(
    '-batchmode', '-quit', '-nographics',
    '-projectPath', $WrapDir,
    '-executeMethod', 'QuestCraftCliBuild.BuildAndroid',
    '-outputPath', $apk,
    '-logFile', $log,
    '-accept-apiupdate') -Wait -PassThru
$sw.Stop()

if ($p.ExitCode -ne 0 -or -not (Test-Path $apk)) {
    Warn "Unity 退出码 $($p.ExitCode)，日志: $log"
    throw 'APK 构建失败'
}

Step '完成'
Write-Host ("   {0}`n   {1:N1} MB，耗时 {2:N1} 分钟" -f $apk, ((Get-Item $apk).Length / 1MB), $sw.Elapsed.TotalMinutes)

if ($Install) {
    Step '安装到头显'
    $r = Run-Native $Adb @('install', '-r', $apk)
    if ($r.Code -ne 0) { Warn "adb install 返回 $($r.Code)（大 APK 常报 exec_process_tree_termination_unconfirmed，实际可能已成功）`n$($r.Out)" }
    $r = Run-Native $Adb @('shell', 'dumpsys', 'package', 'com.qcxr.qcxr')
    ($r.Out -split "`n" | Select-String 'lastUpdateTime') | ForEach-Object { Write-Host "   $_" }
}
