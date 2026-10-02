# Changelog

## Unreleased — probe version 23 candidate

- Left Y toggles the green aiming dot and observed native red crosshair in the
  headset. Gun, shooting, desktop output and ammo placement stay unchanged.
- One change per press, with release required after inactive/failed input;
  preserve session visibility through focus loss. Existing X/A/B bindings remain.
- `AimingCursor` boolean defaults to true and controls launch preference; the
  runtime toggle stays in memory for the session.
- 857 local checks passed, including actual GPU gun/dot visibility comparisons.
  Native replay uses the same toggle helper; physical Y binding/feel pending.

### Version 22 changes retained

- Fix the credit value that still flashed in version 21's physical test. Match
  the captured five directly in the native credit slots; cache other stable
  count glyphs after a freshly recognized credit row authenticates them.
- Recognized counts no longer depend on their label appearing every frame.
  Shared-font draws outside the count slots stay native. Changed or unreadable
  sources cannot become cached count identities.
- 812 local checks passed, including 34 HUD and 14 mocked production-cache
  checks. A standalone fixture fails on version 21 and passes on version 22.
  Physical verification of this correction remains pending.

### Version 21 changes retained

- Hide the observed English player-two join invitation, credit label and count
  from the headset. Match exact bitmap content and footer geometry; retain
  original desktop drawing and ammo placement.
- Optional `HideUnusedPlayerTwo` boolean defaults to true; false restores the
  native footer. Bounded source caching avoids repeated texture readbacks.
- Freshly recognize other count glyphs using a row association expiring after
  one following presentation; unrecognized count graphics stay native until
  authenticated. The known five is covered even before its first label.
- Physical HUD verification and broader menu/campaign coverage remain pending.

### Version 20 changes retained

- Recognize the observed rotated native near-screen shot flame/glow quads and
  give them the existing two-metre HUD-distance eye pass. Previously they stayed
  around one game unit (10 cm), amplifying head movement and eye separation.
- Preserve native desktop drawing and ordinary world-particle depth. Captured
  transform fixtures and moving-head bridge checks cover the regression.
- Physical testing of the shot-flash correction is pending.

### Version 19 changes retained

- Adjustable `HapticStrength` from 0 to 200 percent; 100 preserves prior feel.
- Request cancellation of submitted vibration on tracking/focus/input loss,
  invalid origin, disable/zero strength and XR lifecycle failure.
- Handle positive not-focused statuses without claiming a pulse was delivered.
- Reload pattern takes priority when trigger and reload have simultaneous edges.
- Bound impact traces with cached content keys, UVs and tint; replay traces
  distinguish cached same-size textures.
- 734 local regression checks and a separate 60-second native replay passed.

Physical feedback is pending. Haptics acknowledge input, not native ammo, hit or
damage events. Published `v0.1.0-alpha.18` and the local playable fallback remain
preserved; the candidate is a development change, not a 1.0 release.

## v0.1.0-alpha.18 — 2026-10-01

First public, source-only alpha of the original Windows game's OpenXR PCVR mod.

- Head tracking, per-eye geometry rendering, controller aiming, tracked pistol,
  aiming dot, menu controls and left-stick recentering.
- 1200x1200 per-eye profile, 4x MSAA and 8x anisotropic filtering.
- Native game timing preserved; no production frame-rate unlock.
- PNG texture replacement loader. No game textures or private HD sample bundled.
- Moving cinematic-bar suppression and extended observed visibility queries.
- Private depth-tested, two-sided gun pass preserving native scene depth.
- Aim-down reload with a cinematic-bar guard; B remains available.
- Controller focus/tracking-loss handling and guarded OpenXR frame lifecycle.
- Portable Visual Studio discovery, optional disc-image mounting and source tests.

Version 17 received positive physical headset feedback. Version 18's gun and
cinematic-guard changes passed local regression checks and bounded native-game
replays, but still need a physical headset test. The development pass recorded
709 checks, including 267 actual GPU gun checks and 36 Windows input checks.

Known issues include occasional misplaced hit sprites, brief residual cinematic
bars, missing scenery when looking behind the original rail-camera view, and
incomplete cutscene coverage for the automatic reload guard. A full playthrough
and measured headset performance have not been established.
