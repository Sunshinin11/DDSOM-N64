param(
    [int]$Seconds = 120,
    [string]$Ares = (Join-Path $env:USERPROFILE 'Downloads\ares-v148\ares.exe'),
    [switch]$ShowWindow
)
$ErrorActionPreference = 'Stop'
$rom = (Resolve-Path (Join-Path $PSScriptRoot 'ddsom_native_playtest.z64')).Path
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log = Join-Path $PSScriptRoot "build\native-$stamp.log"
$err = Join-Path $PSScriptRoot "build\native-$stamp-errors.log"
$style = if ($ShowWindow) { 'Normal' } else { 'Hidden' }
$proc = Start-Process -FilePath $Ares -ArgumentList ('"'+$rom+'"') -WindowStyle $style -PassThru -RedirectStandardOutput $log -RedirectStandardError $err
try {
    $until = (Get-Date).AddSeconds($Seconds)
    while (-not $proc.HasExited -and (Get-Date) -lt $until) { Start-Sleep -Seconds 1 }
} finally {
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id }
}
Get-FileHash -LiteralPath $rom -Algorithm SHA256
Write-Output "Log: $log"
Write-Output "Erros: $err"

