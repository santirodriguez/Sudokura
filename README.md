# Sudokura v1.3.0

<p align="center"><img src="assets/branding/source/sudokura-head.png" alt="Sudokura" width="520"></p>

A lightweight, offline desktop Sudoku written in **C11 with SDL2, SDL2_ttf, and SDL2_mixer**. Sudokura focuses on reliable local saves, deterministic puzzles, clear mouse/keyboard interaction, practical accessibility, and small native packages.

Sudokura does not require an account, cloud service, or telemetry.

## Highlights

- Classic, Strikes, and Time Attack modes, plus an independent Daily Sudoku.
- Easy, Medium, and Hard puzzles generated deterministically with a unique solution.
- Generator revision 3 rates new puzzles by supported human-solving techniques; revision 2 remains for existing v1.2 puzzle identities.
- Versioned local profile storage with normal/Daily slots, previous-copy recovery, explicit save-failure handling, and v1.2 migration backups.
- Undo/Redo, reversible note cleanup, Clear, logic Hint, Verify, Reveal, Strict/Free input, and exact-puzzle Restart/Retry.
- Responsive desktop/portrait geometry, HiDPI-aware rendering, dark/light themes, reduced motion, and keyboard focus across visible controls.
- English, Español, and Català.
- Optional adaptive audio with persistent master, Music, and FX controls. Audio failure never blocks gameplay.

The approved v1.3.0 canonical screenshot is intentionally not committed yet. Diagnostic `--render-screenshots` output is validation evidence and must not be used as public artwork. Historical screenshots remain under [`docs/images/`](docs/images/).

## Downloads and platform scope

Published builds are distributed through [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases).

| Platform | Package | Scope / validation |
|---|---|---|
| Windows 11 x64 | Per-user installer + portable ZIP | Final candidate manually accepted on real Windows. The package is not commercially code-signed, so Windows may show a SmartScreen/reputation warning. |
| Linux x86_64 | AppImage | Built against the Ubuntu 22.04 baseline; automated Ubuntu 24.04/Fedora 44 probes and final real-Linux acceptance passed. |
| macOS 15+ Apple Silicon/arm64 | DMG containing `Sudokura.app` | Automated bundle/Mach-O/DMG checks pass. **Experimental** because no real-Mac/Gatekeeper acceptance was recorded for v1.3.0. |

v1.3.0 does **not** create new Intel Mac, pre-macOS-15, Windows 32-bit, ARM Windows, or ARM Linux packages. Historical releases remain available, but that does not imply ongoing v1.3 support.

The macOS build is ad-hoc integrity signed only. It is **not** Developer ID signed and **not** notarized.

## Install and run

### Windows

Use either `Sudokura-v1.3.0-windows-x86_64-setup.exe` for a per-user installation or `Sudokura-v1.3.0-windows-x86_64.zip` for a portable copy. The installer does not require administrator privileges and does not delete the Sudokura profile on uninstall.

If Windows displays a reputation warning, do not disable Windows protections. Verify the published SHA-256 checksum and decide whether to run the package.

### Linux

```sh
chmod +x Sudokura-v1.3.0-linux-x86_64.AppImage
./Sudokura-v1.3.0-linux-x86_64.AppImage
```

If FUSE is unavailable:

```sh
./Sudokura-v1.3.0-linux-x86_64.AppImage --appimage-extract-and-run
```

### macOS

Open `Sudokura-v1.3.0-macos-arm64.dmg` and copy `Sudokura.app` to Applications if desired. The v1.3.0 Mac package is experimental, ad-hoc signed, and not notarized.

## Controls

| Action | Control |
|---|---|
| Select a cell | Mouse, arrows, or WASD |
| Place a number | 1–9 or numeric keypad |
| Clear selected editable cell | 0, Backspace, Delete, or Clear |
| Notes | N toggles Notes; Shift+1–9 writes a note; right-click edits a note in the clicked editable cell |
| Automatic peer-note cleanup | Shift+N |
| Undo / Redo | Ctrl+Z / Ctrl+Shift+Z or Ctrl+Y on Windows/Linux; Cmd equivalents on macOS |
| Hint | H opens a logic hint; H/Enter applies it when possible; Shift+H reveals the selected cell from the Hint panel |
| Verify | Ctrl+Enter on Windows/Linux; Cmd+Enter on macOS, or the Verify control |
| Strict / Free | M |
| Pause / Resume | P or the visible control |
| Theme / Language | T / L |
| Master audio mute | V |
| Music/FX audio controls | Shift+V or long-press the speaker |
| Help / About | F1 / F2 |
| Navigate visible controls | Tab / Shift+Tab; Enter/Space activates focus |
| Back / Menu | Escape where applicable, or the visible control |

