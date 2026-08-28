# Sudokura v1.1.0

<p align="center"><img src="assets/branding/source/sudokura-head.png" alt="Sudokura" width="520"></p>

A lightweight desktop Sudoku written in **C11 with SDL2 and SDL2_ttf**. Sudokura keeps its dark/light visual identity, notes, hints, verification, strict mode and three game modes while fitting every control into compact windows.

## Features and languages

- Classic, Strikes (three wrong moves), and 10-minute Time Attack modes.
- English, natural Argentine **Español**, and **Català**; press **L** to cycle the visible language selector.
- A responsive board, compact action grid and 3×3 numeric palette. The supported minimum is 640×480 and the same geometry drives drawing and pointer hit-testing.
- Dark/light themes, notes, conflict verification, hints, pause, and keyboard or mouse play.

## Controls

| Action | Control |
|---|---|
| Select | Mouse, arrows, or WASD |
| Place / clear | 1–9; 0, Backspace, or Delete |
| Notes | N, Shift+1–9, right-click, or a cell sub-position |
| Hint / strict mode | H / M |
| Theme / pause / language | T / P / L |
| Help / about / back | F1 / F2 / Escape |

## Downloads

Ready-to-play packages are published in [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases). Sudokura v1.1.0 uses these exact filenames:

- `Sudokura-v1.1.0-linux-x86_64.AppImage` — Linux x86_64.
- `Sudokura-v1.1.0-windows-x86_64.zip` — Windows x86_64.
- `Sudokura-v1.1.0-macos-x86_64-unsigned.zip` — macOS Intel.
- `Sudokura-v1.1.0-macos-arm64-unsigned.zip` — macOS Apple Silicon.
- `SHA256SUMS.txt` — SHA-256 checksums for all four packages.

The Linux AppImage and Windows ZIP were tested manually. The macOS builds passed automated packaging checks but were not tested on real Macs; feedback is welcome.

## Build and test

Install a C compiler, `pkg-config`, SDL2, SDL2_ttf, Python 3, and Go. Python and Go provide reproducible asset generation and validation.

```sh
make assets       # regenerate and validate derived branding assets
make              # build ./sudokura with C11 warnings enabled
make test         # deterministic gameplay, generator, i18n, and geometry tests
make test-ui      # SDL_ttf measurements for every language and viewport tier
make clean
./sudokura [--font /path/to/font.ttf]
```

The bounded desktop shell is tested from 640×480 through 3440×1440, and the portrait stacked shell is tested at 360×640, 390×844, and 412×915. Runtime packages bundle a UTF-8-capable fallback font. The English, Español, and Català segments use the same geometry for rendering and pointer hit-testing.

`./sudokura --smoke-test` renders deterministic title and play frames without interaction. `./sudokura --render-screenshots DIR` writes 40 dependency-free BMP reviews covering ten viewports, four screens, all languages, and both themes. `./scripts/validate_screenshots.py DIR` verifies every BMP's exact dimensions and rejects missing or blank-looking frames. Fonts come from a 17-entry cache (10–48 px); geometry selects separate note, body, control, cell, HUD, and heading tiers without reopening fonts during frames.

## Package details

The packaging workflows build consistently named v1.1.0 artifacts, checksums, and diagnostic reports.

- **Linux:** executable x86_64 AppImage with the derived application icon and desktop entry.
- **Windows:** portable x86_64 ZIP with an embedded icon, SDL runtime, recursively collected non-system DLLs, DejaVu Sans, and its license.
- **macOS:** separate x86_64 and arm64 ZIPs containing a real `Sudokura.app`. They are unsigned and were not manually tested on real Mac hardware for v1.1.0.

On macOS, unzip and drag `Sudokura.app` to Applications. Use Finder's **Open** context action if macOS asks for confirmation.

## Visual and technical history

The screenshots under [`docs/images/`](docs/images/) are historical records of earlier releases. No canonical v1.2 screenshot is versioned yet; the final public screenshot will be supplied after the real v1.2 packages have been tested.

See the [v1.1.0 release notes](docs/RELEASE_NOTES_1.1.0.md), the [technical implementation record](docs/V1.1.0_IMPLEMENTATION.md), and the [documentation image archive](docs/images/README.md).

## Branding assets

The v1.2 source artwork lives in `assets/branding/source/`, with two additional exact web-icon variants preserved in chunked Base64 under `assets/branding/source-packed/`. `sudokura-head.png` is the primary Sudokura identity, while the supplied icon variants remain preserved for platform-appropriate derived resources. Language flags are vendored under `assets/flags/` with their upstream source and license so runtime stays fully offline. The former v1.1 artwork is retained under `assets/branding/history/v1.1/`. Generated packages include only the derived resources they require.

## Project scope

Sudokura remains a lightweight desktop game focused on Linux, Windows, and macOS.

GPLv3 — © 2025–2026 [santirodriguez](https://santiagorodriguez.com)
