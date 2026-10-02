param([string]$ModDll,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$ModDll){$ModDll=Join-Path $root 'build/pcvr/ddraw.dll'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build/pcvr/installer-disc-alpha23'}
$output=[IO.Path]::GetFullPath($OutputDirectory)
if(!$output.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Installer output must stay inside this checkout.'}
if(Test-Path -LiteralPath $output){throw 'Choose a fresh installer output directory.'}
$dll=(Resolve-Path -LiteralPath $ModDll).Path
$bytes=[IO.File]::ReadAllBytes($dll)
$pe=[BitConverter]::ToInt32($bytes,60)
if([BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x14c){throw 'Installer requires the x86 production DLL.'}
$strings=[Text.Encoding]::ASCII.GetString($bytes)
if(!$strings.Contains('probe_version=23 arch=x86') -or $strings.Contains('REPLAY_TEST')){throw 'Installer requires a production probe-23 DLL, never a synthetic replay build.'}
$payload=Join-Path $output 'payload'
New-Item -ItemType Directory -Path $payload | Out-Null
$files=[ordered]@{'build/pcvr/ddraw.dll'=$dll}
foreach($name in @('run-probe.ps1','save-session.ps1','graphics-quality.ps1','vr-settings.json')){$files["pcvr/$name"]=Join-Path $PSScriptRoot $name}
foreach($name in @('LICENSE','THIRD_PARTY.md','INSTALL.md')){
 $notice=Join-Path $PSScriptRoot "publication/$name"
 if(!(Test-Path -LiteralPath $notice)){$notice=Join-Path $root $name}
 $files[$name]=$notice
}
$manifest=[ordered]@{}
foreach($file in $files.GetEnumerator()){
 $target=Join-Path $payload $file.Key
 New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
 Copy-Item -LiteralPath $file.Value -Destination $target
 (Get-Item -LiteralPath $target).IsReadOnly=$false
 $manifest[$file.Key]=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
}
$manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payload 'package.json') -Encoding utf8
# Assemble only the explicit authored mod files. Never enumerate a staged game.
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$zip=Join-Path $output 'payload.zip'
$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
try{foreach($relative in @($files.Keys)+@('package.json')){[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $payload $relative),$relative,[IO.Compression.CompressionLevel]::Optimal)|Out-Null}}finally{$archive.Dispose()}
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if(!(Test-Path -LiteralPath $csc)){throw '.NET Framework x64 C# compiler not found.'}
$exe=Join-Path $output 'HotD2VR-Setup-0.2.1-alpha.23.exe'
$refs=@('/r:System.dll','/r:System.Core.dll','/r:System.Drawing.dll','/r:System.Windows.Forms.dll','/r:System.Web.Extensions.dll','/r:System.IO.Compression.dll','/r:System.IO.Compression.FileSystem.dll')
$source=@('InstallerCore.cs','DiscImport.cs','Setup.cs','AssemblyInfo.cs') | ForEach-Object {Join-Path $PSScriptRoot "installer/$_"}
& $csc /nologo /warn:4 /warnaserror+ /optimize+ /platform:x64 /target:winexe "/out:$exe" "/win32manifest:$PSScriptRoot/installer/setup.manifest" "/resource:$zip,HotD2VR.Payload" @refs @source
if($LASTEXITCODE){throw 'Installer compile failed.'}
$test=Join-Path $output 'InstallerTests.exe'
& $csc /nologo /warn:4 /warnaserror+ /optimize+ /platform:x64 /target:exe "/out:$test" @refs (Join-Path $PSScriptRoot 'installer/InstallerCore.cs') (Join-Path $PSScriptRoot 'installer/DiscImport.cs') (Join-Path $PSScriptRoot 'installer/InstallerTests.cs')
if($LASTEXITCODE){throw 'Installer tests compile failed.'}
[ordered]@{release='v0.2.1-alpha.23';probe=23;installer_sha256=(Get-FileHash -LiteralPath $exe).Hash.ToLowerInvariant();production_dll_sha256=$manifest['build/pcvr/ddraw.dll'];payload_files=$manifest;game_files_included=0;third_party_binaries_included=0} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'installer-package-report.json') -Encoding utf8
Write-Output "Built installer: $exe"
