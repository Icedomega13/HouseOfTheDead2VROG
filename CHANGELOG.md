# Changelog

## Unreleased — probe version 19 candidate

- Adjustable `HapticStrength` from 0 to 200 percent; 100 preserves prior feel.
- Request cancellation of submitted vibration on tracking/focus/input loss,
  invalid origin, disable/zero strength and XR lifecycle failure.
- Handle positive not-focused statuses without claiming a pulse was delivered.
- Reload pattern takes priority when trigger and reload have simultaneous edges.
- Bound impact traces with cached content keys, UVs and tint; replay traces
  distinguish cached same-size textures. No hit-sprite placement change yet.
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
