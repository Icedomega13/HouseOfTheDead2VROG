param(
    [Parameter(Mandatory=$true)][datetime]$StartedAt,
    [Nullable[int]]$GameExitCode=$null,
    [ValidateSet('headset','desktop','synthetic-replay')][string]$Mode='headset',
    [string]$GameDirectory,[object]$EffectiveCalibration=$null
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$game=Join-Path $root 'working/pcvr/game'
if($GameDirectory){$game=[IO.Path]::GetFullPath($GameDirectory);if(!$game.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Session game directory leaves this workspace.'}}
$session=Join-Path $root ("working/pcvr/sessions/{0}-{1}-{2}" -f $StartedAt.ToString('yyyyMMdd-HHmmss'),$Mode,([guid]::NewGuid().ToString('N').Substring(0,6)))
New-Item -ItemType Directory -Path $session | Out-Null
foreach($name in @('hotd2-ddraw-probe.log','pcvr-probe.ini','dgVoodoo.conf')) {
    $source=Join-Path $game $name
    if(Test-Path -LiteralPath $source) {Copy-Item -LiteralPath $source -Destination $session}
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'vr-settings.json') -Destination $session
$captures=@(Get-ChildItem -LiteralPath $game -Filter 'frame-*.bmp' -File | Where-Object {$_.LastWriteTime -ge $StartedAt})
$captures | Copy-Item -Destination $session
$log=Join-Path $session 'hotd2-ddraw-probe.log'
$diagnostics=@()
if(Test-Path -LiteralPath $log) {
    # Preserve the complete log, and summarize only its most recent process.
    $lines=@(Get-Content -LiteralPath $log)
    $begin=0
    for($i=0;$i -lt $lines.Count;$i++) {if($lines[$i] -match 'probe_version=') {$begin=$i}}
    # Get-Content strings carry PowerShell provider metadata. Copy their text
    # before JSON serialization so a line does not expand into a provider object.
    $diagnostics=@($lines | Select-Object -Skip $begin | Where-Object {$_ -match 'probe_version=|VR_CALIBRATION|VR_RELOAD|aim_down_reload|XR session created|XR recentered|XR eye_transfer|XR failure|XR frame lifecycle stopped|XR haptics|XR submitted_frame=|SCREEN_PLANE|RENDER_STATE|STEREO restore error|VR_LETTERBOX|NATIVE_CULL|VR_CULL|TIMING_RESEARCH|TEXTURE_PACK|TEXTURE_DUMP|OVERLAY_AUDIT|RHW_AUDIT|XYZ_QUAD_AUDIT|EFFECT_AUDIT|VR_HUD|VR_AIM_CURSOR|VR_SCREEN_EFFECT|VR_PISTOL'} | ForEach-Object {$_.ToString()})
}
[ordered]@{
    mode=$Mode; started_at=$StartedAt.ToString('o'); finished_at=(Get-Date).ToString('o'); game_exit_code=$GameExitCode;
    calibration=$(if($EffectiveCalibration){$EffectiveCalibration}else{Get-Content -LiteralPath (Join-Path $session 'vr-settings.json') -Raw | ConvertFrom-Json});
    captures=@($captures.Name); diagnostic_lines=$diagnostics;
    headset_mode_requested=($Mode -eq 'headset');
    physical_validation='Requires user play-test feedback; a launcher mode or synthetic run does not verify headset comfort.';
    note='Synthetic replay does not measure headset comfort or controller vibration. Eye-transfer timing includes surface lock, CPU conversion and swapchain upload/wait, not total motion-to-photon latency.'
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $session 'session.json') -Encoding utf8
Write-Output "Session evidence saved: $session"
