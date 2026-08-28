# Sudokura v1.2.0

<p align="center"><img src="assets/branding/source/sudokura-head.png" alt="Sudokura" width="520"></p>

A lightweight desktop Sudoku written in **C11 with SDL2**. Sudokura offers three game modes, three difficulty levels, Daily Puzzle, autosave, notes, hints, themes, and optional adaptive audio in a native interface for Linux, Windows, and macOS.

<!-- The canonical v1.2.0 screenshot supplied after final RC testing will be inserted here. -->

## Features

- **Classic, Strikes, and Time Attack** game modes.
- **Easy, Medium, and Hard** puzzles with a unique solution.
- Deterministic generator revision 2 with a 64-bit seed space.
- **Daily Puzzle**, plus exact-puzzle Restart and Retry.
- **Continue and autosave** for the current board, notes, hints, timer, and game state.
- **Pause**, focus-safe timing, Notes, Hint, Verify, and Strict/Free input.
- **Dark and light themes** with responsive desktop and portrait layouts.
- **English, Español, and Català**.
- Optional background music, result jingles, and subtle interface feedback. Sound can be toggled with `V`.
- Mouse, keyboard, and physical numeric-keypad controls.

## Controls

| Action | Control |
|---|---|
| Select a cell | Mouse, arrows, or WASD |
| Place / clear a number | 1–9 or numeric keypad; 0, Backspace, or Delete clears |
| Notes | N, Shift+1–9, right-click, or a cell sub-position |
| Hint | H |
| Strict / Free | M |
| Pause | P or the Pause button |
| Theme / language / sound | T / L / V |
| Continue / Daily from Home | C / D |
| Help / About / back | F1 / F2 / Escape |

## Downloads

Published builds are distributed through [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases). The v1.2.0 package names are:

- `Sudokura-v1.2.0-linux-x86_64.AppImage`
- `Sudokura-v1.2.0-windows-x86_64.zip`
- `Sudokura-v1.2.0-macos-x86_64-unsigned.zip`
- `Sudokura-v1.2.0-macos-arm64-unsigned.zip`
- `SHA256SUMS.txt`

The macOS packages are unsigned.

## Build and test

Install a C compiler, `pkg-config`, SDL2, SDL2_ttf, SDL2_mixer, Python 3, and Go.

```sh
make assets
make
make test
make test-ui
./sudokura
```

`make test` covers gameplay, deterministic generation, persistence, localization, seed handling, geometry, and dedicated UI geometry invariants. `make test-ui` checks SDL_ttf text fitting, SDL2_mixer audio transitions, and top-row/numeric-keypad input mapping. CI also builds with warnings as errors and runs sanitizers on Linux.

For diagnostic UI review, `./sudokura --render-screenshots DIR` produces 60 temporary frames across supported layouts. These are test artifacts and are not used as release screenshots.

## Audio credits

Music: **Cozy Puzzle Jingle & Result** by **MintoDog**, from [OpenGameArt](https://opengameart.org/content/cozy-puzzle-jingle-result), licensed under **CC0**. The bundled OGG files retain their original audio data. See [`assets/audio/README.md`](assets/audio/README.md) for provenance.

Interface and input effects are generated at runtime.

## Documentation

- [v1.2.0 release notes](docs/RELEASE_NOTES_1.2.0.md)
- [v1.2.0 implementation record](docs/V1.2.0_IMPLEMENTATION.md)
- [documentation images](docs/images/README.md)

GPLv3 — © 2025–2026 [santirodriguez](https://santiagorodriguez.com)
