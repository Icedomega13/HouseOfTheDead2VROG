# Third-party components and game ownership

The MIT license covers the authored mod code, documentation and procedural pistol
mesh in this repository. It does not license the original game or external tools.
The House of the Dead 2 and its original executable, art, textures, audio, dialogue
and other game data remain the property of their respective owners, including
SEGA. This is an independent fan project and is not affiliated with SEGA.

No original game files, disc images, captured game imagery, derivative HD sample
artwork or third-party runtime binaries are distributed here. Supply your own game
installation and original media. A private local HD sample used in development is
not part of the public source release.

| Dependency | Use | Distribution / license |
|---|---|---|
| [Khronos OpenXR SDK / loader](https://github.com/KhronosGroup/OpenXR-SDK-Source/releases/tag/release-1.1.63) 1.1.63 | OpenXR headers, x86 import library and loader | Downloaded separately by `prepare-deps.ps1`; the downloaded package contains its Apache-2.0 license at `share/doc/openxr/LICENSE`. See [upstream license](https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/release-1.1.63/LICENSE). |
| [dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2/releases/tag/v2.87.5) 2.87.5 | DirectDraw / Direct3D 7 translation to D3D11 | Downloaded separately under the author's terms; see [official general documentation](https://dege.freeweb.hu/dgVoodoo2/ReadmeGeneral/). It is not covered by this project's MIT license. |
| Microsoft Visual Studio C++ tools, Windows SDK and DirectXMath | Windows x86 compiler and system APIs | Install separately under Microsoft's terms. No SDK source or binaries are vendored. |
| [Pillow](https://pillow.readthedocs.io/) | Optional Python image inspection tools | Install separately; [upstream license](https://github.com/python-pillow/Pillow/blob/main/LICENSE). Not needed to build or play. |
| OpenXR runtime / Virtual Desktop | Headset connection and compositor | Supplied separately by the user under the provider's terms. Not included. |

Dependency downloads are pinned by version and SHA-256 in `pcvr/prepare-deps.ps1`.
Do not bundle them under the project's MIT license when distributing a build.
