# Replacement texture format

Place your own replacement PNGs in the staged game's `hd-textures/` directory.
Enable loading with `run-probe.ps1 -VR -TexturePack`. Restart after changing files.
The public release supplies no original textures or private HD sample artwork.

Each filename is `<content-key>.png`, where the key is SHA-256 of the native
texture's little-endian 32-bit width and height followed by its top-down,
unpremultiplied BGRA pixels. This is the native decoded content's key, not a hash
of the replacement PNG. Runtime texture-dump filenames provide these keys.

Exact-size replacements must keep the original aspect ratio and use 1x, 2x, 4x
or 8x dimensions, with at most 4096 pixels per edge. Opaque matching-aspect artwork
at other sizes may be imported down to the largest compatible scale, capped at
8x; the file itself is unchanged. Artwork containing alpha must already have an
exact supported size, so fitting cannot change cutout edges.

Only eligible observed world draws are replaced. HUD and flat overlays retain
native art. Colour-keyed draws, mipmapped sources, changing surfaces, unsupported
formats, missing files and invalid images fall back to native. The per-process
limits are 256 cached source surfaces, 32 MiB source data and 128 MiB replacement
GPU pixels. First-use PNG loading can hitch; this is not a completed full-pack
streaming system.

For local research, `build-replay.cmd` and `test-replay.ps1 -TextureDump` can export
observed game textures using your own game and disc. Those dumps are game-derived
data and belong outside the source repository. `pcvr/tools/catalog_textures.py`
creates a private inspection catalog; it requires Pillow.

Contribute separately authored artwork only with clear provenance and permission
to distribute it. Do not commit original game dumps or edited game-derived art.
