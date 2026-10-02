# HotD2VR Alpha installer

The **v0.2.0-alpha.23** installer contains the maintainer-tested version-23 mod.
It includes no original game files, disc images or HD texture pack. This remains
an alpha; community hardware and full-campaign coverage are still being gathered.

## Install and play

1. Download **HotD2VR-Setup-0.2.0-alpha.23.exe** from the
   [GitHub release](https://github.com/Icedomega13/HouseOfTheDead2VROG/releases/tag/v0.2.0-alpha.23).
2. Supply the installed **original Windows PC release** of The House of the Dead 2.
   In setup, browse to the folder containing `Hod2.exe`, with `cam`, `coli`, `evt`,
   `mot`, `pol`, `sound` and `tex` alongside it. Select an extracted, installed
   game folder rather than a ZIP, ISO or disc installer. The remake and console
   releases are incompatible. Setup checks the known executable SHA-256:
   `c6b4116788b7f68c56860fb9cc8a94bf984e620907031e4bc3db43623dbe579a`.
3. Choose a separate destination (default: `%LOCALAPPDATA%\HotD2VR`). If the
   original game needs its disc, optionally select your own ISO. Physical or
   already-mounted media can be used by leaving this field blank.
4. Click **Install / update**. Setup copies the game into its own folder and
   downloads checksum-verified dgVoodoo2 2.87.5 and OpenXR loader 1.1.63 directly
   from their upstream GitHub releases. Allow internet access for this step.
5. Connect Quest through Virtual Desktop. Select **VDXR** as the OpenXR runtime
   in Virtual Desktop Streamer. Setup reports whether Windows has an active
   **32-bit** OpenXR runtime; it does not change your runtime or install Virtual
   Desktop. Then use **Play HotD2VR** on your desktop, or the Play button in setup.

Windows 10/11 x64, .NET Framework 4.8 and a D3D11-capable graphics card are the
installer target. End users need no compiler, Git or development SDK. Setup runs
as the current user, without requesting administrator privileges or changing the
system PowerShell execution policy. Mounting an ISO can require permissions that
depend on your Windows configuration; already-mounted/physical media remains an option.

The alpha installer is **unsigned**. Windows may show an unknown-publisher or
SmartScreen message. Check that it came from this repository's release and compare
its SHA-256 with `SHA256SUMS.txt`. Do not disable antivirus or other system protection.

## Finding your game files

The optional **Find game files** button opens the maintainer-requested
[My Abandonware game page](https://www.myabandonware.com/game/the-house-of-the-dead-2-beg)
in your browser. No game download is automated, included or patched by setup.
Obtain the game separately, extract/install it as appropriate, and return to setup
to select the complete installed folder. Availability and package layouts on that
external page can change; only the executable revision above has been tested.

## Controls

| Control | Action |
|---|---|
| Right controller / right trigger | Aim / fire |
| Right A | Start / confirm |
| Right B | Reload; also the original game's deliberate cutscene skip |
| Lower the gun | Reload outside detected cinematics; raise it to rearm |
| Left stick / click | Navigate menus / recenter |
| Left X | Escape / back |
| Left Y | Hide or restore both headset aiming markers |

The cursor starts visible. Left Y changes it for this session. Rendering defaults
are 1200 pixels per eye, 4x MSAA and 8x anisotropic filtering, with native game speed.
To adjust these defaults, edit `pcvr\vr-settings.json` inside your mod installation
and restart. Lower `EyeSize` to 1000/800 or `Antialiasing` to 2/0 if needed.
The public installer retains original textures; the private development HD sample
is not distributed. Player-created PNG packs can be placed in
`working\pcvr\game\texture-packs\default\` (see the repository's texture-pack guide).

## Update, rollback and remove

Close the game, download a newer installer and select the **same HotD2VR destination**.
Setup reuses the copied game and preserves `pcvr\vr-settings.json` and game saves.
Replaced mod files are backed up under `backups\` before any update write. If an
update fails during replacement, setup restores the replaced files automatically.
To return to an earlier mod, rerun that earlier version's installer against the
same destination. Keep its download until the newer build is tested.

Use **Windows Settings → Apps → HotD2VR Alpha → Uninstall** to remove the mod,
launcher and removal entry. Removal checks installation ownership and file hashes;
changed files are retained for inspection. It keeps your original game, the copied
game/saves, settings and update backups. You can remove that retained folder yourself
when you no longer need those files. Installation and removal never alter the
original selected game folder.

## Troubleshooting and reporting

- **Unsupported executable / missing folders:** select the complete installed PC
  game. This alpha deliberately rejects untested revisions instead of patching them.
- **No active 32-bit runtime:** reconnect the headset, configure the provider's
  active OpenXR runtime, then launch again. Setup's registry check does not establish
  that a headset/session is ready. Other headset/runtime combinations are unverified.
- **Checksum/download failure:** retry with working internet. If a cached archive
  is reported corrupt, delete only that named file from `%LOCALAPPDATA%\HotD2VR-downloads`
  and retry. Setup has not committed the installation at this point.
- **Game does not start:** retain `working\pcvr\launcher.log`, the game folder's
  `hotd2-ddraw-probe.log`, and the newest `working\pcvr\sessions` record. Include your
  GPU, headset/runtime and installer version in a
  [GitHub issue](https://github.com/Icedomega13/HouseOfTheDead2VROG/issues).

Install/update/removal and local launch checks do not establish performance on a
clean PC or headset comfort. Report new-user setup and physical play-test results.
