# Sudokura v1.1.0

<p align="center"><img src="assets/branding/source/Sudokura03.png" alt="Sudokura" width="520"></p>

A lightweight desktop Sudoku written in **C11 with SDL2 and SDL2_ttf**. Sudokura keeps its dark/light visual identity, notes, hints, verification, strict mode and three game modes while fitting every control into compact windows.

<p align="center"><img src="docs/images/sudokura-v1.1.0.png" alt="Sudokura v1.1.0 game interface" width="900"></p>

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

The Linux AppImage and Windows ZIP have been tested manually. Both macOS packages are structurally and dependency validated by CI but remain unsigned and not notarized.

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

The tag/manual packaging workflow builds consistently named v1.1.0 artifacts and SHA-256 files, prints their sizes, and uploads diagnostic reports to Actions. A tag run creates only a **draft** release for manual verification.

- **Linux:** executable x86_64 AppImage with the derived application icon and desktop entry.
- **Windows:** portable x86_64 ZIP with an embedded icon, SDL runtime, recursively collected transitive non-system DLLs, DejaVu Sans, and its license. CI fails on missing direct imports and smoke-tests both the package directory and the extracted ZIP with a clean dependency path.
- **macOS:** separate x86_64 and arm64 unsigned ZIPs containing a real `Sudokura.app` (`MacOS`, `Frameworks`, `Resources`, `Info.plist`). SDL libraries, recursively collected non-system dependencies, DejaVu Sans, and its license are bundled; install names use `@rpath`, and CI rejects Homebrew or runner-local paths.

On macOS, unzip and drag `Sudokura.app` to Applications. Because the artifacts are unsigned, use Finder's **Open** context action if Gatekeeper asks for confirmation.

## Visual and technical history

The current screenshot is stored at [`docs/images/sudokura-v1.1.0.png`](docs/images/sudokura-v1.1.0.png). The original v1 interface remains preserved at [`docs/images/sudokura-v1.png`](docs/images/sudokura-v1.png); it is historical documentation and is no longer the current UI.

See the [v1.1.0 release notes](docs/RELEASE_NOTES_1.1.0.md), the [technical implementation record](docs/V1.1.0_IMPLEMENTATION.md), and the [documentation image archive](docs/images/README.md).

## Asset policy

`assets/branding/source/` is immutable artwork. `scripts/generate_assets.py` removes only alpha values at or below 8, crops visible content, centers it with transparent padding, and losslessly writes PNG sizes 16–1024, a multi-size ICO, ICNS, and embedded 128×128 RGBA data for `SDL_SetWindowIcon`. Runtime packages contain only the required derivatives, never all five originals.

## Project scope

Android and iOS remain a later feasibility phase; v1.1.0 contains no mobile projects.

GPLv3 — © 2025–2026 [santirodriguez](https://santiagorodriguez.com)
