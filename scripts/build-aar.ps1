param(
    [Parameter(Mandatory=$true)][string]$Pojlib,
    [string]$Jdk = 'C:\Program Files\Eclipse Adoptium\jdk-17.0.20.8-hotspot'
)
$env:JAVA_HOME = $Jdk
$env:PATH      = 'C:\Program Files\Git\usr\bin;' + $Jdk + '\bin;' + $env:PATH
$env:HOST_OS   = 'windows'   # NDK 只认 MINGW64/CYGWIN，MSYS_NT 需直设
Push-Location $Pojlib
& .\gradlew.bat assembleRelease --no-daemon
$code = $LASTEXITCODE
Pop-Location
if ($code -ne 0) { exit $code }
Get-Item (Join-Path $Pojlib 'build\outputs\aar\Pojlib-release.aar') | Format-List FullName, Length
