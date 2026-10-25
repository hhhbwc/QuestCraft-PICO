param(
    [Parameter(Mandatory=$true)][string]$Unity,
    [Parameter(Mandatory=$true)][string]$Project,
    [Parameter(Mandatory=$true)][string]$Output,
    [string]$Log = "$Output.log"
)
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

$p = Start-Process -FilePath $Unity -ArgumentList @(
    '-batchmode','-quit','-nographics',
    '-projectPath', $Project,
    '-executeMethod','QuestCraftCliBuild.BuildAndroid',
    '-outputPath', $Output,
    '-logFile', $Log,
    '-accept-apiupdate') -Wait -PassThru
Write-Host ("unity exit = " + $p.ExitCode)
if ($p.ExitCode -ne 0) { exit $p.ExitCode }
Get-Item $Output | Format-List Name, Length, LastWriteTime
