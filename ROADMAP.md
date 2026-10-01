# Release roadmap

## 1.0 readiness

The target is a release players can install and play through without developer
tools or routine troubleshooting. Work proceeds through these release gates:

| Area | Acceptance target | Current status |
|---|---|---|
| Aiming and effects | Gun, native shots and impact graphics agree throughout encounters | Hit-sprite placement remains under investigation; bounded traces are being improved |
| Rendering | Solid gun, clean dialogue/transitions and predictable wide turns | Gun depth fix locally checked; residual bars and unseen geometry need broader coverage |
| Controls | Recenter, menus and both reload methods work reliably; resting the gun does not accidentally skip cinematics | Existing controls work in reported tests; cinematic gesture guard needs broader physical coverage |
| Haptics and gun feel | Tunable shot/reload feedback, optional damage/dry-fire patterns, restrained recoil/flash | Probe 19 adds strength and cancellation; confirmed gameplay-event detection and physical feel are pending |
| Comfort | Consistent scale, stereo alignment and usable calibration | Positive feedback on version 17; settings and comfort need repeated physical checks |
| Performance | Stable measured headset delivery with native gameplay speed preserved | Native timing retained; independent higher-rate rendering remains research |
| Installation | Prebuilt mod installer, game selection, runtime checks, shortcut, updates and uninstall | Planned below; current public alpha is source-only |
| Campaign coverage | Entire campaign, bosses, branching routes, deaths/retries and runtime interruption tested | Full-playthrough and broader hardware/runtime coverage pending |

An independent high-rate renderer and a comprehensive HD texture pack may follow
the initial 1.0. The release gates require gameplay evidence; input-edge vibration
does not count as confirmed-shot or successful-reload feedback.

## Easy Windows installer — planned for a later GitHub release

The eventual public release should install the PCVR mod without requiring players
to compile it or run setup scripts. Installer implementation is deferred while
the playable mod is still being polished.

Planned setup flow:

1. Select an existing original-PC game installation, or select game files the
   player has downloaded separately. Support for archive/disc-image input should
   be chosen after testing the actual package layouts.
2. Offer an optional **Find game files / setup help** button that opens a help or
   acquisition page in the player's browser. A link to the abandonware site
   suggested by the maintainer can be evaluated when installer work begins; no
   particular site or download URL has been selected yet. The player obtains
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
The current alpha remains source-only; this roadmap does not announce an installer
or change the current launcher.
