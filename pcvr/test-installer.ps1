param([string]$BuildDirectory,[string]$TestDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$BuildDirectory){$BuildDirectory=Join-Path $root 'build/pcvr/installer-disc-alpha23'}
if(!$TestDirectory){$TestDirectory=Join-Path $root ('working/pcvr/installer-tests-'+[guid]::NewGuid().ToString('N'))}
foreach($path in @($BuildDirectory,$TestDirectory)){if(![IO.Path]::GetFullPath($path).StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Test paths must stay inside this checkout.'}}
& (Join-Path $BuildDirectory 'InstallerTests.exe') $TestDirectory (Join-Path $root 'working/pcvr/deps') (Join-Path $BuildDirectory 'HotD2VR-Setup-0.2.1-alpha.23.exe')
if($LASTEXITCODE){throw 'Installer lifecycle checks failed.'}
