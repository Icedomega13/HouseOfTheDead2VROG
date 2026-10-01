# Release roadmap

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
