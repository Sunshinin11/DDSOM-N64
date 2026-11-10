param([string]$Rom='ddsom_native_playtest.z64',[string]$Label='test',[int]$Seconds=150,[switch]$Interactive)
$ErrorActionPreference='Stop'
$path=if([IO.Path]::IsPathRooted($Rom)){$Rom}else{Join-Path $PSScriptRoot $Rom}
$out=Join-Path $PSScriptRoot "build\five-$Label.log"
$err=Join-Path $PSScriptRoot "build\five-$Label-errors.log"
$p=Start-Process -FilePath (Join-Path $env:USERPROFILE 'Downloads\ares-v148\ares.exe') -ArgumentList ('"'+$path+'"') -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
$done=$false
try {
 $until=(Get-Date).AddSeconds($Seconds)
 while(!$p.HasExited -and (Get-Date) -lt $until){
  Start-Sleep -Seconds 1
  $text=if(Test-Path $out){Get-Content $out -Raw}else{''}
  if($text -match 'DDSOM_BENCH_DONE'){$done=$true;break}
  if($text -match 'ASSERTION FAILED|DDSOM_UNTRANSLATED|DDSOM_FATAL'){throw "Benchmark $Label failed; see $out"}
 }
} finally {if(!$p.HasExited){Stop-Process -Id $p.Id}}
Write-Output "BENCH label=$Label complete=$done log=$out"
Get-Content $out | Select-String 'DD_SOM_PERF.*frames=|DDSOM_CPU_HOT|DDSOM_PURE_COST' | Select-Object -Last 25
if($Interactive){
 if((Get-Content $out -Raw) -notmatch 'profile=0 benchmark_frames=0'){throw 'Interactive ROM was not built with live input'}
}elseif($Label -ne 'baseline' -and !$done){throw "Benchmark $Label did not complete in $Seconds seconds"}
