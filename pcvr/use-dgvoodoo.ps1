$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$package=Join-Path $root 'working/pcvr/deps/dgVoodoo2_87_5'
$game=Join-Path $root 'working/pcvr/game'
$archive=Join-Path $root 'working/pcvr/deps/dgVoodoo2_87_5.zip'
$expected='5ffde6927f7355ca3fdd5d785b581256a8e6539fa13e395a891ade6ba1040850'
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'Unexpected dgVoodoo archive hash.' }
# Pin extraction to the verified archive, not a potentially edited unpacked DLL.
$verified=Join-Path $root 'working/pcvr/deps/verified-2.87.5'
Expand-Archive -LiteralPath $archive -DestinationPath $verified -Force
Copy-Item -LiteralPath (Join-Path $verified 'MS/x86/DDraw.dll') -Destination (Join-Path $game 'ddraw_backend.dll')
Copy-Item -LiteralPath (Join-Path $verified 'MS/x86/D3DImm.dll') -Destination (Join-Path $game 'D3DImm.dll')
$config=Get-Content (Join-Path $verified 'dgVoodoo.conf') -Raw
$config=$config -replace '(?m)^OutputAPI\s*=.*$', 'OutputAPI = d3d11_fl11_0'
$config=$config -replace '(?m)^FullScreenMode\s*=.*$', 'FullScreenMode = false'
$config=$config -replace '(?m)^CaptureMouse\s*=.*$', 'CaptureMouse = false'
$config | Set-Content -LiteralPath (Join-Path $game 'dgVoodoo.conf') -Encoding ascii
Write-Output 'Configured local dgVoodoo backend: D3D11, windowed, no mouse capture.'
