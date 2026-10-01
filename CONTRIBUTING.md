# Contributing

Open an issue with reproduction steps, the release/commit used, Windows version,
GPU, headset and OpenXR runtime. State whether the result came from a real headset
or a synthetic test. For visuals, describe the effect and when it occurs; review
logs for personal paths before sharing. Do not upload game files or disc images.

Build with `pcvr/build.cmd` and run `pcvr/test-source.ps1` before submitting a pull
request. Add a regression check when changing controller state, matrices, render
state restoration or OpenXR frame ownership. Real DirectInput and GPU checks are
optional locally and require the relevant Windows APIs/backend; hosted CI runs
the non-headset suites. See the README for commands.

Keep production game speed at its original cadence. Replay-only timing overrides
are experimental and must remain behind `HOTD2_CONTROLLER_REPLAY_TEST`. Do not
replace the original executable or ship game-derived textures. Use separately
authored artwork with clear provenance when contributing assets.

Contributions are provided under the repository's MIT license. Keep changes
focused and document what was tested and what still needs headset feedback.
