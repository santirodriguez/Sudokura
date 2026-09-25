# Sudokura

<p align="center">
  <img src="assets/branding/source/sudokura-head.png" alt="Sudokura" width="520">
</p>

<p align="center">
  <strong>Classic Sudoku, made for desktop.</strong>
</p>

<p align="center">
  Daily puzzles, four game modes, notes, hints, fast keyboard play, themes, optional audio,<br>
  and local progress — all in a lightweight app that stays on your device.
</p>

<p align="center">
  <a href="#download-sudokura">Download</a>
  ·
  <a href="docs/RELEASE_NOTES_1.3.5.md">What's new in 1.3.5</a>
  ·
  <a href="docs/images/README.md">Screenshots</a>
  ·
  <a href="#build-from-source">Build from source</a>
</p>

<p align="center">
  <a href="docs/images/sudokura-v1.3.0.png">
    <img src="docs/images/sudokura-v1.3.0.png" alt="Sudokura desktop gameplay" width="900">
  </a>
</p>

## Download Sudokura

<p align="center">
  <a href="https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-Windows-x64-Setup.exe">
    <img src="https://img.shields.io/badge/Windows-Installer-0078D4?style=for-the-badge&logo=windows11&logoColor=white" alt="Download Sudokura for Windows">
  </a>
  <a href="https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-Windows-x64-Portable.exe">
    <img src="https://img.shields.io/badge/Windows-Portable-2563EB?style=for-the-badge&logo=windows11&logoColor=white" alt="Download portable Sudokura for Windows">
  </a>
  <a href="https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-Linux-x64.AppImage">
    <img src="https://img.shields.io/badge/Linux-AppImage-333333?style=for-the-badge&logo=linux&logoColor=white" alt="Download Sudokura AppImage for Linux">
  </a>
  <a href="https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-macOS-arm64.dmg">
    <img src="https://img.shields.io/badge/macOS-Apple_Silicon-555555?style=for-the-badge&logo=apple&logoColor=white" alt="Download experimental Sudokura for macOS">
  </a>
</p>

<p align="center">
  <sub>
    Windows 11 x64 · Linux x86_64 · macOS 15+ Apple Silicon (experimental)
  </sub>
</p>

**Windows:** choose the installer for a normal per-user installation, or the portable EXE if you want a single file with no installation.  
**Linux:** the AppImage is the complete application in one file.  
**macOS:** the Apple Silicon build is experimental, ad-hoc signed, and not notarized.

Windows builds are not commercially code-signed, so SmartScreen may show a reputation warning.

## Why Sudokura

<table>
<tr>
<td width="33%" valign="top">
<strong>Built around the puzzle</strong><br><br>
Easy, Medium, and Hard puzzles have a unique solution, with difficulty shaped by human-solving techniques rather than arbitrary clue counts.
</td>
<td width="33%" valign="top">
<strong>Fast on a desktop</strong><br><br>
Mouse and keyboard are first-class. Notes, Undo/Redo, Hint, Verify, Restart, themes, and audio controls stay close at hand without crowding the board.
</td>
<td width="33%" valign="top">
<strong>Private by design</strong><br><br>
Sudokura runs offline. Games, settings, progress, and local results stay on your device, and the app sends no telemetry.
</td>
</tr>
</table>

## Play your way

- **Classic** — focused Sudoku with neutral feedback.
- **Strikes** — mistakes matter.
- **Time Attack** — play against the clock.
- **Daily Puzzle** — one deterministic Classic · Medium puzzle for each local calendar day.
- **Notes, Hint, Verify, Reveal, Undo/Redo, Restart, and Retry** are available when you need them.
- **Dark and light themes**, reduced motion, responsive layouts, and full mouse/keyboard navigation.
- **English, Español, and Català.**
- **Optional music and effects** with master, Music, and FX controls.
- Independent normal and Daily saves, automatic recovery safeguards, and local result history.

## Quick controls

| Action | Control |
|---|---|
| Move | Mouse, arrows, or WASD |
| Enter number | 1–9 |
| Clear | 0, Backspace, or Delete |
| Notes | N · Shift+1–9 · right-click |
| Undo / Redo | Ctrl+Z / Ctrl+Shift+Z or Ctrl+Y |
| Hint | H |
| Verify | Ctrl+Enter |
| Pause | P |
| Theme / Language | T / L |
| Master audio | V |
| Music / FX controls | Shift+V or long-press the speaker |
| Help / About | F1 / F2 |

Most actions are also available directly from the interface.

## Your progress stays yours

Sudokura stores data in the operating system's normal per-user application-data directory. Compatible v1.2 data is preserved during migration, with recovery copies so an upgrade does not silently replace the previous valid state.

For save locations, migration details, rollback guidance, and useful bug-report information, see [Reporting Sudokura issues](docs/REPORTING_ISSUES.md).

## More screenshots

The curated gallery includes Home / Continue, Settings, audio controls, compact Notes, and Help.

[Browse the screenshot gallery →](docs/images/README.md)

---

## Technical notes

### Verify a download

Release checksums and build provenance are published alongside the application packages:

[SHA-256 checksums](https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/SHA256SUMS.txt)
·
[Build details](https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-Build-Info.json)
·
[All releases](https://github.com/santirodriguez/Sudokura/releases)

<details>
<summary><strong>Linux first launch and FUSE fallback</strong></summary>

Make the AppImage executable before first launch:

```sh
chmod +x Sudokura-1.3.5-Linux-x64.AppImage
./Sudokura-1.3.5-Linux-x64.AppImage
```

If FUSE is unavailable:

```sh
./Sudokura-1.3.5-Linux-x64.AppImage --appimage-extract-and-run
```

</details>

## Build from source

Sudokura is written in **C11** with **SDL2**, **SDL2_ttf**, and **SDL2_mixer**.

Requirements: a C compiler, `pkg-config`, SDL2, SDL2_ttf, SDL2_mixer, Python 3, and Go.

Application C sources and private headers live under `src/`; SDL presentation fragments live under `src/sudokura_sdl/`.

```sh
make assets
make
make test
make test-ui
./sudokura
```

## Project documentation

- [v1.3.5 release notes](docs/RELEASE_NOTES_1.3.5.md)
- [Changelog](CHANGELOG.md)
- [Issue reporting, saves, and rollback](docs/REPORTING_ISSUES.md)
- [Screenshot gallery](docs/images/README.md)
- [v1.3.0 release notes](docs/RELEASE_NOTES_1.3.0.md)
- [Implementation record](docs/V1.3.0_IMPLEMENTATION.md)

## Credits

Music: **Cozy Puzzle Jingle & Result** by **MintoDog**, from [OpenGameArt](https://opengameart.org/content/cozy-puzzle-jingle-result), licensed under **CC0**.

Language flags are from [lipis/flag-icons](https://github.com/lipis/flag-icons), licensed under MIT.

## Support

If you enjoy Sudokura and want to support its development, [support Sudokura here](https://santiagorodriguez.com/donate/).

**GPLv3** · © 2025–2026 [Santiago Rodriguez](https://santiagorodriguez.com/)
