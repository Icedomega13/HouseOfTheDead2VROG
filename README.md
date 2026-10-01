# HotD2VR — The House of the Dead 2 PCVR mod

[![Windows source build](https://github.com/Icedomega13/HouseOfTheDead2VROG/actions/workflows/build.yml/badge.svg)](https://github.com/Icedomega13/HouseOfTheDead2VROG/actions/workflows/build.yml)

An open-source OpenXR mod for the **original Windows PC release** of The House of
the Dead 2, developed with Quest 3 over Virtual Desktop. It runs the original game
logic and intercepts DirectDraw / Direct3D 7 rendering to produce tracked stereo
views and controller input. It is a PCVR mod, not a standalone Quest application
or a rebuild of the game's proprietary source.

**Current release: v0.1.0-alpha.18 (HD Preview 3 / probe 18).** This is a source-only
alpha: build instructions are below. Version 17 received positive headset
feedback; version 18 adds locally tested gun rendering and cutscene reload fixes
and still needs physical feedback. See [changes and known issues](CHANGELOG.md).

The development candidate on this branch is **probe 19**, adding adjustable
vibration and cancellation requests on focus/tracking loss. It still needs a
physical headset test; the tagged alpha 18 source remains available separately.

## Features

- Head tracking and separate OpenXR eye rendering at 1200x1200 per eye by default.
- Right-controller aiming, tracked procedural pistol and green aiming dot.
- Controller menu navigation, left-stick recentering, trigger firing and B reload.
- Aim-down reload with a guard for observed cinematic bars.
- Higher-resolution rendering, 4x MSAA, 8x anisotropic filtering and PNG texture
  replacement support. No HD texture pack is included in the public release.
- Suppression of observed cinematic bars, broader observed visibility queries,
  defensive render-state restoration and controller focus-loss handling.

The original game owns rail movement, collisions, shooting and game speed. A naive
120 FPS unlock accelerated gameplay in local experiments, so production retains
the original timing. Independent high-rate rendering is future work.

## Requirements

- Windows x64 with a Direct3D 11-capable GPU. The game and mod are **32-bit x86**.
- Your own installed copy of the original PC game and its required media. The
  tested `Hod2.exe` SHA-256 is
  `c6b4116788b7f68c56860fb9cc8a94bf984e620907031e4bc3db43623dbe579a`.
  Other executable revisions, the modern remake and emulated console versions
  have not been validated; executable-specific hooks may not work on them.
- Visual Studio 2022 or newer / Build Tools with **Desktop development with C++**,
  MSVC x86/x64 tools and a Windows SDK. The build auto-discovers the installation.
- An OpenXR runtime supporting **32-bit applications** and a connected headset.
  Quest 3 through Virtual Desktop / VirtualDesktopXR is the tested development
  setup. Configure the provider's active runtime before launch. Other runtimes
  are unverified.
- Internet access for the separately downloaded, SHA-256-verified OpenXR 1.1.63
  SDK/loader and dgVoodoo2 2.87.5 backend. See [third-party notices](THIRD_PARTY.md).

## Build and set up

Run these commands from the repository root. `-ExecutionPolicy Bypass` applies to
that PowerShell process; it does not change the system execution policy.

```powershell
git clone https://github.com/Icedomega13/HouseOfTheDead2VROG.git
cd HouseOfTheDead2VROG
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\pcvr\prepare-deps.ps1
.\pcvr\build.cmd
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\pcvr\test-source.ps1
```

Copy your installed game's files into **`working/windows-game/`**, with
`Hod2.exe` directly inside that folder and its data folders alongside it. Do not
copy just the executable. Then:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\pcvr\stage.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\pcvr\use-dgvoodoo.ps1
```

Staging creates `working/pcvr/game/`, verifies the copied executable's hash and
refuses to overwrite an existing staged game. It does not change the original
installation. Builds go to `build/pcvr/`; dependencies, game copies, texture packs
and session logs stay under ignored local directories.

Connect the headset, start your PCVR streaming connection, then double-click
**`pcvr/play-vr.cmd`** (equivalently `pcvr/play-source-vr.cmd`). This launches the
normal source build with the saved quality profile and optional texture loading.
Close the game before building/staging or launching another session.

Use your physical or already-mounted disc as required by the original game. To
mount your own ISO read-only for the duration of a session:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\pcvr\run-probe.ps1 -VR -TexturePack -DiscImage 'D:\MyGames\HOTD2.iso'
```

If `working/intake/windows-data.iso` exists, the launcher uses it by default.
Otherwise it leaves disc handling to the game. It dismounts an image only when
this launcher mounted it. No game download or disc-check bypass is supplied.

For a desktop diagnostic, run `run-probe.ps1` without `-VR`; add `-Mono` for the
ordinary desktop view. The desktop stereo preview is intentionally low resolution
and is separate from the higher-resolution headset image. If the runtime reports
no available headset, reconnect it and restart the game.

## Controls and settings

| Control | Action |
|---|---|
| Right controller | Aim the pistol |
| Right trigger | Fire |
| B | Reload; also retains the game's deliberate cutscene-skip behavior |
| Lower the barrel | Automatic reload when armed and outside detected cinematics |
| A | Start / confirm; another press may be needed after the title transition |
| Left stick directions | Menu navigation |
| Left stick click | Recenter while facing your intended forward direction |
| X | Escape / back |

Aim-down reload uses the calibrated barrel: at least 55 degrees down for 80 ms
sends a 120 ms native pulse, with a 500 ms cooldown. Raise above 35 degrees down
to rearm. Holding the gun down does not repeatedly reload. The cinematic guard
covers observed bar signatures, not every possible native cutscene.

Edit `pcvr/vr-settings.json`, then restart. Defaults are `EyeSize: 1200`,
`UnitsPerMetre: 10`, `GunPitchDegrees: 15`, `Antialiasing: 4` and
`AnisotropicFiltering: 8`. Larger units per metre make the world appear smaller.
Positive gun pitch lowers the barrel and shooting ray. If performance suffers,
try an eye size of 1000 or 800 and lower AA. `AimDownReload`, `Haptics`,
`SuppressLetterbox` and `HeadsetVisibility` are switches. Explicit parameters to
`run-probe.ps1` override the profile. This does not enable a frame-rate unlock.

In the probe 19 candidate, `HapticStrength` is an integer percentage from 0 to
200: 100 preserves the existing pulse feel, 50 halves amplitude, 200 doubles it
and 0 is silent. Trigger/reload pulses acknowledge input; they do not yet detect
actual native shots, ammo refill, hits or damage. Strength and focus-loss behavior
need physical testing before release.

## Texture packs

Put separately supplied or authored replacement PNGs in
`working/pcvr/game/hd-textures/` and launch with `-TexturePack` (the VR shortcut
already does this). Missing or invalid replacements retain native textures.
See [the pack format](pcvr/texture-packs/README.md). This release includes the
loader and authored pistol, but no original textures or private derivative HD
sample. Higher eye resolution, AA and filtering work without a texture pack.

## Validation and limitations

Builds use MSVC x86 `/W4 /WX`; the production DLL uses `/O2`. Source tests cover
math, the production OpenXR bridge with mocks, reload timing, pixel conversion,
render-state restoration, procedural geometry and Windows PNG decoding/hashing.
The first public release adds Windows build/test CI without a game or headset.

Optional tests on a Windows desktop with the backend installed:

```powershell
Copy-Item .\working\pcvr\game\D3DImm.dll .\build\pcvr\D3DImm.dll
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\pcvr\test-source.ps1 -WindowsInput -GpuBackend .\working\pcvr\game\ddraw_backend.dll
```

The backend's `D3DImm.dll` companion must also be beside the GPU test executable.
These optional checks need an interactive Windows desktop; restricted or headless
sessions can deny cursor access. GPU results depend on the actual desktop/driver.
Native-game replay tools are research tools
requiring your own staged game and ISO at `working/intake/windows-data.iso`:
build them with `pcvr/build-replay.cmd` and run `pcvr/test-replay.ps1` only when
the game is closed. Do not use their test DLL for ordinary play.

Known issues: occasional hit-effect misplacement; brief residual black bars;
missing backfaces or unloaded scenery when turning far from the original camera;
and scenes not covered by the reload guard. A complete playthrough, broad runtime
compatibility, physical stereo comfort and motion-to-photon performance remain
unverified. Local session diagnostics are saved under `working/pcvr/sessions/`.
For the hit-effect investigation, launch with `-EffectAudit` for a bounded trace.

## Source and license

`pcvr/native/` contains the DirectDraw proxy, stereo renderer, OpenXR bridge,
controller hooks, texture loader and regression tests. `pcvr/assets/` contains the
original procedural pistol OBJ; `pcvr/tools/` contains optional Python inspection
tools (Python 3 plus Pillow for image tools). See [contributing](CONTRIBUTING.md).

The authored mod is licensed under [MIT](LICENSE). The original game remains
proprietary and must be supplied separately. Game files, disc images, captures,
third-party DLLs and private HD artwork are excluded. This repository contains
only the original-PC mod; the separate native Quest project is not included.

## Future releases

A later GitHub release is planned to provide an easy Windows installer. Players
will select their separately obtained game files, with optional links to help
find them; the installer will contain only the mod and permitted dependencies.
See [the installer roadmap](ROADMAP.md). The current alpha is source-only.
