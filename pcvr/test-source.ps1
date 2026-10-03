param([string]$BuildDirectory,[switch]$WindowsInput,[string]$GpuBackend)
$ErrorActionPreference='Stop'
if(!$BuildDirectory) {$BuildDirectory=Join-Path (Split-Path $PSScriptRoot -Parent) 'build/pcvr'}
$BuildDirectory=(Resolve-Path -LiteralPath $BuildDirectory).Path
$backend=$null
if($GpuBackend) {$backend=(Resolve-Path -LiteralPath $GpuBackend).Path}
function Invoke-Check([string]$Name,[string[]]$Arguments=@()) {
    $exe=Join-Path $BuildDirectory "$Name.exe"
    if(!(Test-Path -LiteralPath $exe)) {throw "Missing test: $exe. Run pcvr/build.cmd first."}
    & $exe @Arguments
    if($LASTEXITCODE -ne 0) {throw "$Name failed with exit code $LASTEXITCODE."}
}
Push-Location -LiteralPath $BuildDirectory
try {
    foreach($name in @('xr_math_tests','xr_bridge_tests','reload_gesture_tests','native_ammo_tests','xr_pixels_tests','render_state_tests','pistol_mesh_tests','hud_prompt_tests','hud_cache_tests')) {Invoke-Check $name}
    # Unique scratch file; never overwrite a user's texture artwork.
    $png=Join-Path $BuildDirectory ('texture-check-'+[guid]::NewGuid().ToString('N')+'.png')
    try {Invoke-Check 'texture_image_tests' @($png)} finally {if(Test-Path -LiteralPath $png){Remove-Item -LiteralPath $png}}
    if($WindowsInput) {Invoke-Check 'input_bridge_tests'}
    if($backend) {Invoke-Check 'gun_render_tests' @($backend)}
} finally {Pop-Location}
Write-Output 'Selected source checks passed. These do not establish physical headset comfort or performance.'
