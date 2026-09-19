# Sudokura v1.2.0

<p align="center"><img src="assets/branding/source/sudokura-head.png" alt="Sudokura" width="520"></p>

A lightweight desktop Sudoku written in **C11 with SDL2**. Sudokura offers three game modes, three difficulty levels, Daily Puzzle, autosave, notes, hints, themes, and optional adaptive audio in a native interface for Linux, Windows, and macOS.

<p align="center">
  <a href="docs/images/sudokura-v1.2.0.png">
    <img src="docs/images/sudokura-v1.2.0.png" alt="Sudokura v1.2.0 gameplay" width="900">
  </a>
</p>

## Features

- **Classic, Strikes, and Time Attack** game modes.
- **Easy, Medium, and Hard** puzzles with a unique solution.
- Deterministic generator revision 2 with a 64-bit seed space.
- **Daily Puzzle**, plus exact-puzzle Restart and Retry.
- **Continue and autosave** for the current board, notes, hints, timer, and game state.
- **Pause**, focus-safe timing, Notes, Hint, Verify, and Strict/Free input.
- **Dark and light themes** with responsive desktop and portrait layouts.
- **English, Español, and Català**.
- Optional background music, result jingles, and subtle interface feedback, with independent Music/FX levels and global mute via `V`.
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

Published builds are distributed through [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases). The currently published v1.2.0 release remains available, including its historical Intel and Apple Silicon macOS ZIPs.

The v1.3.0 candidate narrows and makes platform support explicit:

- Windows 11 x64: portable ZIP plus per-user installer.
- Ubuntu 22.04 and 24.04 x86_64: AppImage.
- Fedora 44 x86_64: AppImage compatibility target; exact desktop acceptance is recorded separately.
- macOS 15+ on Apple Silicon/arm64: DMG containing `Sudokura.app`, **experimental until Phase-8 real-device/Gatekeeper acceptance**.

No new v1.3.0 Intel Mac, macOS-before-15, Windows 32-bit, or ARM Linux/Windows package is generated. Systems outside the new candidate scope should use an appropriate older published release; that does not imply new maintenance for v1.2.

The v1.3.0 macOS candidate uses an ad-hoc integrity signature only. It is **not** Developer ID signed or notarized, and the automated headless package smoke is not a substitute for opening a downloaded DMG on a real Mac.

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

For diagnostic UI review, `./sudokura --render-screenshots DIR` produces the Phase 5 temporary state/language/theme matrix plus a semantic manifest. These are test artifacts and are not used as release screenshots.

## Audio credits

Music: **Cozy Puzzle Jingle & Result** by **MintoDog**, from [OpenGameArt](https://opengameart.org/content/cozy-puzzle-jingle-result), licensed under **CC0**. See [`assets/audio/README.md`](assets/audio/README.md) for the file mapping.

Interface and input effects are generated at runtime.

## Documentation

- [v1.2.0 release notes](docs/RELEASE_NOTES_1.2.0.md)
- [v1.2.0 implementation record](docs/V1.2.0_IMPLEMENTATION.md)
- [documentation images](docs/images/README.md)

## Support Sudokura

If you enjoy Sudokura and want to support its development, [support Sudokura here](https://santiagorodriguez.com/donate/).

GPLv3 — © 2025–2026 [Santiago Rodriguez](https://santiagorodriguez.com/)
