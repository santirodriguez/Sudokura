# Sudokura v1.2.0

<p align="center"><img src="assets/branding/source/sudokura-head.png" alt="Sudokura" width="520"></p>

A lightweight desktop Sudoku written in **C11 with SDL2 and SDL2_ttf**. Sudokura v1.2.0 adds deterministic difficulty generation, a daily puzzle, persistent sessions, a redesigned responsive interface, and reproducible desktop packages while keeping the application small, offline, and native.

## Features

- **Three game modes:** Classic, Strikes, and 10-minute Time Attack.
- **Three difficulties:** Easy, Medium, and Hard, generated deterministically with a unique solution.
- **Daily Puzzle:** one deterministic Classic · Medium puzzle per local calendar date.
- **Continue and autosave:** resume the exact board, notes, hints, timer, mistakes, mode, difficulty, and relevant preferences.
- **Restart and Retry:** replay the same generated puzzle instead of silently replacing it.
- **Real pause:** gameplay and the timer stop; losing window focus also pauses safely.
- **Notes, hints, verification, Strict/Free input, progress tracking, and completed-number feedback.**
- **Dark and light themes.**
- **English, Español, and Català**, selectable with the bundled USA, Argentina, and Senyera flag assets. Runtime language selection is fully offline.
- **Mouse and keyboard controls** with responsive desktop and portrait layouts.

## Controls

| Action | Control |
|---|---|
| Navigate Home | Tab / Shift+Tab, arrows, Enter |
| Select cell | Mouse, arrows, or WASD |
| Place / clear | 1–9; 0, Backspace, or Delete |
| Notes | N, Shift+1–9, right-click, or a cell sub-position |
| Hint / Strict-Free | H / M |
| Pause / theme / language | P / T / L |
| Continue / Daily from Home | C / D |
| Help / about / back | F1 / F2 / Escape |

## Downloads

Published packages are available from [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases). When v1.2.0 is published, the release uses these filenames:

- `Sudokura-v1.2.0-linux-x86_64.AppImage` — Linux x86_64.
- `Sudokura-v1.2.0-windows-x86_64.zip` — Windows x86_64.
- `Sudokura-v1.2.0-macos-x86_64-unsigned.zip` — macOS Intel.
- `Sudokura-v1.2.0-macos-arm64-unsigned.zip` — macOS Apple Silicon.
- `SHA256SUMS.txt` — SHA-256 checksums for all four runtime packages.

Release-candidate packages are produced by the package-preview workflow before publication. macOS packages are unsigned; signing and notarization are not claimed.

## Build and test

Install a C compiler, `pkg-config`, SDL2, SDL2_ttf, Python 3, and Go. Python and Go are used for reproducible asset generation and validation.

```sh
make assets       # regenerate and validate derived branding assets
make              # build ./sudokura with C11 warnings enabled
make test         # gameplay, deterministic generation, session, i18n, and geometry tests
make test-ui      # SDL_ttf measurements for languages and responsive tiers
make clean
./sudokura [--font /path/to/font.ttf]
```

The desktop layout is tested from 640×480 through 3440×1440. Native portrait layouts are tested at 360×640, 390×844, and 412×915. The same geometry is used for drawing and pointer hit-testing, and runtime packages bundle a UTF-8-capable fallback font.

`./sudokura --smoke-test` renders deterministic Home and game frames without interaction. `./sudokura --render-screenshots DIR` writes **50 diagnostic BMP frames** covering ten viewports and five screens (Home, Play, Help, Result, and Pause) across languages and themes. `./scripts/validate_screenshots.py DIR` validates their dimensions and rejects missing or blank-looking frames. These generated frames are CI diagnostics, not release screenshots.

Linux CI also runs AddressSanitizer and UndefinedBehaviorSanitizer against the core and persistence test suites. Normal Linux and Windows builds compile with `-Wall -Wextra -Wpedantic -Werror`.

## Package validation

The packaging paths verify more than archive creation:

- **Linux:** builds an x86_64 AppImage, bundles the icon and fallback font, smoke-tests both the installed AppDir binary and the AppImage, records dependencies and inventory, and creates a checksum. The external `linuxdeploy` input is fetched by immutable GitHub asset ID and verified against its expected SHA-256 before execution.
- **Windows:** builds a portable x86_64 ZIP with embedded version/icon resources, recursively closes non-system DLL imports, smoke-tests the packaged executable and an extracted ZIP with a clean runtime path, verifies the archive, and records dependencies, inventory, and checksum.
- **macOS:** builds separate unsigned Intel and Apple Silicon `Sudokura.app` bundles, embeds SDL dependencies, icon, fallback font, and license, rejects build-host library paths, verifies architecture and ZIP integrity, and records dependency/smoke reports and checksums.

The release workflow verifies that a release tag matches the version declared by the source and creates only a **draft** release for manual review. Publication remains a separate manual decision.

## Branding and visual history

The v1.2 source artwork lives in `assets/branding/source/`, with exact additional source variants preserved under `assets/branding/source-packed/`. `sudokura-head.png` is the primary project and in-app identity. Platform icons and embedded SDL resources are generated reproducibly without SDL_image. Language flags and their upstream license/provenance are vendored under `assets/flags/` so runtime remains offline.

Historical screenshots remain under [`docs/images/`](docs/images/). The canonical v1.2 screenshot is deliberately not versioned yet: it will be supplied after a real v1.2 release candidate has been installed and manually tested. Automated UI-review renders are never used as the official screenshot.

See the [v1.2.0 release notes](docs/RELEASE_NOTES_1.2.0.md), the [v1.2.0 implementation record](docs/V1.2.0_IMPLEMENTATION.md), and the [documentation image archive](docs/images/README.md).

## Project scope

Sudokura remains a lightweight, offline desktop game focused on Linux, Windows, and macOS. It does not require an account, network service, or online puzzle API.

GPLv3 — © 2025–2026 [santirodriguez](https://santiagorodriguez.com)
