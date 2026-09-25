# Sudokura 1.3.5

Sudokura 1.3.5 is a focused presentation and distribution update built on the v1.3.0 gameplay, persistence, accessibility, and puzzle-quality baseline.

## Downloads

| Platform | Download | Support |
|---|---|---|
| Windows 11 x64 | [Installer (x64)](https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-Windows-x64-Setup.exe) | Supported |
| Windows 11 x64 | [Portable (x64, no installation)](https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-Windows-x64-Portable.exe) | Supported |
| Linux x86_64 | [AppImage (x64)](https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-Linux-x64.AppImage) | Supported |
| macOS 15+ Apple Silicon | [DMG (arm64)](https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-macOS-arm64.dmg) | Experimental |

Technical files: [SHA-256 checksums](https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/SHA256SUMS.txt) · [Build details](https://github.com/santirodriguez/Sudokura/releases/download/v1.3.5/Sudokura-1.3.5-Build-Info.json).

## What changed

- The global speaker control no longer appears or retains a hidden hit target/popup state over Hint, Pause/focus-pause, Settings, generation overlays, or native dialogs.
- FX unmute confirmation now follows the corrected mute-state transition.
- Repository C sources/private headers are organized under `src/` with build/package paths updated accordingly.
- Windows now has a single-file installation-free portable EXE alongside the existing per-user installer. It uses the same audited runtime payload and the same AppData save/settings location.
- Public package filenames are consistent across platforms. One Build-Info JSON records source identity, artifact sizes/hashes, support scope, and the complete per-platform manifest data.

## Windows

The installer is per-user and does not require administrator privileges. The portable EXE does not install, create shortcuts, register an uninstaller, or download runtime components. It self-extracts to an instance-specific temporary directory, launches the contained game directly, and cleans its own temporary files on normal exit.

A forced process/OS termination can interrupt that cleanup and leave the interrupted launch's temporary files behind. Windows packages are not commercially code-signed, so SmartScreen may show a reputation warning.

Installer and portable builds use the same Sudokura AppData profile. Switching packages does not move or delete saves/settings.

## Linux

Make the AppImage executable before first launch:

```sh
chmod +x Sudokura-1.3.5-Linux-x64.AppImage
./Sudokura-1.3.5-Linux-x64.AppImage
```

If FUSE is unavailable:

```sh
./Sudokura-1.3.5-Linux-x64.AppImage --appimage-extract-and-run
```

## macOS

The macOS package targets macOS 15+ on Apple Silicon/arm64. It is ad-hoc integrity signed, not Developer ID signed, and not notarized. It remains experimental unless and until real-Mac/Gatekeeper acceptance is recorded.

## Saves and compatibility

v1.3.5 does not introduce a new save format. It keeps the v1.3 profile/persistence model and the existing v1.2 migration/recovery safeguards. Back up the complete Sudokura profile directory before downgrading to an older release.

## Verification

`SHA256SUMS.txt` covers the exact four public app binaries plus the Build-Info JSON. The Build-Info file records the release source commit, sizes, SHA-256 values, support declarations, reproducibility claims, and complete platform manifest data.

Publication remains separately gated on final release-candidate review and real-package Windows/Linux acceptance. The macOS experimental status is not upgraded by automated CI alone.

## Credits

Music: **Cozy Puzzle Jingle & Result** by **MintoDog**, from OpenGameArt under CC0. Exact mapping: [`assets/audio/README.md`](../assets/audio/README.md).

Language flags are from `lipis/flag-icons` under MIT: [`assets/flags/README.md`](../assets/flags/README.md). Sudokura is GPLv3.
