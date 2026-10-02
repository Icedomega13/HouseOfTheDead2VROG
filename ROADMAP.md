# Release roadmap

## 1.0 readiness

The target is a release players can install and play through without developer
tools or routine troubleshooting. Work proceeds through these release gates:

| Area | Acceptance target | Current status |
|---|---|---|
| Aiming and effects | Gun, native shots and impact graphics agree throughout encounters | Probe 20 corrects near-screen shot-flash depth; physical placement and other effect coverage remain pending |
| Rendering | Solid gun, clean dialogue/transitions and predictable wide turns | Gun depth fix locally checked; residual bars and unseen geometry need broader coverage |
| Controls | Recenter, menus and both reload methods work reliably; resting the gun does not accidentally skip cinematics | Existing controls work in reported tests; cinematic gesture guard needs broader physical coverage |
| VR HUD | Unused second-player join prompts stay out of single-player gameplay; ammo remains readable | Probe 21 physical test exposed a flashing count; probe 22 corrects that path locally; ammo relocation deferred |
| Haptics and gun feel | Tunable shot/reload feedback, optional damage/dry-fire patterns, restrained recoil/flash | Probe 19 adds strength and cancellation; confirmed gameplay-event detection and physical feel are pending |
| Comfort | Consistent scale, stereo alignment and usable calibration | Positive feedback on version 17; settings and comfort need repeated physical checks |
| Performance | Stable measured headset delivery with native gameplay speed preserved | Native timing retained; independent higher-rate rendering remains research |
| Installation | Prebuilt mod installer, game selection, runtime checks, shortcut, updates and uninstall | Alpha-23 installer implements these; fresh-PC community setup testing pending |
| Campaign coverage | Entire campaign, bosses, branching routes, deaths/retries and runtime interruption tested | Full-playthrough and broader hardware/runtime coverage pending |

An independent high-rate renderer and a comprehensive HD texture pack may follow
the initial 1.0. The release gates require gameplay evidence; input-edge vibration
does not count as confirmed-shot or successful-reload feedback.

## VR HUD cleanup — community request

During single-player gameplay, hide the unused second-player **PRESS START**
invitation and its associated credit text from the headset. Preserve useful
start, continue, retry and game-over prompts, and respect an active second player.
The original desktop output should retain the native HUD.

The maintainer has deferred the community's gun-relative ammo proposal. Keep
the existing ammo display and placement for now. Any later optional relocation
must use validated native ammo draws or state, rather than controller presses.

The current renderer projects native HUD quads onto a shared two-metre plane.
These changes need selective identification of the join prompt elements;
hiding a whole text texture or screen region could also remove useful messages.
Probe 21 identifies the English join/credit bitmaps by content and native footer
geometry, then suppresses those draws and their associated count in the eyes.
Unknown graphics remain native. Test menus, continue/death and cinematics in
the headset; active second-player and full-route coverage remain unverified.
Probe 22 recognizes the known five independently and caches other authenticated
count glyphs so label blink gaps cannot expose them. Physical retesting is pending.
Probe 23 adds a left-Y cursor visibility toggle for gun-only aiming, without
moving ammo or changing shot coordinates. The maintainer reported a successful
version-23 physical play-test; wider coverage remains pending.

## Easy Windows installer — community alpha

The alpha installer is now implemented; see [setup instructions](INSTALL.md).
Players need no developer tools. It creates a separate copy from a supported
original-PC disc-image ZIP/IMG/ISO or installation, automatically prepares the disc
for imported downloads, downloads verified
dependencies, reports the active 32-bit runtime and creates a launcher/removal entry.
Updates back up replaced mod files and preserve settings/saves. Failed replacement
rolls back; removal retains the game/saves, imported ISO and modified files.
The tested CloneCD download now imports directly without original setup.

Implemented setup flow:

1. Select the original-PC disc-image ZIP downloaded by the player (no manual
   unpacking or installation), a loose IMG/ISO, or an existing game installation.
   The import prepares a local ISO for read-only mounting at launch; installed
   folders can still use optional ISO or physical/already-mounted media.
2. Offer an optional **Find game files / setup help** button that opens a help or
   acquisition page in the player's browser. The optional Find game files button
   opens the maintainer-requested My Abandonware game page. The player obtains
   the game separately and then selects its files in setup.
3. Validate required data and executable compatibility before installation, and
   explain missing or unsupported files clearly.
4. Create a separate playable mod installation, configure permitted dependencies,
   check the 32-bit OpenXR runtime/headset setup and create a desktop shortcut.
5. Provide a clean update/uninstall path with backups of replaced mod files and
   preserve the player's original game installation and saves.

The installer and GitHub release downloads must contain **no original game
executable, game data or disc images**. Dependency packaging or downloading must
follow each dependency's own redistribution terms. The installer should clearly
distinguish the mod from the separately supplied game.

Release validation should include setup from a clean Windows system, the supported
game package layouts, upgrades, rollback/uninstall and a physical headset test.
The installer is an alpha prerelease. Local installer lifecycle checks do not replace
fresh-PC setup or physical community play-tests, and do not establish 1.0 readiness.
