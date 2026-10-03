param([switch]$VR,[switch]$Mono,[ValidateRange(0,100)][double]$EyeSeparation=6.4,[ValidateRange(1,10000)][int]$UnitsPerMetre=10,[ValidateRange(400,1600)][int]$EyeSize=800,[ValidateRange(-45,45)][double]$GunPitchDegrees=15,[bool]$Haptics=$true,[ValidateRange(0,200)][int]$HapticStrength=100,[bool]$AimDownReload=$true,[ValidateRange(35,85)][int]$DownReloadDegrees=55,[ValidateSet(-1,0,2,4,8,16)][int]$Antialiasing=-1,[ValidateRange(0,16)][int]$AnisotropicFiltering=0,[bool]$SuppressLetterbox=$true,[bool]$HeadsetVisibility=$true,[bool]$HideUnusedPlayerTwo=$true,[bool]$AimingCursor=$true,[bool]$AmmoGauges=$true,[bool]$HealthGauge=$false,[switch]$DualWield,[switch]$IndependentMagazines,[string]$BuildDirectory,[string]$GameDirectory,[switch]$TexturePack,[switch]$EffectAudit,[string]$DiscImage,[string]$SettingsFile)
$ErrorActionPreference = 'Stop'
if($IndependentMagazines -and (!$VR -or !$DualWield)){throw 'Independent magazines require -VR -DualWield.'}
if ($VR -and $Mono) {throw '-VR and -Mono cannot be combined.'}
. (Join-Path $PSScriptRoot 'graphics-quality.ps1')
if($VR) {
    $profilePath=Join-Path $PSScriptRoot 'vr-settings.json'
    if($SettingsFile){$profilePath=(Resolve-Path -LiteralPath $SettingsFile).Path}
    $profile=Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
    if(!$PSBoundParameters.ContainsKey('EyeSize')) {$EyeSize=[int]$profile.EyeSize}
    if(!$PSBoundParameters.ContainsKey('UnitsPerMetre')) {$UnitsPerMetre=[int]$profile.UnitsPerMetre}
    if(!$PSBoundParameters.ContainsKey('GunPitchDegrees') -and $null -ne $profile.GunPitchDegrees) {$GunPitchDegrees=[double]$profile.GunPitchDegrees}
    if(!$PSBoundParameters.ContainsKey('Haptics') -and $null -ne $profile.Haptics) {
        if($profile.Haptics -isnot [bool]) {throw 'Haptics must be true or false in pcvr/vr-settings.json.'}
        $Haptics=$profile.Haptics
    }
    if(!$PSBoundParameters.ContainsKey('HapticStrength') -and $null -ne $profile.HapticStrength) {
        if($profile.HapticStrength -isnot [int] -and $profile.HapticStrength -isnot [long]) {throw 'HapticStrength must be an integer percentage from 0 to 200.'}
        $HapticStrength=[int]$profile.HapticStrength
        if($HapticStrength -lt 0 -or $HapticStrength -gt 200) {throw 'HapticStrength must be from 0 to 200.'}
    }
    if(!$PSBoundParameters.ContainsKey('AimDownReload') -and $null -ne $profile.AimDownReload) {
        if($profile.AimDownReload -isnot [bool]) {throw 'AimDownReload must be true or false in pcvr/vr-settings.json.'}
        $AimDownReload=$profile.AimDownReload
    }
    if(!$PSBoundParameters.ContainsKey('DownReloadDegrees') -and $null -ne $profile.DownReloadDegrees) {$DownReloadDegrees=[int]$profile.DownReloadDegrees}
    if(!$PSBoundParameters.ContainsKey('Antialiasing') -and $null -ne $profile.Antialiasing) {$Antialiasing=[int]$profile.Antialiasing}
    if(!$PSBoundParameters.ContainsKey('AnisotropicFiltering') -and $null -ne $profile.AnisotropicFiltering) {$AnisotropicFiltering=[int]$profile.AnisotropicFiltering}
    if(!$PSBoundParameters.ContainsKey('SuppressLetterbox') -and $null -ne $profile.SuppressLetterbox) {
        if($profile.SuppressLetterbox -isnot [bool]) {throw 'SuppressLetterbox must be true or false.'}
        $SuppressLetterbox=$profile.SuppressLetterbox
    }
    if(!$PSBoundParameters.ContainsKey('HeadsetVisibility') -and $null -ne $profile.HeadsetVisibility) {
        if($profile.HeadsetVisibility -isnot [bool]) {throw 'HeadsetVisibility must be true or false.'}
        $HeadsetVisibility=$profile.HeadsetVisibility
    }
    if(!$PSBoundParameters.ContainsKey('HideUnusedPlayerTwo') -and $null -ne $profile.HideUnusedPlayerTwo) {
        if($profile.HideUnusedPlayerTwo -isnot [bool]) {throw 'HideUnusedPlayerTwo must be true or false.'}
        $HideUnusedPlayerTwo=$profile.HideUnusedPlayerTwo
    }
    if(!$PSBoundParameters.ContainsKey('AimingCursor') -and $null -ne $profile.AimingCursor) {
        if($profile.AimingCursor -isnot [bool]) {throw 'AimingCursor must be true or false.'}
        $AimingCursor=$profile.AimingCursor
    }
    if(!$PSBoundParameters.ContainsKey('AmmoGauges') -and $null -ne $profile.AmmoGauges) {
        if($profile.AmmoGauges -isnot [bool]) {throw 'AmmoGauges must be true or false.'}
        $AmmoGauges=$profile.AmmoGauges
    }
    if(!$PSBoundParameters.ContainsKey('HealthGauge') -and $null -ne $profile.HealthGauge) {
        if($profile.HealthGauge -isnot [bool]) {throw 'HealthGauge must be true or false.'}; $HealthGauge=$profile.HealthGauge
    }
    foreach($flag in @('DualWield','IndependentMagazines')) {
        if(!$PSBoundParameters.ContainsKey($flag) -and $null -ne $profile.$flag) {
            if($profile.$flag -isnot [bool]) {throw "$flag must be true or false."}
            Set-Variable -Name $flag -Value ([bool]$profile.$flag)
        }
    }
    if($Antialiasing -notin @(-1,0,2,4,8,16) -or $AnisotropicFiltering -lt 0 -or $AnisotropicFiltering -gt 16){throw 'Invalid graphics-quality settings.'}
    if($EyeSize -lt 400 -or $EyeSize -gt 1600 -or $UnitsPerMetre -lt 1 -or $UnitsPerMetre -gt 10000 -or $GunPitchDegrees -lt -45 -or $GunPitchDegrees -gt 45 -or $DownReloadDegrees -lt 35 -or $DownReloadDegrees -gt 85) {throw 'Invalid VR calibration in pcvr/vr-settings.json.'}
}
$projectRoot = Split-Path $PSScriptRoot -Parent
$targetGame = Join-Path $projectRoot 'working/pcvr/game'
$build=Join-Path $projectRoot 'build/pcvr'
if($GameDirectory){$targetGame=[IO.Path]::GetFullPath($GameDirectory)}
if($BuildDirectory){$build=[IO.Path]::GetFullPath($BuildDirectory)}
foreach($path in @($targetGame,$build)){if(!$path.StartsWith($projectRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Launch directory leaves this workspace.'}}
if(Get-Process -Name Hod2 -ErrorAction SilentlyContinue) {throw 'Close the running HOTD2 game before launching another session.'}
if($IndependentMagazines -and (Get-FileHash -LiteralPath (Join-Path $targetGame 'Hod2.exe')).Hash -ne 'C6B4116788B7F68C56860FB9CC8A94BF984E620907031E4BC3DB43623DBE579A'){throw 'Independent magazines currently support the verified original English Hod2.exe only.'}
if (!(Test-Path -LiteralPath (Join-Path $targetGame 'ddraw.dll'))) { throw 'Run pcvr\stage.ps1 first.' }
foreach($runtimeFile in @('ddraw.dll','openxr_loader.dll')) {
    Copy-Item -LiteralPath (Join-Path $build $runtimeFile) -Destination (Join-Path $targetGame $runtimeFile) -Force
    (Get-Item -LiteralPath (Join-Path $targetGame $runtimeFile)).IsReadOnly=$false
}
Set-Hotd2GraphicsQuality -ConfigPath (Join-Path $targetGame 'dgVoodoo.conf') -Antialiasing $Antialiasing -AnisotropicFiltering $AnisotropicFiltering
@('[Stereo]',"Enabled=$([int](!$Mono))","SeparationMilliunits=$([int]($EyeSeparation*1000))",'[OpenXR]',"Enabled=$([int][bool]$VR)","UnitsPerMetre=$UnitsPerMetre","EyeSize=$EyeSize","GunPitchMilliDegrees=$([int]($GunPitchDegrees*1000))","Haptics=$([int]$Haptics)","HapticStrength=$HapticStrength","AimDownReload=$([int]$AimDownReload)","DownReloadDegrees=$DownReloadDegrees","SuppressLetterbox=$([int]$SuppressLetterbox)","HeadsetVisibility=$([int]$HeadsetVisibility)","HideUnusedPlayerTwo=$([int]$HideUnusedPlayerTwo)","AimingCursor=$([int]$AimingCursor)","AmmoGauges=$([int]$AmmoGauges)","HealthGauge=$([int]$HealthGauge)","DualWield=$([int][bool]$DualWield)","IndependentMagazines=$([int][bool]$IndependentMagazines)","EffectAudit=$([int][bool]$EffectAudit)",'[Textures]',"Enabled=$([int][bool]$TexturePack)",'Dump=0') |
    Set-Content -LiteralPath (Join-Path $targetGame 'pcvr-probe.ini') -Encoding ascii
$discPath=$null
if($DiscImage) {
    $discPath=(Resolve-Path -LiteralPath $DiscImage).Path
} else {
    $defaultDisc=Join-Path $projectRoot 'working/intake/windows-data.iso'
    if(Test-Path -LiteralPath $defaultDisc) {$discPath=(Resolve-Path -LiteralPath $defaultDisc).Path}
}
# Without an image, use the original game's own physical/mounted-disc check.
$mountedHere=$false
$startedAt=Get-Date
$gameExitCode=$null
try {
    if ($discPath -and !(Get-DiskImage -ImagePath $discPath).Attached) {
        Mount-DiskImage -ImagePath $discPath -StorageType ISO -Access ReadOnly | Out-Null
        $mountedHere=$true
    }
    # Visible game is intentional: this launcher is for an interactive desktop/headset test.
    $game=Start-Process -FilePath (Join-Path $targetGame 'Hod2.exe') -WorkingDirectory $targetGame -Wait -PassThru
    $gameExitCode=$game.ExitCode
    if ($game.ExitCode -ne 0) { Write-Warning "Game exited with code $($game.ExitCode)." }
} finally {
    if($mountedHere) {Dismount-DiskImage -ImagePath $discPath | Out-Null}
    try {
        $mode=if($VR) {'headset'} else {'desktop'}
        $effective=[pscustomobject]@{EyeSize=$EyeSize;UnitsPerMetre=$UnitsPerMetre;GunPitchDegrees=$GunPitchDegrees;Haptics=$Haptics;HapticStrength=$HapticStrength;AimDownReload=$AimDownReload;DownReloadDegrees=$DownReloadDegrees;Antialiasing=$Antialiasing;AnisotropicFiltering=$AnisotropicFiltering;SuppressLetterbox=$SuppressLetterbox;HeadsetVisibility=$HeadsetVisibility;HideUnusedPlayerTwo=$HideUnusedPlayerTwo;AimingCursor=$AimingCursor;AmmoGauges=$AmmoGauges;HealthGauge=$HealthGauge;DualWield=[bool]$DualWield;IndependentMagazines=[bool]$IndependentMagazines;TexturePack=[bool]$TexturePack;EffectAudit=[bool]$EffectAudit}
        & (Join-Path $PSScriptRoot 'save-session.ps1') -StartedAt $startedAt -GameExitCode $gameExitCode -Mode $mode -GameDirectory $targetGame -EffectiveCalibration $effective
    } catch {Write-Warning "Could not save session diagnostics: $($_.Exception.Message)"}
}
Write-Output "Probe log: $targetGame\hotd2-ddraw-probe.log"
