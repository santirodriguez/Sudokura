# Sudokura distribution notices

This file travels with packaged builds so provenance and licensing do not
depend on the build machine.

## Sudokura

Sudokura is distributed under GNU GPL version 3. The complete license text is
included as `LICENSE.txt` (or the platform-equivalent documentation resource).

## DejaVu Sans

The fallback font is DejaVu Sans. Its license text is included with every
package as `DejaVu-FONT-LICENSE.txt`.

## Audio

The four bundled OGG files come from MintoDog's `Cozy Puzzle Jingle & Result`
asset on OpenGameArt and are CC0. The exact file mapping and provenance are
included as `audio/PROVENANCE.md` or `AUDIO-PROVENANCE.md`.

## Runtime libraries

Sudokura links SDL2, SDL2_ttf and SDL2_mixer and packages the non-system
runtime dependency closure required by each platform. Platform package scripts
record exact library/package versions in the build provenance and dependency
reports. Windows additionally includes `COMPONENTS.txt` and copies license
files exposed by the MSYS2 packages that own each bundled DLL.

These reports are part of the candidate audit trail. They are not a claim of
bit-for-bit reproducibility or a substitute for platform acceptance testing.


## Platform package audit

Linux candidates include `COMPONENTS.txt` plus copied Debian copyright files for bundled shared libraries. The AppImage intentionally leaves graphics-driver implementations to the target system and records external system dependencies separately.

The macOS v1.3 candidate is macOS 15+ arm64 only. Its bundle contains a `COMPONENTS.txt` inventory and license material/metadata for Homebrew-owned dylibs. The bundle is relocalized before an **ad-hoc integrity signature** is applied. Ad-hoc signing is not Developer ID signing and is not notarization; real downloaded-DMG acceptance remains a separate gate.
