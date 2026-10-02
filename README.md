# HotD2VR — The House of the Dead 2 PCVR mod

[![Windows source build](https://github.com/Icedomega13/HouseOfTheDead2VROG/actions/workflows/build.yml/badge.svg)](https://github.com/Icedomega13/HouseOfTheDead2VROG/actions/workflows/build.yml)

An open-source OpenXR mod for the **original Windows PC release** of The House of
the Dead 2, developed with Quest 3 over Virtual Desktop. It runs the original game
logic and intercepts DirectDraw / Direct3D 7 rendering to produce tracked stereo
views and controller input. It is a PCVR mod, not a standalone Quest application
or a rebuild of the game's proprietary source.

**Community installer alpha: v0.2.1-alpha.23 (probe 23).** Download the
[Windows setup EXE](https://github.com/Icedomega13/HouseOfTheDead2VROG/releases/tag/v0.2.1-alpha.23)
and follow [installation help](https://github.com/Icedomega13/HouseOfTheDead2VROG/blob/v0.2.1-alpha.23/INSTALL.md). Select the Windows PC disc-image ZIP
you downloaded; setup prepares your VR game copy and disc automatically. No unzip
or original flatscreen installation is needed. IMG/ISO and existing game folders
also work. Setup downloads verified dependencies and creates a launcher. No game
files or private HD sample are included. No compiler is needed.
The earlier source-only `v0.1.0-alpha.18` remains available as a separate tag.

## Quick start for players

1. Download **HotD2VR-Setup-0.2.1-alpha.23.exe** from the release above.
2. Select your original Windows game's downloaded **disc-image ZIP as-is**, choose
   a destination with at least 3 GB free, then click **Install / update**. Setup
   extracts the game and prepares its disc automatically; no flatscreen install.
3. Connect your Quest through Virtual Desktop, select **VDXR** in Streamer, then
   launch **Play HotD2VR** from the desktop. No compiler or SDK is needed.

Prefer aiming with the gun alone? Press **Y on the left controller** to hide both
the green aiming dot and native red crosshair. Press Y again to restore them.
The gun and shooting remain active. Ammo stays in its original position.

See [complete installer help](https://github.com/Icedomega13/HouseOfTheDead2VROG/blob/v0.2.1-alpha.23/INSTALL.md)
for supported downloads, updates, rollback, removal and troubleshooting. The setup
EXE is unsigned; its release includes checksums. Game files and an HD texture pack
are not included.

The community installer uses **probe 23**, adding a left-Y toggle
for both headset aiming markers. It also hides the observed
unused player-two join/credit footer in the headset while keeping ammo in place.
It retains probe 20's near-screen shot-flash correction and probe 19's
adjustable vibration and cancellation requests on focus/tracking loss. The
maintainer reported a successful version-23 headset play-test. Installer setup
on fresh community PCs and broader campaign/hardware coverage remain pending.

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
- Your own original PC game disc-image ZIP/IMG/ISO or installed copy and media. The
  tested `Hod2.exe` SHA-256 is
  `c6b4116788b7f68c56860fb9cc8a94bf984e620907031e4bc3db43623dbe579a`.
  Other executable revisions, the modern remake and emulated console versions
  have not been validated; executable-specific hooks may not work on them.
- For source builds only: Visual Studio 2022 or newer / Build Tools with **Desktop development with C++**,
  MSVC x86/x64 tools and a Windows SDK. The build auto-discovers the installation.
- An OpenXR runtime supporting **32-bit applications** and a connected headset.
  Quest 3 through Virtual Desktop / VirtualDesktopXR is the tested development
  setup. Configure the provider's active runtime before launch. Other runtimes
  are unverified.
- Internet access for the separately downloaded, SHA-256-verified OpenXR 1.1.63
  SDK/loader and dgVoodoo2 2.87.5 backend. See [third-party notices](THIRD_PARTY.md).

## Build and set up

Most players should use the [installer](https://github.com/Icedomega13/HouseOfTheDead2VROG/blob/v0.2.1-alpha.23/INSTALL.md). The commands below are for
developers who want to compile the mod themselves.

Run these commands from the repository root. `-ExecutionPolicy Bypass` applies to
that PowerShell process; it does not change the system execution policy.

```powershell
git clone https://github.com/Icedomega13/HouseOfTheDead2VROG.git
cd HouseOfTheDead2VROG
git checkout v0.2.1-alpha.23
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

These controls describe the current probe-23 installer. For matching source,
build the release tag shown in the developer commands above.

| Control | Action |
|---|---|
| Right controller | Aim the pistol |
| Right trigger | Fire |
| Right B | Reload; also retains the game's deliberate cutscene-skip behavior |
| Lower the barrel | Automatic reload when armed and outside detected cinematics |
| Right A | Start / confirm; another press may be needed after the title transition |
| Left stick directions | Menu navigation |
| Left stick click | Recenter while facing your intended forward direction |
| Left X | Escape / back |
| Left Y | Hide / restore both headset aiming markers; gun-only aiming |

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

Probe 21 adds `HideUnusedPlayerTwo` (boolean, default true) for the English
player-two join/credit footer. Set it to false to restore that headset display.
Title/start and other messages retain their native paths; ammo is unchanged.
Unrecognized/modified prompt graphics remain visible. Broader campaign and
physical headset coverage remain pending.

Probe 22 corrects the credit count that still flashed in version 21: the known
five is matched directly in its native counter slots, and other stable count
glyphs are remembered after a recognized credit row authenticates them. They
stay hidden when the label blinks off. The shared font remains native elsewhere.

In probe 23, press **left Y** to toggle the green aiming dot and native red
crosshair without hiding the gun or changing aiming/shooting. X remains Back;
right A/B remain Start/Reload. The toggle lasts for this session and survives
focus loss. `AimingCursor` (boolean, default true) controls starting visibility.
No menu is required. The maintainer reported that the version-23 play-test went
well; community controller/runtime coverage is still needed.

`HapticStrength` is an integer percentage from 0 to
200: 100 preserves the existing pulse feel, 50 halves amplitude, 200 doubles it
and 0 is silent. Trigger/reload pulses acknowledge input; they do not yet detect
actual native shots, ammo refill, hits or damage. Strength and focus-loss behavior
need broader physical testing across community hardware.

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

## Development and future releases

The community installer is available now. The accepted single-gun build remains
the public playable baseline. A separate dual-wield prototype is being explored:
two controller-aimed guns sharing one player's health and, initially, native ammo.
Dual wielding is **not included in the current installer**. Independent magazines,
reliable overlapping shots and broader campaign/hardware coverage remain work
for later previews. See [the release roadmap](ROADMAP.md).
