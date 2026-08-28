# Sudokura v1.2.0

Sudokura v1.2.0 is a substantial gameplay, persistence, branding, audio, and interface update while keeping the project lightweight and offline: native C11 with SDL2, SDL2_ttf, and SDL2_mixer, with no account or network-backed puzzle service.

The canonical v1.2 screenshot will be added only after the final release-candidate package has been installed and manually tested.

## What’s new

- **Easy, Medium, and Hard** deterministic puzzle generation with a unique solution.
- **Generator revision 2**, using deterministic randomized backtracking to build complete grids from an empty board before uniqueness-preserving clue removal. The generator accepts `2^64` seed values per difficulty; that describes the seed input space, not a guarantee that every seed produces a globally unique final puzzle.
- **Daily Puzzle**, deterministically generated from the local calendar date as Classic · Medium.
- **Classic/Daily hidden-solution neutrality:** normal entries do not expose whether they match the stored solution. Strikes and Time Attack retain the explicit correctness feedback their rules require.
- **Autosave and Continue** for the complete active session, including board state, notes, hints, mistakes, timer, mode, difficulty, and relevant preferences.
- **Restart and Retry** preserve the exact current puzzle.
- A redesigned **Home** with mode and difficulty selectors plus New Game, Continue, and Daily Puzzle actions.
- A reorganized in-game **HUD**, graphical progress bar, completed-number feedback, and distinct hint styling.
- **Responsive XL desktop layout:** maximized 1920×1080 and larger windows grow the board, sidebar, typography, and surrounding screens instead of retaining the former 720-pixel board cap. Portrait layouts remain separately tuned.
- **Real pause** behavior with safe focus-loss handling and correct timer accounting. Music also pauses while the game is manually paused or focus-paused.
- **Optional offline audio** using SDL2_mixer: background music, result jingles, and subtle programmatically generated UI/input effects. `V` toggles sound and the preference persists.
- A refreshed **About** screen with concise project context, generator/stack information, music credit, and direct GitHub/repository/website links.
- A richer result summary and a small SDL-rendered victory effect.
- Visible offline language selection for **English, Español, and Català** using bundled USA, Argentina, and Senyera flag assets.
- Updated `sudokura-head` branding and reproducibly generated application icons for Linux, Windows, macOS, and SDL.
- Versioned, CRC-protected session persistence with semantic validation, explicit incompatible-generator handling, and corruption quarantine.
- Expanded geometry, persistence, seed, audio, text-fit, sanitizer, smoke-test, and package validation.

## Audio credit

The four bundled OGG tracks come from **Cozy Puzzle Jingle & Result** by **MintoDog**, published on OpenGameArt under **CC0**. The original audio payloads are shipped without recompression. Provenance and filename mapping are recorded in [`assets/audio/README.md`](../assets/audio/README.md).

Short button and input sounds are synthesized at runtime and do not add further media assets.

## Downloads

The final v1.2.0 release is prepared to contain:

- `Sudokura-v1.2.0-linux-x86_64.AppImage`
- `Sudokura-v1.2.0-windows-x86_64.zip`
- `Sudokura-v1.2.0-macos-x86_64-unsigned.zip`
- `Sudokura-v1.2.0-macos-arm64-unsigned.zip`
- `SHA256SUMS.txt`

The macOS packages are unsigned. No signing or notarization is claimed.

## Validation policy

All four packages are built by the same repository automation used for release previews. Automated validation covers deterministic generation, core behavior, persistence, seed acquisition, SDL2_mixer asset/effect handling, responsive geometry including the XL tier, translated Help/About text fitting, package structure, dependency closure, checksums, and smoke tests where the hosted runner supports them.

Diagnostic UI review produces **60 non-canonical frames** across ten viewports and six screens: Home, Play, Help, About, Result, and Pause. These frames are validation artifacts only and are never substituted for the final user-supplied release screenshot.

Manual release-candidate testing and the final public screenshot remain separate gates before publication. Platform-specific manual-testing claims will be added only after those tests actually occur.
