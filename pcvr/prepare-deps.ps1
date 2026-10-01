$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$deps=Join-Path $root 'working/pcvr/deps'
New-Item -ItemType Directory -Force -Path $deps | Out-Null
function Get-VerifiedFile($url,$path,$expected) {
    if(!(Test-Path -LiteralPath $path)) {Invoke-WebRequest -Uri $url -OutFile $path}
    $actual=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if($actual -ne $expected.ToLowerInvariant()) {throw "SHA256 mismatch: $path"}
}
$dg='dgVoodoo2_87_5.zip'
Get-VerifiedFile "https://github.com/dege-diosg/dgVoodoo2/releases/download/v2.87.5/$dg" (Join-Path $deps $dg) '5ffde6927f7355ca3fdd5d785b581256a8e6539fa13e395a891ade6ba1040850'
$xr='openxr_loader_windows-1.1.63.zip'
Get-VerifiedFile "https://github.com/KhronosGroup/OpenXR-SDK-Source/releases/download/release-1.1.63/$xr" (Join-Path $deps $xr) '01c631aeabbfe0879540f77ef833416c532a20746285b494630160c23588b771'
Expand-Archive -LiteralPath (Join-Path $deps $xr) -DestinationPath (Join-Path $deps 'openxr-1.1.63') -Force
foreach($name in @('openxr.h','openxr_platform.h','openxr_platform_defines.h')) {
    if(!(Test-Path -LiteralPath (Join-Path $deps "openxr-1.1.63/include/openxr/$name"))) {throw "SDK header missing from verified archive: $name"}
}
Write-Output 'Verified dgVoodoo 2.87.5 and OpenXR 1.1.63 dependencies.'