Settings also exposes Theme, Language, Sound, Music, FX, Reduced motion, and Back.

## Saves, upgrades, recovery, and rollback

Sudokura stores data locally through SDL's per-user preference path for organization `santirodriguez` and application `Sudokura`. On Windows this is normally:

```text
%APPDATA%\santirodriguez\Sudokura\
```

Linux follows the user's XDG data location; without a custom XDG path it is normally under `~/.local/share/santirodriguez/Sudokura/`.

v1.3 uses `profile.dat` as the active profile and keeps `profile.dat.bak` as the previous valid copy. The profile contains independent normal/Daily slots, preferences, bounded Undo/Redo history, and local results.

First migration from v1.2 preserves immutable migration-source copies:

- `session.dat.v1.2.bak`
- `preferences.dat.v1.2.bak`
- `audio-levels.dat.v1.2.bak`

The original v1.2 files are not rewritten as v1.3 data. If `profile.dat` is corrupt but `profile.dat.bak` is valid, startup offers recovery. Future/incompatible profiles are not deleted or silently rewritten.

### Returning to an older release

Before downgrading, close Sudokura and copy the entire profile directory somewhere safe.

Do **not** rename `profile.dat` to a v1.2 filename or feed the v1.3 container to an older reader. Keep the `.v1.2.bak` migration sources untouched. If an exact pre-upgrade v1.2 state must be restored, make working copies of those backups rather than modifying the backups themselves.

A v1.3-only session is not expected to appear in v1.2. Rollback preserves the older compatible state, not progress created only in v1.3.

## Reporting problems

See [`docs/REPORTING_ISSUES.md`](docs/REPORTING_ISSUES.md). Useful reports include the version, OS/architecture, package type, game mode/difficulty, reproducible steps, expected/actual behavior, and seed/generator revision when relevant.

Sudokura has no telemetry. Do not upload profile/save files unless they are specifically needed to reproduce a problem and you have reviewed what you are sharing.

## Build and test

Install a C compiler, `pkg-config`, SDL2, SDL2_ttf, SDL2_mixer, Python 3, and Go.

```sh
make assets
make
make test
make test-ui
./sudokura
```

`make test` covers gameplay, deterministic generation, persistence, storage failures, migration, clock policy, seed handling, geometry, desktop state, and external URL launching. `make test-ui` covers translated text fitting, SDL2_mixer behavior, input mapping, localization, and interaction paths.

`./sudokura --render-screenshots DIR` produces diagnostic UI-review frames and a semantic manifest. These are temporary validation artifacts, **not** canonical release screenshots.

## Documentation

- [v1.3.0 release notes](docs/RELEASE_NOTES_1.3.0.md)
- [v1.3.0 implementation record](docs/V1.3.0_IMPLEMENTATION.md)
- [changelog](CHANGELOG.md)
- [reporting issues and rollback data](docs/REPORTING_ISSUES.md)
- [documentation images](docs/images/README.md)
- [v1.2.0 release notes](docs/RELEASE_NOTES_1.2.0.md)

## Credits

Music: **Cozy Puzzle Jingle & Result** by **MintoDog**, from [OpenGameArt](https://opengameart.org/content/cozy-puzzle-jingle-result), licensed under **CC0**. See [`assets/audio/README.md`](assets/audio/README.md) for the exact file mapping.

Language flags are vendored from `lipis/flag-icons` under the MIT license; see [`assets/flags/README.md`](assets/flags/README.md). Interface and input effects are generated at runtime.

## Support Sudokura

If you enjoy Sudokura and want to support its development, [support Sudokura here](https://santiagorodriguez.com/donate/).

GPLv3 — © 2025–2026 [Santiago Rodriguez](https://santiagorodriguez.com/)
