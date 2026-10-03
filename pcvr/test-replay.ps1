param([ValidateRange(15,90)][int]$Seconds=35,[string]$BuildDirectory,[string]$GameDirectory,
    [ValidateRange(400,1600)][int]$EyeSize=800,[ValidateSet(-1,0,2,4,8,16)][int]$Antialiasing=-1,
    [ValidateRange(0,16)][int]$AnisotropicFiltering=0,[bool]$SuppressLetterbox=$true,
    [ValidateRange(-180,180)][int]$YawDegrees=0,[switch]$Passive,[bool]$HeadsetVisibility=$true,
    [switch]$TimingAudit,[ValidateSet(0,60,90,120)][int]$NativeHz=0,
    [switch]$TextureDump,[switch]$TexturePack,[switch]$NoVSync,[switch]$OverlayAudit,[switch]$Combat,[switch]$EffectAudit,[bool]$HideUnusedPlayerTwo=$true,[bool]$AimingCursor=$true,[bool]$AmmoGauges=$true,[bool]$HealthGauge=$false,[switch]$CursorToggle,[switch]$DualWield,[switch]$IndependentMagazines,[string]$SettingsFile)
$ErrorActionPreference='Stop'
if($IndependentMagazines -and !$DualWield){throw 'Independent magazines require -DualWield.'}
$root=Split-Path $PSScriptRoot -Parent
$game=Join-Path $root 'working/pcvr/game'
$build=Join-Path $root 'build/pcvr'
if($GameDirectory){$game=[IO.Path]::GetFullPath($GameDirectory)}
if($BuildDirectory){$build=[IO.Path]::GetFullPath($BuildDirectory)}
foreach($path in @($game,$build)){if(!$path.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Replay directory leaves this workspace.'}}
. (Join-Path $PSScriptRoot 'graphics-quality.ps1')
if(Get-Process Hod2 -ErrorAction SilentlyContinue) {throw 'Close HOTD2 before running the replay test.'}
$normal=Join-Path $build 'ddraw.dll'
$replay=Join-Path $build 'replay/ddraw.dll'
if(!(Test-Path -LiteralPath $normal) -or !(Test-Path -LiteralPath $replay)) {throw 'Build both build.cmd and build-replay.cmd first.'}
$disc=(Resolve-Path -LiteralPath (Join-Path $root 'working/intake/windows-data.iso')).Path
$mounted=$false
$startedAt=Get-Date
$evidence=Join-Path $root 'working/pcvr/captures/controller-replay'
$profilePath=Join-Path $PSScriptRoot 'vr-settings.json'
if($SettingsFile){$profilePath=(Resolve-Path -LiteralPath $SettingsFile).Path}
$profile=Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
if(!$PSBoundParameters.ContainsKey('AimingCursor') -and $null -ne $profile.AimingCursor){
 if($profile.AimingCursor -isnot [bool]){throw 'AimingCursor must be true or false.'};$AimingCursor=$profile.AimingCursor
}
if(!$PSBoundParameters.ContainsKey('AmmoGauges') -and $null -ne $profile.AmmoGauges){
 if($profile.AmmoGauges -isnot [bool]){throw 'AmmoGauges must be true or false.'};$AmmoGauges=$profile.AmmoGauges
}
if(!$PSBoundParameters.ContainsKey('EyeSize')){$EyeSize=$profile.EyeSize}
$profile.EyeSize=$EyeSize
if(!$PSBoundParameters.ContainsKey('HealthGauge') -and $null -ne $profile.HealthGauge){
 if($profile.HealthGauge -isnot [bool]){throw 'HealthGauge must be true or false.'};$HealthGauge=$profile.HealthGauge
}
$profile | Add-Member -NotePropertyName HealthGauge -NotePropertyValue $HealthGauge -Force
$profile | Add-Member -NotePropertyName AimingCursor -NotePropertyValue $AimingCursor -Force
$profile | Add-Member -NotePropertyName AmmoGauges -NotePropertyValue $AmmoGauges -Force
$profile | Add-Member -NotePropertyName ReplayCursorToggle -NotePropertyValue ([bool]$CursorToggle) -Force
$profile | Add-Member -NotePropertyName DualWield -NotePropertyValue ([bool]$DualWield) -Force
$profile | Add-Member -NotePropertyName IndependentMagazines -NotePropertyValue ([bool]$IndependentMagazines) -Force
$profile | Add-Member -NotePropertyName HideUnusedPlayerTwo -NotePropertyValue $HideUnusedPlayerTwo -Force
$profile | Add-Member -NotePropertyName Antialiasing -NotePropertyValue $Antialiasing -Force
$profile | Add-Member -NotePropertyName AnisotropicFiltering -NotePropertyValue $AnisotropicFiltering -Force
$profile | Add-Member -NotePropertyName HeadsetVisibility -NotePropertyValue $HeadsetVisibility -Force
$profile | Add-Member -NotePropertyName SuppressLetterbox -NotePropertyValue $SuppressLetterbox -Force
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
try {
 Copy-Item -LiteralPath $replay -Destination (Join-Path $game 'ddraw.dll') -Force
 (Get-Item -LiteralPath (Join-Path $game 'ddraw.dll')).IsReadOnly=$false
 Set-Hotd2GraphicsQuality -ConfigPath (Join-Path $game 'dgVoodoo.conf') -Antialiasing $Antialiasing -AnisotropicFiltering $AnisotropicFiltering
 @('[Stereo]','Enabled=1','SeparationMilliunits=6400','[OpenXR]','Enabled=1',"UnitsPerMetre=$($profile.UnitsPerMetre)","EyeSize=$($profile.EyeSize)","GunPitchMilliDegrees=$([int]($profile.GunPitchDegrees*1000))","Haptics=$([int][bool]$profile.Haptics)","HapticStrength=$($profile.HapticStrength)","AimDownReload=$([int][bool]$profile.AimDownReload)","DownReloadDegrees=$($profile.DownReloadDegrees)","SuppressLetterbox=$([int]$SuppressLetterbox)","HeadsetVisibility=$([int]$HeadsetVisibility)","HideUnusedPlayerTwo=$([int]$HideUnusedPlayerTwo)","AimingCursor=$([int]$AimingCursor)","ReplayCursorToggle=$([int][bool]$CursorToggle)") | Set-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 @("ReplayYawDegrees=$YawDegrees","ReplayPassive=$([int][bool]$Passive)") | Add-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 @("ReplayOverlayAudit=$([int][bool]$OverlayAudit)") | Add-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 @("ReplayCombat=$([int][bool]$Combat)") | Add-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 @("DualWield=$([int][bool]$DualWield)","IndependentMagazines=$([int][bool]$IndependentMagazines)") | Add-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 @("AmmoGauges=$([int]$AmmoGauges)","HealthGauge=$([int]$HealthGauge)") | Add-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 @("EffectAudit=$([int][bool]$EffectAudit)") | Add-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 @("ReplayTimingAudit=$([int][bool]$TimingAudit)","ReplayNativeHz=$NativeHz") | Add-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 @("ReplayNoVSync=$([int][bool]$NoVSync)",'[Textures]',"Dump=$([int][bool]$TextureDump)","Enabled=$([int][bool]$TexturePack)") | Add-Content -LiteralPath (Join-Path $game 'pcvr-probe.ini') -Encoding ascii
 $profile | Add-Member -NotePropertyName TextureDump -NotePropertyValue ([bool]$TextureDump) -Force
 $profile | Add-Member -NotePropertyName TexturePack -NotePropertyValue ([bool]$TexturePack) -Force
 $profile | Add-Member -NotePropertyName ReplayNoVSync -NotePropertyValue ([bool]$NoVSync) -Force
 $profile | Add-Member -NotePropertyName ReplayTimingAudit -NotePropertyValue ([bool]$TimingAudit) -Force
 $profile | Add-Member -NotePropertyName ReplayNativeHz -NotePropertyValue $NativeHz -Force
 $profile | Add-Member -NotePropertyName ReplayYawDegrees -NotePropertyValue $YawDegrees -Force
 $profile | Add-Member -NotePropertyName ReplayPassive -NotePropertyValue ([bool]$Passive) -Force
 $profile | Add-Member -NotePropertyName ReplayOverlayAudit -NotePropertyValue ([bool]$OverlayAudit) -Force
 $profile | Add-Member -NotePropertyName ReplayCombat -NotePropertyValue ([bool]$Combat) -Force
 $profile | Add-Member -NotePropertyName EffectAudit -NotePropertyValue ([bool]$EffectAudit) -Force
 if(!(Get-DiskImage -ImagePath $disc).Attached) {Mount-DiskImage -ImagePath $disc -StorageType ISO -Access ReadOnly | Out-Null;$mounted=$true}
 & (Join-Path $build 'startup_trace.exe') (Join-Path $game 'Hod2.exe') $Seconds --visible-game |
     Set-Content -LiteralPath (Join-Path $evidence 'startup-trace.txt')
 if($LASTEXITCODE -ne 0) {throw "Replay debugger failed with exit code $LASTEXITCODE. See startup-trace.txt."}
 Get-ChildItem -LiteralPath $game -Filter 'frame-*.bmp' | Copy-Item -Destination $evidence
 Get-Content -LiteralPath (Join-Path $game 'hotd2-ddraw-probe.log') -Tail 600 | Set-Content -LiteralPath (Join-Path $evidence 'probe-tail.log')
 & (Join-Path $PSScriptRoot 'save-session.ps1') -StartedAt $startedAt -Mode synthetic-replay -GameDirectory $game -EffectiveCalibration $profile
} finally {
 Copy-Item -LiteralPath $normal -Destination (Join-Path $game 'ddraw.dll') -Force
 (Get-Item -LiteralPath (Join-Path $game 'ddraw.dll')).IsReadOnly=$false
 if($mounted) {Dismount-DiskImage -ImagePath $disc | Out-Null}
}
Write-Output "Synthetic controller replay evidence: $evidence. Normal VR DLL restored."
