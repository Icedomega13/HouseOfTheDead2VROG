$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sourceGame = Join-Path $projectRoot 'working/windows-game'
$probeDll = Join-Path $projectRoot 'build/pcvr/ddraw.dll'
$targetGame = Join-Path $projectRoot 'working/pcvr/game'
if (!(Test-Path -LiteralPath $probeDll)) { throw 'Build the probe first with pcvr\build.cmd.' }
if (!(Test-Path -LiteralPath (Join-Path $sourceGame 'Hod2.exe'))) { throw 'Restored PC game missing.' }
if (Test-Path -LiteralPath $targetGame) { throw 'Staged game already exists. Refusing to overwrite a test installation.' }
New-Item -ItemType Directory -Path $targetGame | Out-Null
Get-ChildItem -LiteralPath $sourceGame | Copy-Item -Destination $targetGame -Recurse
Copy-Item -LiteralPath $probeDll -Destination (Join-Path $targetGame 'ddraw.dll')
$sourceHash = (Get-FileHash -LiteralPath (Join-Path $sourceGame 'Hod2.exe') -Algorithm SHA256).Hash
$stagedHash = (Get-FileHash -LiteralPath (Join-Path $targetGame 'Hod2.exe') -Algorithm SHA256).Hash
if ($sourceHash -ne $stagedHash) { throw 'Staged executable hash mismatch.' }
Write-Output "Staged unchanged game and diagnostic probe at $targetGame"
