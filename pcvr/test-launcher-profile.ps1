param([string]$SettingsFile)
$ErrorActionPreference='Stop'
if(!$SettingsFile){$SettingsFile=Join-Path $PSScriptRoot 'vr-settings-release.json';if(!(Test-Path $SettingsFile)){$SettingsFile=Join-Path $PSScriptRoot 'vr-settings.json'}}
$root=Split-Path $PSScriptRoot -Parent
$fixture=Join-Path $root ('working/pcvr/launcher-profile-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $fixture|Out-Null
Copy-Item (Join-Path $PSScriptRoot 'graphics-quality.ps1') $fixture
# Execute the actual production parameter/profile-loading block, stopping before
# staging or starting a game. This test needs neither game files nor a headset.
$source=Get-Content (Join-Path $PSScriptRoot 'run-probe.ps1') -Raw
$boundary=$source.IndexOf('$projectRoot =')
if($boundary -lt 0){throw 'Cannot locate launcher staging boundary.'}
$probe=Join-Path $fixture 'profile-only.ps1'
$source.Substring(0,$boundary)+'[pscustomobject]@{Dual=[bool]$DualWield;Independent=[bool]$IndependentMagazines;Health=$HealthGauge;Ammo=$AmmoGauges;Cursor=$AimingCursor;Eye=$EyeSize}'|Set-Content $probe -Encoding utf8
Copy-Item $SettingsFile (Join-Path $fixture 'vr-settings.json')
$checks=0
function Check($ok,$name){if(!$ok){throw "FAIL $name"};$script:checks++;Write-Output "PASS $name"}
$fresh=& $probe -VR
Check ($fresh.Dual -and $fresh.Independent) 'packaged profile activates both guns and independent magazines without extra launch flags'
Check ($fresh.Health -and $fresh.Ammo -and !$fresh.Cursor) 'packaged profile enables status panels and hidden aiming dots'
Check ($fresh.Eye -eq 1200) 'accepted eye resolution retained'
$override=& $probe -VR -DualWield:$false -IndependentMagazines:$false -HealthGauge:$false -AimingCursor:$true
Check (!$override.Dual -and !$override.Independent -and !$override.Health -and $override.Cursor) 'explicit command-line options override profile preferences'
$profile=Get-Content $SettingsFile -Raw|ConvertFrom-Json
foreach($flag in @('DualWield','IndependentMagazines')){
 $bad=$profile.PSObject.Copy();$bad.$flag='true';$bad|ConvertTo-Json|Set-Content (Join-Path $fixture 'vr-settings.json')
 $rejected=$false;try{& $probe -VR|Out-Null}catch{$rejected=$_.Exception.Message -like "$flag must be true or false*"}
 Check $rejected "invalid $flag type is rejected before staging"
}
Write-Output "PASS $checks production launcher profile checks (no game or headset)"
