param(
    [int]$Seconds = 60,
    [string]$LogFile = "telemetria.log",
    [string]$Ares = (Join-Path $env:USERPROFILE 'Downloads\ares-v148\ares.exe')
)
Set-Location $PSScriptRoot
$rom = (Resolve-Path ".\ddsom_native_playtest.z64").Path

$proc = Start-Process -FilePath $Ares -ArgumentList "`"$rom`"" -PassThru -RedirectStandardOutput $LogFile -RedirectStandardError "telemetria_err.log"
Start-Sleep -Seconds $Seconds
if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force
}
